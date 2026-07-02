/**
 * new_ops.cpp
 * New optimized operations: registry scanner, driver scanner,
 * process tree analyzer. Uses thread pool for parallel processing.
 */

#include "../include/scanners.h"
#include "../include/logger.h"
#include "../include/ui.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <set>
#include <map>
#include <future>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <tlhelp32.h>
  #include <psapi.h>
  #pragma comment(lib, "psapi.lib")
#endif

// ─────────────────────────────────────────────
//  RegistryScanner
// ─────────────────────────────────────────────
RegistryScanner::RegistryScanner(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

RegistryScanResult RegistryScanner::scan(bool detectOnly,
    std::function<void(int, const std::string&)> progress) {
    RegistryScanResult result;
    Logger::instance().info("Registry scan started (detect-only: " +
                            std::string(detectOnly ? "yes" : "no") + ")", "registry");

    if (platform_.id == OS::Windows) {
        // Scan for orphaned uninstall entries (referencing non-existent paths)
        if (progress) progress(20, "Checking uninstall entries...");
        {
            auto r = Platform::exec(
                "powershell -NoProfile -Command \""
                "Get-ItemProperty HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\* "
                "| Where-Object { $_.InstallLocation -and -not (Test-Path $_.InstallLocation) } "
                "| Select-Object @{N='App';E={$_.DisplayName}}, InstallLocation "
                "| Format-Table -HideTableHeader -AutoSize\"", 30);
            std::istringstream iss(r.stdOut);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.empty() || line.size() < 5) continue;
                RegistryIssue ri;
                ri.path = "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
                ri.key = line;
                ri.issue = "orphaned";
                ri.details = "InstallLocation does not exist";
                result.issues.push_back(ri);
                ++result.orphanedKeys;
            }
        }

        // Scan for broken COM references
        if (progress) progress(50, "Checking COM references...");
        {
            auto r = Platform::exec(
                "powershell -NoProfile -Command \""
                "Get-ChildItem HKLM:\\SOFTWARE\\Classes\\CLSID -ErrorAction SilentlyContinue "
                "| ForEach-Object { "
                "  $path = $_.PSPath + '\\\\InprocServer32'; "
                "  $val = Try { Get-ItemProperty -Path $path -ErrorAction Stop } Catch { $null }; "
                "  if ($val -and $val.'(default)' -and -not (Test-Path $val.'(default)')) { "
                "    Write-Output ($_.PSChildName + ' -> ' + $val.'(default)') "
                "  } "
                "}\"", 45);
            std::istringstream iss(r.stdOut);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.empty()) continue;
                RegistryIssue ri;
                ri.path = "HKLM\\Software\\Classes\\CLSID";
                ri.issue = "broken";
                ri.key = line;
                ri.details = "Referenced COM DLL not found";
                result.issues.push_back(ri);
                ++result.brokenReferences;
            }
        }

        // Scan for invalid shell extensions
        if (progress) progress(75, "Checking shell extensions...");
        {
            auto r = Platform::exec(
                "powershell -NoProfile -Command \""
                "Get-ChildItem 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved' -ErrorAction SilentlyContinue "
                "| ForEach-Object { "
                "  $guid = $_.PSChildName; "
                "  $clsPath = 'HKLM:\\SOFTWARE\\Classes\\CLSID\\' + $guid; "
                "  if (-not (Test-Path $clsPath)) { "
                "    Write-Output $guid "
                "  } "
                "}\"", 30);
            std::istringstream iss(r.stdOut);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.empty()) continue;
                RegistryIssue ri;
                ri.path = "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
                ri.key = line;
                ri.issue = "invalid";
                ri.details = "CLSID reference has no matching class registration";
                result.issues.push_back(ri);
                ++result.invalidValues;
            }
        }
    }

    result.totalIssues = static_cast<int>(result.issues.size());

    if (progress) progress(100, "Registry scan complete");
    Logger::instance().info("Registry scan: " + std::to_string(result.totalIssues) +
                            " issues found", "registry");
    return result;
}

// ─────────────────────────────────────────────
//  DriverScanner
// ─────────────────────────────────────────────
DriverScanner::DriverScanner(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

DriverScanResult DriverScanner::scan(std::function<void(int, const std::string&)> progress) {
    DriverScanResult result;
    Logger::instance().info("Driver scan started", "drivers");

    if (platform_.id == OS::Windows) {
        if (progress) progress(10, "Querying all drivers...");

        // ONE PowerShell call: enumerate drivers pipe-delimited, then batch
        // the signature check over all unique paths in the same process
        // (previously this spawned two powershell.exe per driver).
        auto r = Platform::exec(
            "powershell -NoProfile -Command \""
            "$ds=Get-CimInstance Win32_SystemDriver -ErrorAction SilentlyContinue;"
            "foreach($d in $ds){ 'D='+$d.Name+'|'+$d.DisplayName+'|'+$d.State+'|'+$d.StartMode+'|'+$d.PathName };"
            "$paths=$ds | ForEach-Object {$_.PathName} | Where-Object {$_ -and (Test-Path $_)} | Select-Object -Unique;"
            "if($paths){ Get-AuthenticodeSignature -FilePath $paths -ErrorAction SilentlyContinue |"
            " Where-Object {$_.Status -ne 'Valid'} | ForEach-Object { 'U='+$_.Path } }\"", 120);

        std::set<std::string> unsignedPaths;
        std::vector<std::array<std::string, 5>> rows;
        {
            std::istringstream iss(r.stdOut);
            std::string line;
            auto splitPipe = [](const std::string& s) {
                std::vector<std::string> v; std::istringstream is2(s);
                std::string t; while (std::getline(is2, t, '|')) v.push_back(t);
                return v;
            };
            while (std::getline(iss, line)) {
                if (line.rfind("U=", 0) == 0) {
                    std::string p = line.substr(2);
                    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
                    unsignedPaths.insert(p);
                } else if (line.rfind("D=", 0) == 0) {
                    auto p = splitPipe(line.substr(2));
                    std::array<std::string, 5> row{};
                    for (size_t i = 0; i < p.size() && i < 5; ++i) row[i] = p[i];
                    if (!row[0].empty()) rows.push_back(row);
                }
            }
        }

        if (progress) progress(70, "Classifying drivers...");
        static const std::set<std::string> criticalDrivers = {
            "SamSs", "LanmanServer", "lanmanserver", "Tcpip", "Dhcp", "Dnscache",
            "NlaSvc", "EventLog", "PlugPlay", "Power", "W32Time", "RpcSs", "DcomLaunch"
        };

        for (auto& row : rows) {
            DriverInfo di;
            di.name        = row[0];
            di.displayName = row[1];
            di.state       = row[2];
            di.version     = row[3]; // StartMode shown where version is unknown

            std::string pathLower = row[4];
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
            di.signed_ = pathLower.empty() || !unsignedPaths.count(pathLower);
            if (!di.signed_) {
                result.unsignedDrivers.push_back(di);
                ++result.unsignedCount;
            }

            if (di.state == "Stopped" && criticalDrivers.count(di.name)) {
                result.problematicDrivers.push_back(di);
                ++result.stoppedCritical;
            }
            result.drivers.push_back(di);
            ++result.totalCount;
        }
    }

    if (progress) progress(100, "Driver scan complete");
    Logger::instance().info("Driver scan: " + std::to_string(result.totalCount) +
                            " total, " + std::to_string(result.unsignedCount) +
                            " unsigned", "drivers");
    return result;
}

// ─────────────────────────────────────────────
//  ProcessTreeAnalyzer
// ─────────────────────────────────────────────
ProcessTreeAnalyzer::ProcessTreeAnalyzer(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

std::vector<ProcessTreeNode> ProcessTreeAnalyzer::buildTree(
    std::function<void(int, const std::string&)> progress) {
    Logger::instance().info("Building process tree", "proctree");
    std::map<int, ProcessTreeNode> nodeMap;
    std::set<int> allPids;

    if (platform_.id == OS::Windows) {
        // Pipe-delimited output survives process names containing spaces.
        auto r = Platform::exec(
            "powershell -NoProfile -Command \""
            "Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | ForEach-Object {"
            " 'P='+$_.Name+'|'+$_.ProcessId+'|'+$_.ParentProcessId+'|'+"
            "($_.KernelModeTime+$_.UserModeTime)+'|'+$_.WorkingSetSize+'|'+$_.ThreadCount+'|'+$_.HandleCount }\"", 30);

        std::istringstream iss(r.stdOut);
        std::string line;
        while (std::getline(iss, line)) {
            auto start = line.find("P=");
            if (start == std::string::npos) continue;
            std::istringstream ls(line.substr(start + 2));
            std::vector<std::string> p;
            std::string tok;
            while (std::getline(ls, tok, '|')) p.push_back(tok);
            if (p.size() < 3) continue;

            ProcessTreeNode node;
            node.name = p[0];
            try { node.pid = std::stoi(p[1]); } catch (...) {}
            try { node.ppid = std::stoi(p[2]); } catch (...) {}
            if (p.size() > 3) { try { node.cpuPercent = std::stod(p[3]) / 10000000.0; } catch (...) {} }
            if (p.size() > 4) { try { node.memoryBytes = std::stoull(p[4]); } catch (...) {} }
            if (p.size() > 5) { try { node.threadCount = std::stoi(p[5]); } catch (...) {} }
            if (p.size() > 6) { try { node.handleCount = std::stoi(p[6]); } catch (...) {} }

            nodeMap[node.pid] = node;
            allPids.insert(node.pid);
        }
    }

    // Build tree structure (parent -> children)
    std::vector<ProcessTreeNode> forest;
    if (progress) progress(80, "Building tree hierarchy...");

    for (auto& [pid, node] : nodeMap) {
        if (node.ppid > 0 && nodeMap.count(node.ppid)) {
            nodeMap[node.ppid].children.push_back(node);
        } else {
            // Root node (no parent or parent not found)
            forest.push_back(node);
        }
    }

    if (progress) progress(100, "Process tree built");
    Logger::instance().info("Process tree: " + std::to_string(forest.size()) +
                            " root processes", "proctree");
    return forest;
}

void ProcessTreeAnalyzer::printTree(const std::vector<ProcessTreeNode>& forest, int depth) {
    std::string indent(depth * 2, ' ');
    for (const auto& node : forest) {
        auto memStr = Platform::formatBytes(node.memoryBytes);
        UI::printDim(indent + "├─ " + node.name + " (PID:" + std::to_string(node.pid) +
                     " CPU:" + std::to_string((int)node.cpuPercent) + "% MEM:" + memStr +
                     " Thr:" + std::to_string(node.threadCount) + ")");
        if (!node.children.empty()) {
            printTree(node.children, depth + 1);
        }
    }
}
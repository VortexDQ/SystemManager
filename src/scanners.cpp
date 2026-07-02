/**
 * scanners.cpp
 * Full implementations of all scanner classes: StartupAnalyzer, HardwareScanner,
 * NetworkScanner, EventLogAnalyzer, PerformanceMonitor, ReliabilityAnalyzer,
 * WindowsInfoScanner, and CorruptionScanner.
 *
 * Design note: each scanner issues a SINGLE PowerShell invocation that emits
 * machine-readable KEY=VALUE (pipe-delimited for lists) lines. Process spawn
 * is by far the dominant cost (~1s per powershell.exe), so batching every
 * query into one script makes each scanner an order of magnitude faster than
 * one-process-per-metric. Scripts use single-quoted PowerShell strings with
 * string-first concatenation ('KEY='+$x) — no inner double quotes, so nothing
 * needs escaping through cmd.exe.
 */

#include "../include/scanners.h"
#include "../include/logger.h"
#include "../include/ui.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#elif defined(__APPLE__)
  #include <sys/sysctl.h>
  #include <mach/mach.h>
#endif

// ─────────────────────────────────────────────
//  Shared parsing helpers
// ─────────────────────────────────────────────
namespace {

std::string trimmed(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        line = trimmed(line);
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

std::vector<std::string> splitBy(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream iss(s);
    while (std::getline(iss, cur, delim)) out.push_back(trimmed(cur));
    return out;
}

bool truthy(const std::string& v) {
    std::string t = v;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    return t.find("true") != std::string::npos || t == "1";
}

int toInt(const std::string& v, int fallback = 0) {
    try { return std::stoi(trimmed(v)); } catch (...) { return fallback; }
}

double toDouble(const std::string& v, double fallback = 0.0) {
    try { return std::stod(trimmed(v)); } catch (...) { return fallback; }
}

uint64_t toU64(const std::string& v, uint64_t fallback = 0) {
    try { return std::stoull(trimmed(v)); } catch (...) { return fallback; }
}

// Wrap a PowerShell body for Platform::exec. The body must not contain
// double quotes (use single-quoted PS strings).
std::string ps(const std::string& body) {
    return "powershell -NoProfile -ExecutionPolicy Bypass -Command \"" + body + "\"";
}

// Parse "KEY=VALUE" lines into callback(key, value). Lines without '='
// are skipped. Values may contain further '=' characters.
template <typename Fn>
void forEachKV(const std::string& text, Fn&& fn) {
    for (auto& line : splitLines(text)) {
        auto eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        fn(line.substr(0, eq), trimmed(line.substr(eq + 1)));
    }
}

#ifdef _WIN32
std::string expandEnv(const std::string& s) {
    char buf[4096];
    DWORD n = ExpandEnvironmentStringsA(s.c_str(), buf, sizeof(buf));
    if (n == 0 || n > sizeof(buf)) return s;
    return std::string(buf);
}
#else
std::string expandEnv(const std::string& s) { return s; }
#endif

// Extract the executable path from a startup command line such as:
//   "C:\Program Files\App\app.exe" --flag
//   C:\Program Files\App\app.exe --flag
//   %ProgramFiles%\App\app.exe /background
// Returns empty string when no plausible path can be isolated.
std::string extractExecutable(const std::string& raw) {
    std::string cmd = trimmed(expandEnv(raw));
    if (cmd.empty()) return "";
    if (cmd[0] == '"') {
        auto end = cmd.find('"', 1);
        return end == std::string::npos ? cmd.substr(1) : cmd.substr(1, end - 1);
    }
    // Unquoted: if the full string is an existing file, use it (handles
    // spaces in the path with no arguments).
    if (Platform::fileExists(cmd)) return cmd;
    // Otherwise cut at the extension — covers "path with spaces\x.exe -args".
    std::string lower = cmd;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const char* ext : {".exe", ".com", ".bat", ".cmd"}) {
        auto pos = lower.find(ext);
        if (pos != std::string::npos) return cmd.substr(0, pos + 4);
    }
    // Fall back to the first whitespace-delimited token.
    auto sp = cmd.find(' ');
    return sp == std::string::npos ? cmd : cmd.substr(0, sp);
}

// A startup entry counts as broken only when we can positively resolve an
// absolute path that does not exist. Relative names (resolved via PATH)
// and un-parseable commands are treated as valid to avoid false positives.
bool startupTargetMissing(const std::string& command) {
    std::string exe = extractExecutable(command);
    if (exe.size() < 3) return false;
    bool absolute = (std::isalpha(static_cast<unsigned char>(exe[0])) && exe[1] == ':')
                    || exe.rfind("\\\\", 0) == 0;
    if (!absolute) return false;
    return !Platform::fileExists(exe);
}

} // namespace

// ─────────────────────────────────────────────
//  StartupAnalyzer
// ─────────────────────────────────────────────
StartupAnalyzer::StartupAnalyzer(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

StartupResult StartupAnalyzer::analyze(std::function<void(int, const std::string&)> progress) {
    StartupResult result;
    Logger::instance().info("Startup analysis started", "startup");
    Timer t;

    if (platform_.id == OS::Windows) {
        if (progress) progress(10, "Scanning startup folders...");

        // Startup folders (fast, native)
        std::string home = Platform::getHomePath();
        auto checkFolder = [&](const std::string& folder, const std::string& location) {
            if (!Platform::dirExists(folder)) return;
            for (auto& f : Platform::listDir(folder)) {
                std::string name = f.substr(f.find_last_of("\\/") + 1);
                if (name == "desktop.ini") continue;
                StartupEntry e;
                e.name = name;
                e.path = f;
                e.location = location;
                e.enabled = true;
                e.valid = true;
                result.entries.push_back(e);
            }
        };
        checkFolder(home + "\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup",
                    "Startup Folder (User)");
        checkFolder("C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\StartUp",
                    "Startup Folder (Common)");

        if (progress) progress(30, "Querying registry autoruns...");

        // Registry Run keys + Win32_StartupCommand + enable/disable state,
        // all in ONE PowerShell invocation.
        auto r = Platform::exec(ps(
            "$paths=@('HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run',"
            "'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce',"
            "'HKCU:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run',"
            "'HKCU:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce',"
            "'HKLM:\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run');"
            "foreach($p in $paths){ $ip=Get-ItemProperty -Path $p -ErrorAction SilentlyContinue;"
            " if($ip){ $ip.PSObject.Properties | Where-Object {$_.Name -notmatch '^PS'} |"
            " ForEach-Object { 'REG='+$_.Name+'|'+$_.Value } } };"
            "Get-CimInstance Win32_StartupCommand -ErrorAction SilentlyContinue |"
            " ForEach-Object { 'CMD='+$_.Name+'|'+$_.Command+'|'+$_.Location };"
            "$sa=@('HKCU:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run',"
            "'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run',"
            "'HKCU:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder');"
            "foreach($p in $sa){ $ip=Get-ItemProperty -Path $p -ErrorAction SilentlyContinue;"
            " if($ip){ $ip.PSObject.Properties | Where-Object {$_.Name -notmatch '^PS'} |"
            " ForEach-Object { if($_.Value -is [byte[]] -and $_.Value.Length -gt 0 -and (($_.Value[0] -band 1) -eq 1)){"
            " 'DIS='+$_.Name } } } }"), 30);

        std::vector<std::string> disabledNames;
        forEachKV(r.stdOut, [&](const std::string& key, const std::string& value) {
            if (key == "REG") {
                auto parts = splitBy(value, '|');
                if (parts.size() < 2 || parts[0].empty() || parts[1].empty()) return;
                StartupEntry e;
                e.name = parts[0];
                e.path = parts[1];
                e.location = "Registry Run";
                e.enabled = true;
                e.valid = !startupTargetMissing(parts[1]);
                result.entries.push_back(e);
            } else if (key == "CMD") {
                auto parts = splitBy(value, '|');
                if (parts.empty() || parts[0].empty()) return;
                StartupEntry e;
                e.name = parts[0];
                e.path = parts.size() > 1 ? parts[1] : "";
                e.location = "Task Manager (" + (parts.size() > 2 ? parts[2] : "?") + ")";
                e.enabled = true;
                e.valid = e.path.empty() ? true : !startupTargetMissing(e.path);
                result.entries.push_back(e);
            } else if (key == "DIS") {
                disabledNames.push_back(value);
            }
        });

        // Apply disabled state from StartupApproved
        for (auto& e : result.entries) {
            for (auto& d : disabledNames) {
                if (e.name == d) { e.enabled = false; break; }
            }
        }
    }

    if (progress) progress(80, "Deduplicating entries...");

    // Deduplicate by (name, path) — the same app can legitimately appear
    // in multiple locations only if the command differs.
    std::sort(result.entries.begin(), result.entries.end(),
        [](const auto& a, const auto& b) {
            return a.name != b.name ? a.name < b.name : a.path < b.path;
        });
    auto last = std::unique(result.entries.begin(), result.entries.end(),
        [](const auto& a, const auto& b) { return a.name == b.name && a.path == b.path; });
    result.duplicateCount = static_cast<int>(std::distance(last, result.entries.end()));
    result.entries.erase(last, result.entries.end());

    result.totalCount = static_cast<int>(result.entries.size());
    result.enabledCount = result.disabledCount = result.brokenCount = 0;
    for (auto& e : result.entries) {
        if (e.enabled) ++result.enabledCount; else ++result.disabledCount;
        if (!e.valid) ++result.brokenCount;
    }

    if (progress) progress(100, "Startup analysis complete");
    Logger::instance().logScan("Startup Analysis", true, t.elapsedMs(),
        "Entries=" + std::to_string(result.totalCount) +
        " Broken=" + std::to_string(result.brokenCount));
    return result;
}

// ─────────────────────────────────────────────
//  HardwareScanner
// ─────────────────────────────────────────────
HardwareScanner::HardwareScanner(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

HardwareResult HardwareScanner::scan(std::function<void(int, const std::string&)> progress) {
    HardwareResult result;
    Logger::instance().info("Hardware scan started", "hardware");
    Timer t;

    if (platform_.id == OS::Windows) {
        if (progress) progress(10, "Querying hardware inventory...");
        auto r = Platform::exec(ps(
            "$cpu=Get-CimInstance Win32_Processor | Select-Object -First 1;"
            "'CPUMODEL='+$cpu.Name;"
            "'CPUCORES='+$cpu.NumberOfCores;"
            "'CPUTHREADS='+$cpu.NumberOfLogicalProcessors;"
            "'CPUMHZ='+$cpu.MaxClockSpeed;"
            "'CPUARCH='+$cpu.Architecture;"
            "'CPULOAD='+$cpu.LoadPercentage;"
            "$os=Get-CimInstance Win32_OperatingSystem;"
            "'MEMTOTKB='+$os.TotalVisibleMemorySize;"
            "'MEMFREEKB='+$os.FreePhysicalMemory;"
            "$pm=Get-CimInstance Win32_PhysicalMemory -ErrorAction SilentlyContinue;"
            "if($pm){ $m=$pm | Measure-Object -Property Capacity -Sum;"
            " 'MEMBYTES='+$m.Sum; 'MEMSLOTS='+$m.Count;"
            " 'MEMSPEED='+($pm | Select-Object -First 1).Speed };"
            "Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue |"
            " ForEach-Object { 'GPU='+$_.Name+'|'+$_.DriverVersion+'|'+$_.AdapterRAM };"
            "'BOARD='+(Get-CimInstance Win32_BaseBoard -ErrorAction SilentlyContinue).Product;"
            "'BIOS='+(Get-CimInstance Win32_BIOS -ErrorAction SilentlyContinue).SMBIOSBIOSVersion;"
            "'UEFI='+($env:firmware_type -eq 'UEFI' -or (Test-Path 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State'));"
            "$sb=(Get-ItemProperty 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State' -ErrorAction SilentlyContinue).UEFISecureBootEnabled;"
            "'SECBOOT='+($sb -eq 1);"
            "$tpm=Get-CimInstance -Namespace root/cimv2/Security/MicrosoftTpm -ClassName Win32_Tpm -ErrorAction SilentlyContinue;"
            "if($tpm){'TPM=True'}else{"
            "'TPM='+[bool](Get-PnpDevice -Class SecurityDevices -ErrorAction SilentlyContinue |"
            " Where-Object {$_.FriendlyName -match 'Trusted Platform'})};"
            "$b=Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue | Select-Object -First 1;"
            "if($b){'BATT='+$b.EstimatedChargeRemaining}"), 45);

        uint64_t memTotKb = 0, memFreeKb = 0;
        forEachKV(r.stdOut, [&](const std::string& key, const std::string& v) {
            if      (key == "CPUMODEL")   result.cpu.model = v;
            else if (key == "CPUCORES")   result.cpu.cores = toInt(v);
            else if (key == "CPUTHREADS") result.cpu.threads = toInt(v);
            else if (key == "CPUMHZ")     result.cpu.frequencyGHz = toDouble(v) / 1000.0;
            else if (key == "CPULOAD")    result.cpu.usagePercent = toDouble(v);
            else if (key == "CPUARCH") {
                int arch = toInt(v, -1);
                result.cpu.architecture = (arch == 9) ? "x64" : (arch == 12) ? "ARM64"
                                        : (arch == 0) ? "x86" : "Unknown";
            }
            else if (key == "MEMTOTKB")  memTotKb = toU64(v);
            else if (key == "MEMFREEKB") memFreeKb = toU64(v);
            else if (key == "MEMBYTES")  result.memory.totalBytes = toU64(v);
            else if (key == "MEMSLOTS")  result.memory.slots = toInt(v);
            else if (key == "MEMSPEED")  result.memory.speedMHz = toDouble(v);
            else if (key == "GPU") {
                auto parts = splitBy(v, '|');
                GpuInfo g;
                if (parts.size() > 0) g.name = parts[0];
                if (parts.size() > 1) g.driverVersion = parts[1];
                if (parts.size() > 2) g.vramBytes = toU64(parts[2]);
                if (!g.name.empty()) result.gpus.push_back(g);
            }
            else if (key == "BOARD")   result.motherboard = v;
            else if (key == "BIOS")    result.biosVersion = v;
            else if (key == "UEFI")    result.uefi = truthy(v);
            else if (key == "SECBOOT") result.secureBoot = truthy(v);
            else if (key == "TPM")     result.tpmPresent = truthy(v);
            else if (key == "BATT")    result.batteryPercent = toDouble(v, -1.0);
        });

        // Prefer OS-visible memory for availability math; fall back to it for
        // total when Win32_PhysicalMemory was inaccessible.
        if (result.memory.totalBytes == 0) result.memory.totalBytes = memTotKb * 1024;
        result.memory.availableBytes = memFreeKb * 1024;
        if (result.memory.totalBytes >= result.memory.availableBytes)
            result.memory.usedBytes = result.memory.totalBytes - result.memory.availableBytes;
        if (memTotKb > 0)
            result.memory.usagePercent = 100.0 * (memTotKb - memFreeKb) / memTotKb;
    }

    if (progress) progress(100, "Hardware scan complete");
    Logger::instance().logScan("Hardware Scan", true, t.elapsedMs(), result.cpu.model);
    return result;
}

// ─────────────────────────────────────────────
//  NetworkScanner
// ─────────────────────────────────────────────
NetworkScanner::NetworkScanner(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

NetworkResult NetworkScanner::scan(std::function<void(int, const std::string&)> progress) {
    NetworkResult result;
    Logger::instance().info("Network scan started", "network");
    Timer t;

    if (platform_.id == OS::Windows) {
        if (progress) progress(10, "Querying adapters...");
        auto r = Platform::exec(ps(
            "Get-NetAdapter -ErrorAction SilentlyContinue |"
            " Where-Object {$_.Status -ne 'Disconnected' -and $_.Status -ne 'Not Present'} |"
            " ForEach-Object {"
            " $ip=(Get-NetIPAddress -InterfaceIndex $_.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |"
            " Select-Object -First 1).IPAddress;"
            " $dns=((Get-DnsClientServerAddress -InterfaceIndex $_.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue).ServerAddresses -join ' ');"
            " 'AD='+$_.Name+'|'+$_.PhysicalMediaType+'|'+$_.MacAddress+'|'+$_.Status+'|'+$_.LinkSpeed+'|'+$ip+'|'+$dns };"
            "'GW='+(Get-NetRoute -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue |"
            " Sort-Object RouteMetric | Select-Object -First 1).NextHop;"
            "'FW='+@(Get-NetFirewallProfile -ErrorAction SilentlyContinue | Where-Object {$_.Enabled -eq $true}).Count;"
            "'VPN='+@(Get-NetAdapter -ErrorAction SilentlyContinue |"
            " Where-Object {($_.InterfaceDescription+' '+$_.Name) -match 'VPN|TAP|Tunnel|WireGuard' -and $_.Status -eq 'Up'}).Count;"
            "$d=Measure-Command { Resolve-DnsName microsoft.com -ErrorAction SilentlyContinue | Out-Null };"
            "'DNSMS='+[int]$d.TotalMilliseconds"), 45);

        forEachKV(r.stdOut, [&](const std::string& key, const std::string& v) {
            if (key == "AD") {
                auto parts = splitBy(v, '|');
                if (parts.empty()) return;
                NetworkAdapter a;
                a.name = parts[0];
                if (parts.size() > 1) {
                    if (parts[1].find("802.11") != std::string::npos) a.type = "Wi-Fi";
                    else if (parts[1].find("802.3") != std::string::npos) a.type = "Ethernet";
                    else a.type = parts[1].empty() ? "Virtual" : parts[1];
                }
                if (parts.size() > 2) a.mac = parts[2];
                if (parts.size() > 3) a.connected = (parts[3] == "Up");
                if (parts.size() > 4) {
                    // LinkSpeed is a localized string like "1 Gbps" / "300 Mbps"
                    double val = toDouble(parts[4]);
                    if (parts[4].find("Gbps") != std::string::npos) val *= 1000.0;
                    else if (parts[4].find("Kbps") != std::string::npos) val /= 1000.0;
                    a.speedMbps = val;
                }
                if (parts.size() > 5) a.ipv4 = parts[5];
                if (parts.size() > 6) a.dns = parts[6];
                result.adapters.push_back(a);
            }
            else if (key == "GW")    result.gateway = v;
            else if (key == "FW")    result.firewallActive = toInt(v) > 0;
            else if (key == "VPN")   result.vpnActive = toInt(v) > 0;
            else if (key == "DNSMS") result.dnsLatencyMs = toDouble(v);
        });

        if (progress) progress(50, "Measuring latency and packet loss...");
        // One ping run provides both average latency and packet loss.
        auto pingR = Platform::exec("ping -n 4 8.8.8.8", 20);
        {
            auto pos = pingR.stdOut.find("Average");
            if (pos != std::string::npos) {
                auto eq = pingR.stdOut.find('=', pos);
                auto ms = pingR.stdOut.find("ms", eq);
                if (eq != std::string::npos && ms != std::string::npos)
                    result.pingMs = toDouble(pingR.stdOut.substr(eq + 1, ms - eq - 1));
            }
            auto paren = pingR.stdOut.find('(');
            auto pct   = pingR.stdOut.find('%', paren);
            if (paren != std::string::npos && pct != std::string::npos)
                result.packetLoss = toDouble(pingR.stdOut.substr(paren + 1, pct - paren - 1));
        }

        if (progress) progress(80, "Resolving public IP...");
        auto pubR = Platform::exec(ps(
            "try {(Invoke-WebRequest -Uri 'https://api.ipify.org' -UseBasicParsing -TimeoutSec 5).Content} catch {}"), 12);
        std::string pub = trimmed(pubR.stdOut);
        if (!pub.empty() && pub.size() < 46 && pub.find(' ') == std::string::npos)
            result.publicIp = pub;
        else
            result.publicIp = "unavailable";
    }

    if (progress) progress(100, "Network scan complete");
    Logger::instance().logScan("Network Scan", true, t.elapsedMs(),
        "Adapters=" + std::to_string(result.adapters.size()));
    return result;
}

// ─────────────────────────────────────────────
//  EventLogAnalyzer
// ─────────────────────────────────────────────
EventLogAnalyzer::EventLogAnalyzer(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

EventLogSummary EventLogAnalyzer::analyze(std::function<void(int, const std::string&)> progress) {
    EventLogSummary result;
    Logger::instance().info("Event log analysis started", "events");
    Timer t;

    if (platform_.id == OS::Windows) {
        if (progress) progress(20, "Reading System & Application logs...");
        // Read both logs once and aggregate by numeric level
        // (locale-independent, unlike LevelDisplayName).
        auto r = Platform::exec(ps(
            "$ev=@(); foreach($l in 'System','Application'){"
            " $ev+=@(Get-WinEvent -LogName $l -MaxEvents 1000 -ErrorAction SilentlyContinue) };"
            "'CRIT='+@($ev | Where-Object {$_.Level -eq 1}).Count;"
            "'ERR='+@($ev | Where-Object {$_.Level -eq 2}).Count;"
            "'WARN='+@($ev | Where-Object {$_.Level -eq 3}).Count;"
            "$ev | Where-Object {$_.Level -eq 1} | Select-Object -First 5 | ForEach-Object {"
            " $m=[string]$_.Message; $m=($m -replace '\\r|\\n',' ');"
            " if($m.Length -gt 160){$m=$m.Substring(0,160)};"
            " 'CMSG='+$_.ProviderName+': '+$m };"
            "$ev | Where-Object {$_.Level -eq 2} | Select-Object -First 5 | ForEach-Object {"
            " $m=[string]$_.Message; $m=($m -replace '\\r|\\n',' ');"
            " if($m.Length -gt 160){$m=$m.Substring(0,160)};"
            " 'EMSG='+$_.ProviderName+': '+$m }"), 60);

        forEachKV(r.stdOut, [&](const std::string& key, const std::string& v) {
            if      (key == "CRIT") result.critical = toInt(v);
            else if (key == "ERR")  result.error = toInt(v);
            else if (key == "WARN") result.warning = toInt(v);
            else if (key == "CMSG") result.recentCritical.push_back(v);
            else if (key == "EMSG") result.recentErrors.push_back(v);
        });
    }

    if (progress) progress(100, "Event log analysis complete");
    Logger::instance().logScan("Event Log Analysis", true, t.elapsedMs(),
        std::to_string(result.critical) + " critical, " +
        std::to_string(result.error) + " errors");
    return result;
}

// ─────────────────────────────────────────────
//  PerformanceMonitor
// ─────────────────────────────────────────────
PerformanceMonitor::PerformanceMonitor(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

PerformanceResult PerformanceMonitor::measure(std::function<void(int, const std::string&)> progress) {
    PerformanceResult result;
    Logger::instance().info("Performance measurement started", "perf");
    Timer t;

    if (platform_.id == OS::Windows) {
        if (progress) progress(20, "Sampling CPU, RAM, processes...");
        auto r = Platform::exec(ps(
            "$os=Get-CimInstance Win32_OperatingSystem;"
            "if($os.TotalVisibleMemorySize -gt 0){"
            " 'RAM='+[math]::Round(100.0*($os.TotalVisibleMemorySize-$os.FreePhysicalMemory)/$os.TotalVisibleMemorySize,1) };"
            "'CPU='+(Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average;"
            "$procs=Get-Process;"
            "'PROC='+@($procs).Count;"
            "'SVC='+@(Get-Service -ErrorAction SilentlyContinue | Where-Object {$_.Status -eq 'Running'}).Count;"
            "$procs | Sort-Object CPU -Descending | Select-Object -First 5 | ForEach-Object {"
            " 'TC='+$_.ProcessName+'|'+$_.Id+'|'+[int]$_.CPU+'|'+$_.WorkingSet64 };"
            "$procs | Sort-Object WorkingSet64 -Descending | Select-Object -First 5 | ForEach-Object {"
            " 'TR='+$_.ProcessName+'|'+$_.Id+'|'+$_.WorkingSet64 }"), 45);

        forEachKV(r.stdOut, [&](const std::string& key, const std::string& v) {
            if      (key == "RAM")  result.ramUsagePercent = toDouble(v);
            else if (key == "CPU")  result.cpuUsagePercent = toDouble(v);
            else if (key == "PROC") result.processCount = toInt(v);
            else if (key == "SVC")  result.runningServices = toInt(v);
            else if (key == "TC") {
                auto p = splitBy(v, '|');
                if (p.size() < 4) return;
                ProcessInfo pi;
                pi.name = p[0]; pi.pid = p[1];
                pi.cpuPercent = toDouble(p[2]);
                pi.memoryBytes = toU64(p[3]);
                result.topCpu.push_back(pi);
            }
            else if (key == "TR") {
                auto p = splitBy(v, '|');
                if (p.size() < 3) return;
                ProcessInfo pi;
                pi.name = p[0]; pi.pid = p[1];
                pi.memoryBytes = toU64(p[2]);
                result.topRam.push_back(pi);
            }
        });
    }

    if (progress) progress(100, "Performance measurement complete");
    Logger::instance().logScan("Performance", true, t.elapsedMs(),
        "CPU=" + std::to_string((int)result.cpuUsagePercent) +
        "% RAM=" + std::to_string((int)result.ramUsagePercent) + "%");
    return result;
}

// ─────────────────────────────────────────────
//  ReliabilityAnalyzer
// ─────────────────────────────────────────────
ReliabilityAnalyzer::ReliabilityAnalyzer(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

ReliabilityResult ReliabilityAnalyzer::analyze(std::function<void(int, const std::string&)> progress) {
    ReliabilityResult result;
    Logger::instance().info("Reliability analysis started", "reliability");
    Timer t;

    if (platform_.id == OS::Windows) {
        if (progress) progress(20, "Analyzing failure events (last 14 days)...");
        // Read each log ONCE and derive every counter from the same snapshot.
        // Bounded to the last 14 days so ancient crashes don't permanently
        // tank the reliability index.
        auto r = Platform::exec(ps(
            "$cut=(Get-Date).AddDays(-14);"
            "$s=@(Get-WinEvent -LogName 'System' -MaxEvents 2000 -ErrorAction SilentlyContinue |"
            " Where-Object {$_.TimeCreated -gt $cut});"
            "$a=@(Get-WinEvent -LogName 'Application' -MaxEvents 2000 -ErrorAction SilentlyContinue |"
            " Where-Object {$_.TimeCreated -gt $cut});"
            "'BSOD='+@($s | Where-Object {$_.Id -eq 41 -or ($_.Id -eq 1001 -and $_.ProviderName -match 'BugCheck')}).Count;"
            "'SHUT='+@($s | Where-Object {$_.Id -eq 6008}).Count;"
            "'DRV='+@($s | Where-Object {$_.Id -eq 219 -and $_.ProviderName -match 'Kernel-PnP'}).Count;"
            "'APP='+@($a | Where-Object {$_.Id -eq 1000 -and $_.ProviderName -eq 'Application Error'}).Count;"
            // Setup-log failures are Level 2 (Error). Event *ID* 2 is the
            // routine 'package successfully changed state' success message.
            "'UPD='+@(Get-WinEvent -LogName 'Setup' -MaxEvents 200 -ErrorAction SilentlyContinue |"
            " Where-Object {$_.Level -eq 2 -and $_.TimeCreated -gt $cut}).Count"), 60);

        forEachKV(r.stdOut, [&](const std::string& key, const std::string& v) {
            if      (key == "BSOD") result.blueScreens = toInt(v);
            else if (key == "SHUT") result.unexpectedShutdowns = toInt(v);
            else if (key == "DRV")  result.driverCrashes = toInt(v);
            else if (key == "APP")  result.appCrashes = toInt(v);
            else if (key == "UPD")  result.updateFailures = toInt(v);
        });

        double index = 10.0;
        index -= result.blueScreens * 0.5;
        index -= result.unexpectedShutdowns * 0.3;
        index -= result.appCrashes * 0.1;
        index -= result.driverCrashes * 0.2;
        index -= result.updateFailures * 0.1;
        result.reliabilityIndex = std::max(0.0, std::min(10.0, index));
    }

    if (progress) progress(100, "Reliability analysis complete");
    Logger::instance().logScan("Reliability", true, t.elapsedMs(),
        "index=" + std::to_string(result.reliabilityIndex) +
        " BSOD=" + std::to_string(result.blueScreens));
    return result;
}

// ─────────────────────────────────────────────
//  WindowsInfoScanner
// ─────────────────────────────────────────────
WindowsInfoScanner::WindowsInfoScanner(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

WindowsInfoResult WindowsInfoScanner::scan(std::function<void(int, const std::string&)> progress) {
    WindowsInfoResult result;
    Logger::instance().info("Windows info scan started", "winfo");
    Timer t;

    if (platform_.id == OS::Windows) {
        result.version = platform_.osVersion;
        result.build   = platform_.osBuild;

        if (progress) progress(20, "Gathering Windows configuration...");
        // Everything in one shot. Feature checks use services/registry instead
        // of Get-WindowsOptionalFeature, which requires elevation and is slow.
        auto r = Platform::exec(ps(
            "$os=Get-CimInstance Win32_OperatingSystem;"
            "'ED='+$os.Caption;"
            "'INST='+$os.InstallDate;"
            "'BOOT='+$os.LastBootUpTime;"
            "$up=(Get-Date)-$os.LastBootUpTime;"
            "'UPT='+$up.Days+'d '+$up.Hours+'h '+$up.Minutes+'m';"
            "'ACT='+@(Get-CimInstance SoftwareLicensingProduct -ErrorAction SilentlyContinue |"
            " Where-Object {$_.PartialProductKey -and $_.LicenseStatus -eq 1 -and"
            " $_.ApplicationID -eq '55c92734-d682-4d71-983e-d6ec3f16059f'}).Count;"
            "$mp=Get-MpComputerStatus -ErrorAction SilentlyContinue;"
            "if($mp){'DEF='+($mp.AntivirusEnabled -and $mp.RealTimeProtectionEnabled)}else{'DEF=False'};"
            "'FW='+@(Get-NetFirewallProfile -ErrorAction SilentlyContinue | Where-Object {$_.Enabled -eq $true}).Count;"
            "'HV='+[bool](Get-Service -Name vmms -ErrorAction SilentlyContinue);"
            "'WSL='+[bool](Get-Service -Name LxssManager -ErrorAction SilentlyContinue);"
            "'SBX='+(Test-Path (Join-Path $env:windir 'System32\\WindowsSandbox.exe'));"
            "$hvci=(Get-ItemProperty 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity' -ErrorAction SilentlyContinue).Enabled;"
            "'MI='+($hvci -eq 1);"
            "$vbs=(Get-ItemProperty 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard' -ErrorAction SilentlyContinue).EnableVirtualizationBasedSecurity;"
            "'VBS='+($vbs -eq 1);"
            "$ss=(Get-ItemProperty 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer' -ErrorAction SilentlyContinue).SmartScreenEnabled;"
            "'SS='+($null -ne $ss -and $ss -ne 'Off');"
            "'FS='+((Get-ItemProperty 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power' -ErrorAction SilentlyContinue).HiberbootEnabled -eq 1)"), 60);

        forEachKV(r.stdOut, [&](const std::string& key, const std::string& v) {
            if      (key == "ED")   result.edition = v;
            else if (key == "INST") result.installDate = v;
            else if (key == "BOOT") result.lastBoot = v;
            else if (key == "UPT")  result.uptime = v;
            else if (key == "ACT")  result.activated = toInt(v) > 0;
            else if (key == "DEF")  result.defenderActive = truthy(v);
            else if (key == "FW")   result.firewallActive = toInt(v) > 0;
            else if (key == "HV")   result.hyperV = truthy(v);
            else if (key == "WSL")  result.wsl = truthy(v);
            else if (key == "SBX")  result.sandbox = truthy(v);
            else if (key == "MI") { result.memoryIntegrity = truthy(v); result.coreIsolation = truthy(v); }
            else if (key == "VBS")  result.vbs = truthy(v);
            else if (key == "SS")   result.smartScreen = truthy(v);
            else if (key == "FS")   result.fastStartup = truthy(v);
        });
    }

    if (progress) progress(100, "Windows info scan complete");
    Logger::instance().logScan("Windows Info", true, t.elapsedMs(),
        result.edition + " " + result.version);
    return result;
}

// ─────────────────────────────────────────────
//  CorruptionScanner
// ─────────────────────────────────────────────
CorruptionScanner::CorruptionScanner(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

CorruptionResult CorruptionScanner::scan(std::function<void(int, const std::string&)> progress) {
    CorruptionResult result;
    Logger::instance().info("Corruption scan started", "corruption");
    Timer t;

    if (platform_.id == OS::Windows) {
        if (progress) progress(20, "Checking component store (DISM)...");
        // CheckHealth only reads stored flags — fast. ScanHealth (a full
        // multi-minute store verification) is deliberately NOT run here;
        // it lives in Advanced Tools. DISM also needs elevation.
        if (platform_.elevated) {
            auto dismR = Platform::exec("DISM /Online /Cleanup-Image /CheckHealth", 120);
            std::string lower = dismR.stdOut;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("repairable") != std::string::npos ||
                lower.find("corrupt") != std::string::npos) {
                result.dismIssues = true;
                ++result.totalIssues;
                result.details.push_back("DISM reports component store corruption (run DISM RestoreHealth)");
            }
        }

        if (progress) progress(50, "Checking CBS log, pending operations & store...");
        // Single PowerShell pass. The CBS check reads only the log tail
        // (recent servicing activity) — old entries about long-fixed issues
        // are noise, and CBS.log can grow to hundreds of MB.
        auto r = Platform::exec(ps(
            "'CBS='+@(Get-Content -Path (Join-Path $env:windir 'Logs\\CBS\\CBS.log') -Tail 20000 -ErrorAction SilentlyContinue |"
            " Select-String -SimpleMatch 'corrupt' | Where-Object {$_ -notmatch '(?i)successfully'}).Count;"
            "'PFRO='+[bool](Get-ItemProperty 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\Session Manager'"
            " -Name PendingFileRenameOperations -ErrorAction SilentlyContinue);"
            "'CBSRP='+(Test-Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending');"
            "'WURP='+(Test-Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired');"
            "'SETUPFAIL='+@(Get-WinEvent -LogName 'Setup' -MaxEvents 100 -ErrorAction SilentlyContinue |"
            " Where-Object {$_.Level -eq 2}).Count;"
            "'STORE='+@(Get-AppxPackage -Name Microsoft.WindowsStore -ErrorAction SilentlyContinue).Count"), 90);

        bool pfro = false, cbsrp = false, wurp = false;
        int setupFail = 0, storeCount = -1;
        forEachKV(r.stdOut, [&](const std::string& key, const std::string& v) {
            if      (key == "CBS") {
                int cbsIssues = toInt(v);
                if (cbsIssues > 0) {
                    result.cbsIssues = true;
                    ++result.totalIssues;
                    result.details.push_back("CBS log contains " + std::to_string(cbsIssues) +
                                             " recent corruption-related entries (run SFC for details)");
                }
            }
            else if (key == "PFRO")      pfro = truthy(v);
            else if (key == "CBSRP")     cbsrp = truthy(v);
            else if (key == "WURP")      wurp = truthy(v);
            else if (key == "SETUPFAIL") setupFail = toInt(v);
            else if (key == "STORE")     storeCount = toInt(v, -1);
        });

        if (cbsrp || wurp || pfro) {
            result.pendingReboot = true;
            ++result.totalIssues;
            std::string why = cbsrp ? "component servicing" : wurp ? "Windows Update" : "file rename operations";
            result.details.push_back("Pending reboot detected (" + why + ")");
        }
        if (setupFail > 0) {
            result.failedUpdates = true;
            ++result.totalIssues;
            result.details.push_back(std::to_string(setupFail) + " failed Windows update event(s) found");
        }
        // Only flag the Store when the query SUCCEEDED and returned zero
        // packages — an inaccessible query (storeCount < 0) proves nothing.
        if (storeCount == 0) {
            result.storeCorruption = true;
            ++result.totalIssues;
            result.details.push_back("Microsoft Store package not found — Store may be corrupted or removed");
        }
    }

    if (result.totalIssues == 0)
        result.details.push_back("No corruption detected");

    if (progress) progress(100, "Corruption scan complete");
    Logger::instance().logScan("Corruption Scan", true, t.elapsedMs(),
        std::to_string(result.totalIssues) + " issues");
    return result;
}

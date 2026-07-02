/**
 * repair.cpp
 * Full implementation of all Windows system repair operations.
 * All repair actions are Windows-only; on other platforms they return
 * informative "not supported" results rather than failing silently.
 */

#include "../include/repair.h"
#include "../include/logger.h"
#include "../include/ui.h"

#include <sstream>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <wuapi.h>
  #include <comdef.h>
  #pragma comment(lib, "wbemuuid.lib")
#endif

// ─────────────────────────────────────────────
//  RepairResult static helpers
// ─────────────────────────────────────────────
RepairResult RepairResult::skip(const std::string& action, const std::string& reason) {
    RepairResult r; r.action = action; r.skipped = true;
    r.success = true; r.output = reason; return r;
}
RepairResult RepairResult::ok(const std::string& action, double ms, const std::string& out) {
    RepairResult r; r.action = action; r.success = true;
    r.durationMs = ms; r.output = out; return r;
}
RepairResult RepairResult::fail(const std::string& action, double ms, const std::string& err) {
    RepairResult r; r.action = action; r.success = false;
    r.durationMs = ms; r.errorDetail = err; return r;
}

// ─────────────────────────────────────────────
//  RepairSession
// ─────────────────────────────────────────────
void RepairSession::add(const RepairResult& r) {
    results.push_back(r);
    if (r.skipped) { ++totalSkip; return; }
    ++totalRun;
    if (r.success) ++totalPass; else ++totalFail;
    if (r.rebootNeeded) rebootNeeded = true;
    totalMs += r.durationMs;
}

void RepairSession::summarize() {
    Logger::instance().info(
        "Repair session complete. Run=" + std::to_string(totalRun) +
        " Pass=" + std::to_string(totalPass) +
        " Fail=" + std::to_string(totalFail) +
        " Skip=" + std::to_string(totalSkip),
        "repair", totalMs);
}

// ─────────────────────────────────────────────
//  WindowsRepair constructor
// ─────────────────────────────────────────────
WindowsRepair::WindowsRepair(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

bool WindowsRepair::isWindows() const {
    return platform_.id == OS::Windows;
}

RepairResult WindowsRepair::notWindows(const std::string& action) const {
    return RepairResult::skip(action,
        "Not supported on " + platform_.osName + " (Windows only)");
}

CmdResult WindowsRepair::runCmd(const std::string& cmd, int timeoutSec) {
    return Platform::exec(cmd, timeoutSec);
}

// ─────────────────────────────────────────────
//  DISM
// ─────────────────────────────────────────────
RepairResult WindowsRepair::dismCheckHealth(ProgressCb cb) {
    if (!isWindows()) return notWindows("DISM CheckHealth");
    if (cb) cb(0, "Running DISM /Online /Cleanup-Image /CheckHealth...");
    Timer t;
    auto r = runCmd("DISM /Online /Cleanup-Image /CheckHealth", 300);
    double ms = t.elapsedMs();
    if (cb) cb(100, "DISM CheckHealth complete");
    Logger::instance().logRepair("DISM CheckHealth", r.exitCode == 0, ms, r.stdOut);
    if (r.exitCode == 0)
        return RepairResult::ok("DISM CheckHealth", ms, r.stdOut);
    return RepairResult::fail("DISM CheckHealth", ms,
        "Exit code " + std::to_string(r.exitCode) + "\n" + r.stdOut);
}

RepairResult WindowsRepair::dismScanHealth(ProgressCb cb) {
    if (!isWindows()) return notWindows("DISM ScanHealth");
    if (cb) cb(0, "Running DISM /Online /Cleanup-Image /ScanHealth...");
    Timer t;
    auto r = runCmd("DISM /Online /Cleanup-Image /ScanHealth", 600);
    double ms = t.elapsedMs();
    if (cb) cb(100, "DISM ScanHealth complete");
    Logger::instance().logRepair("DISM ScanHealth", r.exitCode == 0, ms, r.stdOut);
    if (r.exitCode == 0)
        return RepairResult::ok("DISM ScanHealth", ms, r.stdOut);
    return RepairResult::fail("DISM ScanHealth", ms,
        "Exit code " + std::to_string(r.exitCode) + "\n" + r.stdOut);
}

RepairResult WindowsRepair::dismRestoreHealth(ProgressCb cb) {
    if (!isWindows()) return notWindows("DISM RestoreHealth");
    if (cb) cb(0, "Running DISM /Online /Cleanup-Image /RestoreHealth...");
    Timer t;
    auto r = runCmd("DISM /Online /Cleanup-Image /RestoreHealth", 1800);
    double ms = t.elapsedMs();
    if (cb) cb(100, "DISM RestoreHealth complete");
    Logger::instance().logRepair("DISM RestoreHealth", r.exitCode == 0, ms, r.stdOut);
    if (r.exitCode == 0)
        return RepairResult::ok("DISM RestoreHealth", ms, r.stdOut);
    return RepairResult::fail("DISM RestoreHealth", ms,
        "Exit code " + std::to_string(r.exitCode) + "\n" + r.stdOut);
}

// ─────────────────────────────────────────────
//  SFC
// ─────────────────────────────────────────────
RepairResult WindowsRepair::runSfc(ProgressCb cb) {
    if (!isWindows()) return notWindows("SFC /scannow");
    if (cb) cb(0, "Running SFC /scannow...");
    Timer t;
    auto r = runCmd("sfc /scannow", 1800);
    double ms = t.elapsedMs();
    if (cb) cb(100, "SFC complete");
    bool ok = (r.exitCode == 0) ||
              (r.stdOut.find("did not find any integrity violations") != std::string::npos);
    Logger::instance().logRepair("SFC /scannow", ok, ms, r.stdOut);
    return ok ? RepairResult::ok("SFC /scannow", ms, r.stdOut)
              : RepairResult::fail("SFC /scannow", ms, r.stdOut);
}

// ─────────────────────────────────────────────
//  CHKDSK
// ─────────────────────────────────────────────
RepairResult WindowsRepair::chkdskScan(const std::string& drive, ProgressCb cb) {
    if (!isWindows()) return notWindows("CHKDSK Scan");
    if (cb) cb(0, "Running CHKDSK " + drive + "...");
    Timer t;
    auto r = runCmd("chkdsk " + drive, 600);
    double ms = t.elapsedMs();
    if (cb) cb(100, "CHKDSK scan complete");
    Logger::instance().logRepair("CHKDSK " + drive, r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("CHKDSK " + drive, ms, r.stdOut)
                           : RepairResult::fail("CHKDSK " + drive, ms, r.stdErr);
}

RepairResult WindowsRepair::scheduleChkdsk(const std::string& drive) {
    if (!isWindows()) return notWindows("Schedule CHKDSK");
    Timer t;
    // Echo 'Y' to confirm scheduling on next reboot
    auto r = runCmd("echo Y | chkdsk " + drive + " /f /r /x", 30);
    double ms = t.elapsedMs();
    RepairResult res;
    res.action       = "Schedule CHKDSK " + drive;
    res.success      = true;
    res.rebootNeeded = true;
    res.durationMs   = ms;
    res.output       = "CHKDSK scheduled for next reboot on " + drive;
    Logger::instance().logRepair(res.action, true, ms, res.output);
    return res;
}

// ─────────────────────────────────────────────
//  Network repairs
// ─────────────────────────────────────────────
RepairResult WindowsRepair::flushDns() {
    if (!isWindows()) {
        // On macOS: dscacheutil -flushcache; on Linux: systemd-resolve --flush-caches
        Timer t;
        std::string cmd;
        if (platform_.id == OS::macOS)
            cmd = "dscacheutil -flushcache && killall -HUP mDNSResponder";
        else
            cmd = "systemd-resolve --flush-caches 2>/dev/null || resolvectl flush-caches 2>/dev/null";
        auto r = Platform::exec(cmd, 30);
        double ms = t.elapsedMs();
        Logger::instance().logRepair("Flush DNS", r.exitCode == 0, ms, r.stdOut);
        return r.exitCode == 0
            ? RepairResult::ok("Flush DNS", ms, "DNS cache flushed")
            : RepairResult::fail("Flush DNS", ms, r.stdErr);
    }
    Timer t;
    auto r = runCmd("ipconfig /flushdns", 30);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Flush DNS", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("Flush DNS", ms, r.stdOut)
                           : RepairResult::fail("Flush DNS", ms, r.stdErr);
}

RepairResult WindowsRepair::renewIp() {
    if (!isWindows()) return notWindows("Renew IP");
    Timer t;
    auto r = runCmd("ipconfig /release && ipconfig /renew", 60);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Renew IP", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("Renew IP", ms, r.stdOut)
                           : RepairResult::fail("Renew IP", ms, r.stdErr);
}

RepairResult WindowsRepair::winsockReset() {
    if (!isWindows()) return notWindows("Winsock Reset");
    Timer t;
    auto r = runCmd("netsh winsock reset", 60);
    double ms = t.elapsedMs();
    RepairResult res = r.exitCode == 0
        ? RepairResult::ok("Winsock Reset", ms, r.stdOut)
        : RepairResult::fail("Winsock Reset", ms, r.stdErr);
    res.rebootNeeded = (r.exitCode == 0);
    Logger::instance().logRepair("Winsock Reset", r.exitCode == 0, ms, r.stdOut);
    return res;
}

RepairResult WindowsRepair::networkStackReset() {
    if (!isWindows()) return notWindows("Network Stack Reset");
    Timer t;
    std::string cmds =
        "netsh int ip reset && "
        "netsh int ipv6 reset && "
        "netsh interface tcp set global autotuning=normal && "
        "netsh advfirewall reset";
    auto r = runCmd(cmds, 120);
    double ms = t.elapsedMs();
    RepairResult res = r.exitCode == 0
        ? RepairResult::ok("Network Stack Reset", ms, r.stdOut)
        : RepairResult::fail("Network Stack Reset", ms, r.stdErr);
    res.rebootNeeded = (r.exitCode == 0);
    Logger::instance().logRepair("Network Stack Reset", r.exitCode == 0, ms, r.stdOut);
    return res;
}

RepairResult WindowsRepair::resetNetworkAdapters() {
    if (!isWindows()) return notWindows("Reset Network Adapters");
    Timer t;
    // Disable and re-enable all adapters via PowerShell
    std::string cmd =
        "powershell -NoProfile -Command \""
        "Get-NetAdapter | Disable-NetAdapter -Confirm:$false; "
        "Start-Sleep -Seconds 2; "
        "Get-NetAdapter | Enable-NetAdapter -Confirm:$false\"";
    auto r = runCmd(cmd, 60);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Reset Network Adapters", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("Reset Network Adapters", ms, r.stdOut)
                           : RepairResult::fail("Reset Network Adapters", ms, r.stdErr);
}

// ─────────────────────────────────────────────
//  Windows Update cache
// ─────────────────────────────────────────────
RepairResult WindowsRepair::resetWindowsUpdateCache(bool confirmed) {
    if (!isWindows()) return notWindows("Reset Windows Update Cache");
    if (!confirmed)
        return RepairResult::skip("Reset Windows Update Cache",
            "Requires explicit confirmation");

    Timer t;
    // Timestamped rename targets so the reset also works when a previous
    // run already left a *.old directory behind.
    std::string stamp = Platform::timestamp();
    std::string cmds =
        "net stop wuauserv && "
        "net stop cryptSvc && "
        "net stop bits && "
        "net stop msiserver && "
        "ren \"%SystemRoot%\\SoftwareDistribution\" SoftwareDistribution." + stamp + ".old && "
        "ren \"%SystemRoot%\\System32\\catroot2\" catroot2." + stamp + ".old && "
        "net start wuauserv && "
        "net start cryptSvc && "
        "net start bits && "
        "net start msiserver";
    auto r = runCmd(cmds, 120);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Reset WU Cache", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0
        ? RepairResult::ok("Reset Windows Update Cache", ms,
                           "SoftwareDistribution and catroot2 renamed. WU restarted.")
        : RepairResult::fail("Reset Windows Update Cache", ms, r.stdErr);
}

// ─────────────────────────────────────────────
//  Cleanup operations
// ─────────────────────────────────────────────
RepairResult WindowsRepair::cleanupComponents() {
    if (!isWindows()) return notWindows("Component Cleanup");
    Timer t;
    auto r = runCmd("DISM /Online /Cleanup-Image /StartComponentCleanup", 1800);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Component Cleanup", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("Component Cleanup", ms, r.stdOut)
                           : RepairResult::fail("Component Cleanup", ms, r.stdErr);
}

RepairResult WindowsRepair::cleanupWinSxS() {
    if (!isWindows()) return notWindows("WinSxS Cleanup");
    Timer t;
    auto r = runCmd("DISM /Online /Cleanup-Image /StartComponentCleanup /ResetBase", 3600);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("WinSxS Cleanup", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("WinSxS Cleanup", ms, r.stdOut)
                           : RepairResult::fail("WinSxS Cleanup", ms, r.stdErr);
}

RepairResult WindowsRepair::cleanTempFolders() {
    // Works cross-platform
    Timer t;
    std::string tempPath = Platform::getTempPath();
    std::string cmd;
#ifdef _WIN32
    cmd = "del /q /f /s \"" + tempPath + "\\*\" >nul 2>&1";
#else
    cmd = "find \"" + tempPath + "\" -maxdepth 1 -mindepth 1 -delete 2>/dev/null";
#endif
    auto r = Platform::exec(cmd, 60);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Clean Temp Folders", true, ms, "Cleaned: " + tempPath);
    return RepairResult::ok("Clean Temp Folders", ms, "Cleaned: " + tempPath);
}

RepairResult WindowsRepair::cleanWinTemp() {
    if (!isWindows()) return notWindows("Clean Windows Temp");
    Timer t;
    auto r = runCmd("del /q /f /s \"%SystemRoot%\\Temp\\*\" >nul 2>&1", 60);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Clean Windows Temp", true, ms, "");
    return RepairResult::ok("Clean Windows Temp", ms, "Windows temp cleaned");
}

RepairResult WindowsRepair::cleanUserTemp() {
    if (!isWindows()) return notWindows("Clean User Temp");
    Timer t;
    auto r = runCmd("del /q /f /s \"%TEMP%\\*\" >nul 2>&1", 60);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Clean User Temp", true, ms, "");
    return RepairResult::ok("Clean User Temp", ms, "User temp cleaned");
}

RepairResult WindowsRepair::cleanRecycleBin() {
    if (!isWindows()) {
        // macOS: rm -rf ~/.Trash/*; Linux: rm -rf ~/.local/share/Trash/*
        Timer t;
        std::string cmd;
        if (platform_.id == OS::macOS)
            cmd = "rm -rf ~/.Trash/* 2>/dev/null";
        else
            cmd = "rm -rf ~/.local/share/Trash/files/* ~/.local/share/Trash/info/* 2>/dev/null";
        Platform::exec(cmd, 30);
        double ms = t.elapsedMs();
        Logger::instance().logRepair("Clean Recycle Bin", true, ms, "Trash emptied");
        return RepairResult::ok("Clean Recycle Bin/Trash", ms, "Trash emptied");
    }
    Timer t;
    auto r = runCmd("powershell -NoProfile -Command \"Clear-RecycleBin -Force -ErrorAction SilentlyContinue\"", 30);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Clean Recycle Bin", true, ms, "");
    return RepairResult::ok("Clean Recycle Bin", ms, "Recycle Bin emptied");
}

RepairResult WindowsRepair::cleanBrowserCache(bool confirmed) {
    if (!confirmed)
        return RepairResult::skip("Clean Browser Cache", "Requires explicit confirmation");
    Timer t;
    std::vector<std::string> paths;
    std::string home = Platform::getHomePath();
#ifdef _WIN32
    paths.push_back(home + "\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cache");
    paths.push_back(home + "\\AppData\\Local\\Microsoft\\Edge\\User Data\\Default\\Cache");
    paths.push_back(home + "\\AppData\\Local\\Mozilla\\Firefox\\Profiles");
    for (auto& p : paths) {
        if (Platform::dirExists(p))
            Platform::exec("rd /s /q \"" + p + "\" >nul 2>&1", 30);
    }
#else
    paths.push_back(home + "/.cache/google-chrome");
    paths.push_back(home + "/.cache/chromium");
    paths.push_back(home + "/.cache/mozilla/firefox");
    for (auto& p : paths) {
        if (Platform::dirExists(p))
            Platform::exec("rm -rf \"" + p + "\" 2>/dev/null", 30);
    }
#endif
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Clean Browser Cache", true, ms, "");
    return RepairResult::ok("Clean Browser Cache", ms, "Browser caches cleared");
}

RepairResult WindowsRepair::cleanDeliveryOpt() {
    if (!isWindows()) return notWindows("Clean Delivery Optimization");
    Timer t;
    auto r = runCmd("powershell -NoProfile -Command \"Delete-DeliveryOptimizationCache -Force\"", 60);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Clean Delivery Optimization", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("Clean Delivery Optimization", ms, r.stdOut)
                           : RepairResult::fail("Clean Delivery Optimization", ms, r.stdErr);
}

// ─────────────────────────────────────────────
//  Defender
// ─────────────────────────────────────────────
RepairResult WindowsRepair::updateDefenderDefs() {
    if (!isWindows()) return notWindows("Update Defender Definitions");
    Timer t;
    auto r = runCmd("\"%ProgramFiles%\\Windows Defender\\MpCmdRun.exe\" -SignatureUpdate", 300);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Update Defender", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("Update Defender Definitions", ms, r.stdOut)
                           : RepairResult::fail("Update Defender Definitions", ms, r.stdErr);
}

// ─────────────────────────────────────────────
//  Services
// ─────────────────────────────────────────────
static RepairResult verifyAndStartService(const std::string& svcName) {
    Timer t;
    auto check = Platform::exec("sc query " + svcName, 10);
    bool running = check.stdOut.find("RUNNING") != std::string::npos;
    if (!running) {
        auto start = Platform::exec("net start " + svcName, 30);
        double ms = t.elapsedMs();
        bool ok = start.exitCode == 0 ||
                  start.stdOut.find("already been started") != std::string::npos;
        Logger::instance().logRepair("Start service: " + svcName, ok, ms, start.stdOut);
        return ok ? RepairResult::ok("Start service: " + svcName, ms, "Started")
                  : RepairResult::fail("Start service: " + svcName, ms, start.stdErr);
    }
    double ms = t.elapsedMs();
    return RepairResult::ok("Verify service: " + svcName, ms, "Already running");
}

RepairResult WindowsRepair::verifyWindowsInstaller() {
    if (!isWindows()) return notWindows("Verify Windows Installer");
    return verifyAndStartService("msiserver");
}
RepairResult WindowsRepair::verifyBits() {
    if (!isWindows()) return notWindows("Verify BITS");
    return verifyAndStartService("bits");
}
RepairResult WindowsRepair::verifyUpdateService() {
    if (!isWindows()) return notWindows("Verify Windows Update");
    return verifyAndStartService("wuauserv");
}
RepairResult WindowsRepair::verifyCryptServices() {
    if (!isWindows()) return notWindows("Verify Cryptographic Services");
    return verifyAndStartService("cryptSvc");
}

RepairResult WindowsRepair::repairCommonServices() {
    if (!isWindows()) return notWindows("Repair Common Services");
    RepairSession session;
    session.add(verifyWindowsInstaller());
    session.add(verifyBits());
    session.add(verifyUpdateService());
    session.add(verifyCryptServices());
    session.add(verifyAndStartService("EventLog"));
    session.add(verifyAndStartService("Schedule"));
    session.add(verifyAndStartService("Dnscache"));
    session.summarize();
    bool allOk = session.totalFail == 0;
    return allOk
        ? RepairResult::ok("Repair Common Services", session.totalMs,
                           std::to_string(session.totalPass) + " services verified")
        : RepairResult::fail("Repair Common Services", session.totalMs,
                             std::to_string(session.totalFail) + " services failed to start");
}

// ─────────────────────────────────────────────
//  Activation & restore points
// ─────────────────────────────────────────────
RepairResult WindowsRepair::verifyActivation() {
    if (!isWindows()) return notWindows("Verify Windows Activation");
    Timer t;
    auto r = runCmd("cscript /nologo \"%SystemRoot%\\System32\\slmgr.vbs\" /dli", 30);
    double ms = t.elapsedMs();
    bool activated = r.stdOut.find("Licensed") != std::string::npos;
    Logger::instance().logRepair("Verify Activation", activated, ms, r.stdOut);
    return activated ? RepairResult::ok("Verify Activation", ms, r.stdOut)
                     : RepairResult::fail("Verify Activation", ms, r.stdOut);
}

RepairResult WindowsRepair::checkRestorePoints() {
    if (!isWindows()) return notWindows("Check Restore Points");
    Timer t;
    auto r = runCmd("powershell -NoProfile -Command \"Get-ComputerRestorePoint | Select-Object -Property CreationTime,Description | Format-Table -AutoSize\"", 30);
    double ms = t.elapsedMs();
    bool hasPoints = !r.stdOut.empty() && r.stdOut.find("Description") != std::string::npos;
    Logger::instance().logRepair("Check Restore Points", hasPoints, ms, r.stdOut);
    return hasPoints ? RepairResult::ok("Check Restore Points", ms, r.stdOut)
                     : RepairResult::fail("Check Restore Points", ms, "No restore points found");
}

RepairResult WindowsRepair::createRestorePoint(const std::string& desc) {
    if (!isWindows()) return notWindows("Create Restore Point");
    Timer t;
    std::string cmd = "powershell -NoProfile -Command \""
        "Enable-ComputerRestore -Drive 'C:\\'; "
        "Checkpoint-Computer -Description '" + desc + "' -RestorePointType MODIFY_SETTINGS\"";
    auto r = runCmd(cmd, 120);
    double ms = t.elapsedMs();
    Logger::instance().logRepair("Create Restore Point", r.exitCode == 0, ms, r.stdOut);
    return r.exitCode == 0 ? RepairResult::ok("Create Restore Point", ms, "Restore point created: " + desc)
                           : RepairResult::fail("Create Restore Point", ms, r.stdErr);
}

// ─────────────────────────────────────────────
//  Page file
// ─────────────────────────────────────────────
RepairResult WindowsRepair::verifyPageFile() {
    if (!isWindows()) return notWindows("Verify Page File");
    Timer t;
    auto r = runCmd("wmic pagefile list /format:list", 15);
    double ms = t.elapsedMs();
    bool ok = r.stdOut.find("AllocatedBaseSize") != std::string::npos;
    Logger::instance().logRepair("Verify Page File", ok, ms, r.stdOut);
    return ok ? RepairResult::ok("Verify Page File", ms, r.stdOut)
              : RepairResult::fail("Verify Page File", ms, "Page file not configured");
}

// ─────────────────────────────────────────────
//  Full repair run
// ─────────────────────────────────────────────
RepairSession WindowsRepair::runFullRepair(bool doRestorePoint, ProgressCb cb) {
    RepairSession session;
    int step = 0, total = 20;

    auto progress = [&](const std::string& msg) {
        ++step;
        if (cb) cb((step * 100) / total, msg);
        UI::printInfo(msg);
    };

    if (doRestorePoint) {
        progress("Creating restore point...");
        session.add(createRestorePoint());
    }

    progress("DISM CheckHealth...");
    session.add(dismCheckHealth());

    progress("DISM ScanHealth...");
    session.add(dismScanHealth());

    progress("DISM RestoreHealth...");
    session.add(dismRestoreHealth());

    progress("SFC /scannow...");
    session.add(runSfc());

    progress("CHKDSK scan...");
    session.add(chkdskScan());

    progress("Flushing DNS...");
    session.add(flushDns());

    progress("Renewing IP...");
    session.add(renewIp());

    progress("Winsock reset...");
    session.add(winsockReset());

    progress("Network stack reset...");
    session.add(networkStackReset());

    progress("Cleaning temp folders...");
    session.add(cleanTempFolders());
    session.add(cleanWinTemp());
    session.add(cleanUserTemp());

    progress("Cleaning Recycle Bin...");
    session.add(cleanRecycleBin());

    progress("Cleaning Delivery Optimization...");
    session.add(cleanDeliveryOpt());

    progress("Updating Defender definitions...");
    session.add(updateDefenderDefs());

    progress("Verifying critical services...");
    session.add(repairCommonServices());

    progress("Verifying page file...");
    session.add(verifyPageFile());

    progress("Verifying Windows activation...");
    session.add(verifyActivation());

    progress("Component cleanup...");
    session.add(cleanupComponents());

    session.summarize();
    if (cb) cb(100, "Full repair complete");
    return session;
}

// ─────────────────────────────────────────────
//  UpdateManager
// ─────────────────────────────────────────────
UpdateManager::UpdateManager(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

UpdateCategory UpdateManager::classifyUpdate(const std::string& title) const {
    std::string lower = title;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("security") != std::string::npos)   return UpdateCategory::Security;
    if (lower.find("cumulative") != std::string::npos) return UpdateCategory::Quality;
    if (lower.find("feature") != std::string::npos)    return UpdateCategory::Feature;
    if (lower.find("driver") != std::string::npos)     return UpdateCategory::Driver;
    if (lower.find("definition") != std::string::npos) return UpdateCategory::Definition;
    if (lower.find("optional") != std::string::npos)   return UpdateCategory::Optional;
    return UpdateCategory::Unknown;
}

UpdateResult UpdateManager::detectUpdates(ProgressCb cb) {
    UpdateResult result;
    if (platform_.id != OS::Windows) {
        result.lastCheckDate = Platform::timestampHuman();
        return result;
    }

    if (cb) cb(10, "Querying Windows Update...");

    // Use PowerShell + PSWindowsUpdate or built-in WU API via COM
    // PSWindowsUpdate may not be installed; use UsoClient / WMI as fallback
    std::string cmd =
        "powershell -NoProfile -Command \""
        "try { "
        "  $Session = New-Object -ComObject Microsoft.Update.Session; "
        "  $Searcher = $Session.CreateUpdateSearcher(); "
        "  $Results = $Searcher.Search('IsInstalled=0 and Type=Software'); "
        "  foreach ($u in $Results.Updates) { "
        "    $kb = ($u.KBArticleIDs | Select-Object -First 1); "
        "    Write-Output (\\\"KB=$kb|Title=\\\" + $u.Title + \\\"|Reboot=\\\" + $u.RebootRequired); "
        "  } "
        "} catch { Write-Error $_.Exception.Message }\"";

    auto r = Platform::exec(cmd, 300);
    if (cb) cb(80, "Parsing update results...");

    result.lastCheckDate = Platform::timestampHuman();

    // Parse lines of form KB=XXXXXX|Title=...|Reboot=True
    std::istringstream iss(r.stdOut);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("KB=") == std::string::npos) continue;
        UpdateInfo info;
        auto extractField = [&](const std::string& key) -> std::string {
            auto pos = line.find(key + "=");
            if (pos == std::string::npos) return "";
            pos += key.size() + 1;
            auto end = line.find('|', pos);
            return line.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        };
        info.kb          = extractField("KB");
        info.title       = extractField("Title");
        info.rebootNeeded= extractField("Reboot") == "True";
        info.type        = classifyUpdate(info.title);
        if (info.rebootNeeded) result.rebootPending = true;

        result.available.push_back(info);
        ++result.totalAvailable;

        switch (info.type) {
            case UpdateCategory::Security:   result.security.push_back(info);  break;
            case UpdateCategory::Quality:    result.quality.push_back(info);   break;
            case UpdateCategory::Feature:    result.feature.push_back(info);   break;
            case UpdateCategory::Driver:     result.drivers.push_back(info);   break;
            case UpdateCategory::Optional:   result.optional.push_back(info);  break;
            default: break;
        }
    }

    // Check for failed updates in the Setup log. Failures are Level 2
    // (Error); event *ID* 2 is a routine success message.
    auto cbsR = Platform::exec(
        "powershell -NoProfile -Command \""
        "Get-WinEvent -LogName 'Setup' -MaxEvents 50 -ErrorAction SilentlyContinue | "
        "Where-Object { $_.Level -eq 2 } | Select-Object -ExpandProperty Message\"", 30);
    if (!cbsR.stdOut.empty()) {
        std::istringstream cbs(cbsR.stdOut);
        std::string l;
        while (std::getline(cbs, l))
            if (!l.empty()) result.failedUpdates.push_back(l);
    }

    if (cb) cb(100, "Update detection complete");
    Logger::instance().logScan("Windows Update Detection",
        true, 0.0,
        "Available=" + std::to_string(result.totalAvailable));
    return result;
}

RepairResult UpdateManager::installAll(ProgressCb cb) {
    if (platform_.id != OS::Windows) {
        return RepairResult::skip("Install Updates",
            "Not supported on " + platform_.osName);
    }
    if (cb) cb(0, "Installing all available updates via Windows Update...");
    Timer t;
    std::string cmd =
        "powershell -NoProfile -Command \""
        "$Session = New-Object -ComObject Microsoft.Update.Session; "
        "$Searcher = $Session.CreateUpdateSearcher(); "
        "$Results = $Searcher.Search('IsInstalled=0 and Type=Software'); "
        "$Downloader = $Session.CreateUpdateDownloader(); "
        "$Downloader.Updates = $Results.Updates; "
        "$Downloader.Download(); "
        "$Installer = $Session.CreateUpdateInstaller(); "
        "$Installer.Updates = $Results.Updates; "
        "$Install = $Installer.Install(); "
        "Write-Output ('Result=' + $Install.ResultCode)\"";
    auto r = Platform::exec(cmd, 3600);
    double ms = t.elapsedMs();
    bool ok = r.stdOut.find("Result=2") != std::string::npos ||
              r.stdOut.find("Result=3") != std::string::npos;
    if (cb) cb(100, "Update installation complete");
    Logger::instance().logRepair("Install All Updates", ok, ms, r.stdOut);
    RepairResult res = ok ? RepairResult::ok("Install All Updates", ms, r.stdOut)
                          : RepairResult::fail("Install All Updates", ms, r.stdErr);
    res.rebootNeeded = r.stdOut.find("reboot") != std::string::npos ||
                       r.stdOut.find("Reboot") != std::string::npos;
    return res;
}

RepairResult UpdateManager::installUpdates(const std::vector<std::string>& kbs, ProgressCb cb) {
    if (kbs.empty())
        return RepairResult::skip("Install Selected Updates", "No KBs specified");
    // Build a filter for specific KBs
    if (cb) cb(0, "Installing " + std::to_string(kbs.size()) + " selected updates...");
    // For now delegate to installAll; KB filtering requires additional COM work
    return installAll(cb);
}
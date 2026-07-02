/**
 * repair.h
 * Windows system repair operations: DISM, SFC, CHKDSK, network repairs,
 * cleanup, service management, Windows Update, and restore points.
 */

#pragma once

#include "toolkit.h"
#include "platform.h"
#include <string>
#include <vector>
#include <functional>

using ProgressCb = std::function<void(int percent, const std::string& status)>;

struct RepairSession {
    std::vector<RepairResult> results;
    int  totalRun   = 0;
    int  totalPass  = 0;
    int  totalFail  = 0;
    int  totalSkip  = 0;
    double totalMs = 0.0;
    bool  rebootNeeded = false;

    void add(const RepairResult& r);
    void summarize();
};

class WindowsRepair {
public:
    WindowsRepair(const AppConfig& cfg, const PlatformInfo& platform);

    // DISM
    RepairResult dismCheckHealth(ProgressCb cb = nullptr);
    RepairResult dismScanHealth(ProgressCb cb = nullptr);
    RepairResult dismRestoreHealth(ProgressCb cb = nullptr);

    // SFC
    RepairResult runSfc(ProgressCb cb = nullptr);

    // CHKDSK
    RepairResult chkdskScan(const std::string& drive = "C:", ProgressCb cb = nullptr);
    RepairResult scheduleChkdsk(const std::string& drive = "C:");

    // Network
    RepairResult flushDns();
    RepairResult renewIp();
    RepairResult winsockReset();
    RepairResult networkStackReset();
    RepairResult resetNetworkAdapters();

    // Windows Update cache
    RepairResult resetWindowsUpdateCache(bool confirmed = false);

    // Cleanup
    RepairResult cleanupComponents();
    RepairResult cleanupWinSxS();
    RepairResult cleanTempFolders();
    RepairResult cleanWinTemp();
    RepairResult cleanUserTemp();
    RepairResult cleanRecycleBin();
    RepairResult cleanBrowserCache(bool confirmed = false);
    RepairResult cleanDeliveryOpt();

    // Defender
    RepairResult updateDefenderDefs();

    // Services
    RepairResult verifyWindowsInstaller();
    RepairResult verifyBits();
    RepairResult verifyUpdateService();
    RepairResult verifyCryptServices();
    RepairResult repairCommonServices();

    // Activation & restore points
    RepairResult verifyActivation();
    RepairResult checkRestorePoints();
    RepairResult createRestorePoint(const std::string& desc = "System Health Toolkit - Pre-repair snapshot");

    // Page file
    RepairResult verifyPageFile();

    // Full repair run
    RepairSession runFullRepair(bool doRestorePoint = true, ProgressCb cb = nullptr);

private:
    bool isWindows() const;
    RepairResult notWindows(const std::string& action) const;
    CmdResult runCmd(const std::string& cmd, int timeoutSec = 60);

    AppConfig      cfg_;
    PlatformInfo   platform_;
};

class UpdateManager {
public:
    UpdateManager(const AppConfig& cfg, const PlatformInfo& platform);

    UpdateResult detectUpdates(ProgressCb cb = nullptr);
    RepairResult installAll(ProgressCb cb = nullptr);
    RepairResult installUpdates(const std::vector<std::string>& kbs, ProgressCb cb = nullptr);

private:
    UpdateCategory classifyUpdate(const std::string& title) const;
    AppConfig    cfg_;
    PlatformInfo platform_;
};
/**
 * scanners.h
 * System scanners: storage analysis, startup analysis, hardware detection,
 * network diagnostics, event log analysis, performance monitoring,
 * and reliability analysis.
 */

#pragma once

#include "toolkit.h"
#include "platform.h"
#include <string>
#include <vector>
#include <functional>

// ─────────────────────────────────────────────
//  StorageAnalyzer
// ─────────────────────────────────────────────
class StorageAnalyzer {
public:
    StorageAnalyzer(const AppConfig& cfg, const PlatformInfo& platform);

    // deepScan=false skips the full-volume file walk (drive totals and
    // special-path sizes only) — used by the quick Health Scan.
    StorageResult analyze(bool deepScan = true,
                          std::function<void(int, const std::string&)> progress = nullptr);
    ScanResult    scanDrive(const std::string& root,
                           std::function<void(int, const std::string&)> progress = nullptr);

private:
    void enumerateDrives(StorageResult& result);
    void scanTopItems(StorageResult& result, const std::string& root,
                      std::function<void(int, const std::string&)> progress);
    void scanTopItemsSingleThreaded(StorageResult& result, const std::string& root,
                                    std::function<void(int, const std::string&)> progress);
    void detectSpecialPaths(StorageResult& result);
    void computeReclaimable(StorageResult& result);

    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  StartupInfo
// ─────────────────────────────────────────────
struct StartupEntry {
    std::string name;
    std::string path;
    std::string location;   // e.g. "HKLM\\Run", "Startup Folder"
    std::string impact;     // "High", "Medium", "Low"
    bool        enabled     = true;
    bool        valid       = true;
    bool        signed_     = true;
};

struct StartupResult {
    std::vector<StartupEntry> entries;
    int totalCount   = 0;
    int enabledCount = 0;
    int disabledCount= 0;
    int brokenCount  = 0;
    int duplicateCount = 0;
    int unsignedCount  = 0;
};

class StartupAnalyzer {
public:
    StartupAnalyzer(const AppConfig& cfg, const PlatformInfo& platform);
    StartupResult analyze(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  HardwareInfo
// ─────────────────────────────────────────────
struct CpuInfo {
    std::string model;
    std::string architecture;
    int  cores       = 0;
    int  threads     = 0;
    double frequencyGHz = 0.0;
    double usagePercent  = 0.0;
    double temperatureC  = -1.0;
};

struct MemoryInfo {
    uint64_t totalBytes  = 0;
    uint64_t availableBytes = 0;
    uint64_t usedBytes   = 0;
    double   usagePercent= 0.0;
    int      slots       = 0;
    double   speedMHz    = 0.0;
};

struct GpuInfo {
    std::string name;
    std::string driver;
    std::string driverVersion;
    uint64_t vramBytes = 0;
    double   temperatureC = -1.0;
};

struct HardwareResult {
    CpuInfo    cpu;
    MemoryInfo memory;
    std::vector<GpuInfo> gpus;
    std::string motherboard;
    std::string biosVersion;
    bool        uefi       = false;
    bool        secureBoot = false;
    bool        tpmPresent = false;
    int         displayCount = 0;
    double      batteryPercent = -1.0;
};

class HardwareScanner {
public:
    HardwareScanner(const AppConfig& cfg, const PlatformInfo& platform);
    HardwareResult scan(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  NetworkInfo
// ─────────────────────────────────────────────
struct NetworkAdapter {
    std::string name;
    std::string type;       // "Wi-Fi", "Ethernet", "Virtual"
    std::string mac;
    std::string ipv4;
    std::string ipv6;
    std::string dns;
    std::string dhcp;
    bool        connected   = false;
    double      speedMbps   = 0.0;
    int         mtu         = 1500;
};

struct NetworkResult {
    std::vector<NetworkAdapter> adapters;
    std::string publicIp;
    std::string gateway;
    double      pingMs      = 0.0;
    double      packetLoss  = 0.0;
    double      dnsLatencyMs= 0.0;
    bool        vpnActive   = false;
    bool        proxyActive = false;
    bool        firewallActive = true;
    std::vector<int> openPorts;
    std::vector<int> listeningPorts;
};

class NetworkScanner {
public:
    NetworkScanner(const AppConfig& cfg, const PlatformInfo& platform);
    NetworkResult scan(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  EventLogInfo
// ─────────────────────────────────────────────
struct EventLogSummary {
    int critical = 0;
    int error    = 0;
    int warning  = 0;
    int info     = 0;
    std::vector<std::string> recentCritical;
    std::vector<std::string> recentErrors;
};

class EventLogAnalyzer {
public:
    EventLogAnalyzer(const AppConfig& cfg, const PlatformInfo& platform);
    EventLogSummary analyze(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  PerformanceInfo
// ─────────────────────────────────────────────
struct ProcessInfo {
    std::string name;
    std::string pid;
    double cpuPercent  = 0.0;
    uint64_t memoryBytes = 0;
    uint64_t diskBytes   = 0;
    int      handleCount = 0;
    int      threadCount = 0;
};

struct PerformanceResult {
    double cpuUsagePercent  = 0.0;
    double ramUsagePercent  = 0.0;
    double gpuUsagePercent  = 0.0;
    double diskActivityPercent = 0.0;
    int    processCount     = 0;
    int    runningServices  = 0;
    std::vector<ProcessInfo> topCpu;
    std::vector<ProcessInfo> topRam;
    std::vector<ProcessInfo> topDisk;
};

class PerformanceMonitor {
public:
    PerformanceMonitor(const AppConfig& cfg, const PlatformInfo& platform);
    PerformanceResult measure(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  ReliabilityInfo
// ─────────────────────────────────────────────
struct ReliabilityResult {
    int  blueScreens      = 0;
    int  unexpectedShutdowns = 0;
    int  kernelCrashes    = 0;
    int  driverCrashes    = 0;
    int  appCrashes       = 0;
    int  updateFailures   = 0;
    double reliabilityIndex = 10.0; // 0-10 scale
    std::vector<std::string> recentFailures;
};

class ReliabilityAnalyzer {
public:
    ReliabilityAnalyzer(const AppConfig& cfg, const PlatformInfo& platform);
    ReliabilityResult analyze(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  WindowsInfo
// ─────────────────────────────────────────────
struct WindowsInfoResult {
    std::string edition;
    std::string version;
    std::string build;
    std::string installDate;
    std::string lastBoot;
    std::string uptime;
    bool        activated       = false;
    bool        fastStartup     = false;
    bool        hibernate       = false;
    bool        hyperV          = false;
    bool        wsl             = false;
    bool        containers      = false;
    bool        sandbox         = false;
    bool        defenderActive  = false;
    bool        firewallActive  = false;
    bool        smartScreen     = false;
    bool        coreIsolation   = false;
    bool        memoryIntegrity = false;
    bool        credentialGuard = false;
    bool        deviceGuard     = false;
    bool        vbs             = false;
};

class WindowsInfoScanner {
public:
    WindowsInfoScanner(const AppConfig& cfg, const PlatformInfo& platform);
    WindowsInfoResult scan(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  CorruptionScanner
// ─────────────────────────────────────────────
struct CorruptionResult {
    bool missingSystemFiles    = false;
    bool corruptedFiles        = false;
    bool brokenComponents      = false;
    bool pendingReboot         = false;
    bool failedUpdates         = false;
    bool corruptRegistry       = false;
    bool brokenServicingStack  = false;
    bool cbsIssues             = false;
    bool dismIssues            = false;
    bool storeCorruption       = false;
    int  totalIssues           = 0;
    std::vector<std::string> details;
};

class CorruptionScanner {
public:
    CorruptionScanner(const AppConfig& cfg, const PlatformInfo& platform);
    CorruptionResult scan(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  DiskBenchmark
// ─────────────────────────────────────────────
class DiskBenchmark {
public:
    DiskBenchmark(const AppConfig& cfg, const PlatformInfo& platform);
    DiskBenchResult run(const std::string& drivePath,
                       std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  NetworkBenchmark
// ─────────────────────────────────────────────
class NetworkBenchmark {
public:
    NetworkBenchmark(const AppConfig& cfg, const PlatformInfo& platform);
    NetworkBenchResult run(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  SystemBenchmark
// ─────────────────────────────────────────────
class SystemBenchmark {
public:
    SystemBenchmark(const AppConfig& cfg, const PlatformInfo& platform);
    SystemBenchResult runAll(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  RegistryScanner
// ─────────────────────────────────────────────
class RegistryScanner {
public:
    RegistryScanner(const AppConfig& cfg, const PlatformInfo& platform);
    RegistryScanResult scan(bool detectOnly = true,
                           std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  DriverScanner
// ─────────────────────────────────────────────
class DriverScanner {
public:
    DriverScanner(const AppConfig& cfg, const PlatformInfo& platform);
    DriverScanResult scan(std::function<void(int, const std::string&)> progress = nullptr);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  MalwareScanner  (defensive AV — full-system)
// ─────────────────────────────────────────────
// Depth of the antivirus pass. `None` runs only the fast assessment
// (Defender status + threat history + local heuristics) with no disk sweep;
// `Quick` and `Full` invoke the platform AV engine's scan.
enum class MalwareScanDepth { None, Quick, Full };

struct MalwareThreat {
    std::string name;       // e.g. "Trojan:Win32/Wacatac.B!ml"
    std::string path;       // affected resource / file
    std::string severity;   // "Low", "Moderate", "High", "Severe", "Unknown"
    std::string status;     // "Active", "Quarantined", "Removed", "Allowed", "Cleaned"
    std::string source;     // "Defender", "ClamAV", "Heuristic"
    std::string detected;   // timestamp when first seen
};

struct MalwareResult {
    // ── AV engine status ──
    bool        enginePresent      = false; // Defender / ClamAV available
    std::string engineName;                 // "Windows Defender", "ClamAV", "None"
    bool        realTimeProtection = false;
    bool        antivirusEnabled   = false;
    bool        tamperProtection    = false;
    int         definitionAgeDays  = -1;    // -1 = unknown
    std::string signatureVersion;
    std::string engineVersion;
    std::string lastQuickScan;
    std::string lastFullScan;

    // ── Scan pass ──
    bool        scanRan         = false;
    std::string scanType        = "Assessment"; // "Assessment", "Quick", "Full"
    uint64_t    filesScanned    = 0;
    double      scanDurationSec = 0.0;

    // ── Detections ──
    std::vector<MalwareThreat> activeThreats;   // current, unresolved
    std::vector<MalwareThreat> history;         // past detections (resolved)
    int         activeCount   = 0;

    // ── Heuristic indicators (fast, always run) ──
    std::vector<std::string> suspiciousProcesses; // running from temp
    std::vector<std::string> unsignedProcesses;   // userland exe, not validly signed
    std::vector<std::string> lolbinProcesses;     // living-off-the-land binary abuse
    std::vector<std::string> suspiciousServices;  // service ImagePath in userland
    std::vector<std::string> suspiciousConnections; // established conns from userland procs
    std::vector<std::string> suspiciousAutoruns;  // odd autorun locations
    std::vector<std::string> suspiciousTasks;     // suspicious scheduled tasks
    std::vector<std::string> wmiPersistence;      // WMI event-subscription persistence
    std::vector<std::string> defenderExclusions;  // configured AV exclusions (tamper signal)
    bool        defenderTampered  = false;        // realtime/behavior/script scan disabled
    bool        hostsFileModified = false;        // non-default hosts entries
    int         heuristicFlags    = 0;

    // ── System integrity (deep scan, elevated) ──
    bool        integrityChecked    = false;
    bool        integrityViolations = false;
    std::vector<std::string> integrityDetails;

    // ── Verdict ──
    int         threatScore = 100;   // 100 = clean, lower = worse
    std::string verdict     = "Clean"; // "Clean", "Suspicious", "Infected"
    std::vector<std::string> recommendations;
};

class MalwareScanner {
public:
    MalwareScanner(const AppConfig& cfg, const PlatformInfo& platform);

    // Full entry point. `depth` selects the AV-engine pass:
    //   None  → status + history + heuristics only (fast, used by Full Scan)
    //   Quick → engine quick scan of common infection points
    //   Full  → engine full sweep of the entire system (can take a while)
    MalwareResult scan(MalwareScanDepth depth = MalwareScanDepth::None,
                       std::function<void(int, const std::string&)> progress = nullptr);

private:
    void queryEngineStatus(MalwareResult& r);
    void queryThreatHistory(MalwareResult& r);
    void runEngineScan(MalwareResult& r, MalwareScanDepth depth,
                       std::function<void(int, const std::string&)> progress);
    void runHeuristics(MalwareResult& r);
    void runIntegrityCheck(MalwareResult& r);
    void computeVerdict(MalwareResult& r);

    AppConfig    cfg_;
    PlatformInfo platform_;
};

// ─────────────────────────────────────────────
//  ProcessTreeAnalyzer
// ─────────────────────────────────────────────
class ProcessTreeAnalyzer {
public:
    ProcessTreeAnalyzer(const AppConfig& cfg, const PlatformInfo& platform);
    std::vector<ProcessTreeNode> buildTree(
        std::function<void(int, const std::string&)> progress = nullptr);
    void printTree(const std::vector<ProcessTreeNode>& forest, int depth = 0);

private:
    AppConfig    cfg_;
    PlatformInfo platform_;
};

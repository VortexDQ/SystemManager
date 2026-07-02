/**
 * toolkit.h
 * Core toolkit configuration, version info, timer, thread pool,
 * benchmark types, and forward declarations.
 * Every other header in this project depends on this file.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <chrono>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <atomic>
#include <memory>

// ─────────────────────────────────────────────
//  Version
// ─────────────────────────────────────────────
#define TOOLKIT_VERSION      "3.0.0"
#define TOOLKIT_AUTHOR       "System Health Toolkit Team"
#define TOOLKIT_REPO_URL     "https://github.com/your-username/SystemHealthToolkit"
#define TOOLKIT_COPYRIGHT    "Copyright (c) 2026 System Health Toolkit. MIT License."

// ─────────────────────────────────────────────
//  Platform
// ─────────────────────────────────────────────
enum class OS { Unknown, Windows, Linux, macOS };

struct PlatformInfo {
    OS      id          = OS::Unknown;
    bool    supported   = false;
    bool    elevated    = false;
    bool    isVM        = false;
    bool    isContainer = false;
    std::string osName;
    std::string osVersion;
    std::string osBuild;
    std::string arch;
    std::string hostname;
    std::string username;
};

struct AppConfig {
    std::string baseDir;
    std::string logDir;
    std::string reportDir;
    std::string tempDir;
    std::string configDir;
    std::string assetsDir;
    bool        verbose      = false;
    bool        noColor      = false;
    bool        autoRepair   = false;
    bool        exportOnExit = true;
    bool        skipTracing  = false;
    int         threadCount  = 0; // 0 = auto-detect

    static AppConfig defaults();
    static AppConfig parseArgs(int argc, char* argv[]);
};

// ─────────────────────────────────────────────
//  Command result
// ─────────────────────────────────────────────
struct CmdResult {
    int       exitCode    = 0;
    std::string stdOut;
    std::string stdErr;
    double      durationMs = 0.0;
    bool        timedOut   = false; // process was killed after exceeding its timeout
};

// ─────────────────────────────────────────────
//  Timer (high-resolution)
// ─────────────────────────────────────────────
struct Timer {
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start = Clock::now();

    void      reset()            { start = Clock::now(); }
    double    elapsedMs()  const { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
    double    elapsedSec() const { return elapsedMs() / 1000.0; }
};

// ─────────────────────────────────────────────
//  Thread pool (parallel processing engine)
// ─────────────────────────────────────────────
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 0);
    ~ThreadPool();

    // Enqueue a task and return a future
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>
    {
        using return_type = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (stopped_) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks_.emplace([task](){ (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

    // Wait for all queued tasks to complete
    void waitAll();
    size_t threadCount() const { return workers_.size(); }
    size_t pending() const;
    bool   busy() const;

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<size_t> activeTasks_{0};
    bool stopped_ = false;
};

// ─────────────────────────────────────────────
//  Global thread pool (lazy singleton)
// ─────────────────────────────────────────────
ThreadPool& globalThreadPool();

// ─────────────────────────────────────────────
//  Health score
// ─────────────────────────────────────────────
enum class HealthCategory {
    WindowsIntegrity,
    Storage,
    Memory,
    Network,
    Security,
    Drivers,
    Startup,
    Updates,
    Hardware,
    EventLogs,
    Performance,
    Reliability
};

struct HealthScore {
    std::map<HealthCategory, int> categories;
    std::vector<std::string>      warnings;
    std::vector<std::string>      errors;
    std::vector<std::string>      recommendations;
    int    overall = 100;
    std::string grade = "A+";

    void        compute();
    std::string gradeStr();
};

// ─────────────────────────────────────────────
//  ScanResult (generic)
// ─────────────────────────────────────────────
struct ScanResult {
    std::string category;
    bool        success    = false;
    double      durationMs = 0.0;
    std::string detail;
    std::string scanType;
};

// ─────────────────────────────────────────────
//  Repair types
// ─────────────────────────────────────────────
// Named UpdateCategory (not UpdateType) because Windows' own <wuapi.h>
// declares a global `UpdateType` typedef that collides with ours when
// both headers end up in the same translation unit (as in repair.cpp).
enum class UpdateCategory {
    Unknown,
    Security,
    Quality,
    Feature,
    Driver,
    Definition,
    Optional
};

struct UpdateInfo {
    std::string kb;
    std::string title;
    UpdateCategory type     = UpdateCategory::Unknown;
    bool        rebootNeeded= false;
};

struct UpdateResult {
    std::vector<UpdateInfo> available;
    std::vector<UpdateInfo> security;
    std::vector<UpdateInfo> quality;
    std::vector<UpdateInfo> feature;
    std::vector<UpdateInfo> drivers;
    std::vector<UpdateInfo> optional;
    std::vector<std::string> failedUpdates;
    int      totalAvailable  = 0;
    bool     rebootPending   = false;
    std::string lastCheckDate;
};

struct RepairResult {
    std::string action;
    bool        success      = true;
    bool        skipped      = false;
    bool        rebootNeeded = false;
    double      durationMs   = 0.0;
    std::string output;
    std::string errorDetail;

    static RepairResult skip(const std::string& action, const std::string& reason);
    static RepairResult ok(const std::string& action, double ms, const std::string& out);
    static RepairResult fail(const std::string& action, double ms, const std::string& err);
};

// ─────────────────────────────────────────────
//  Storage types
// ─────────────────────────────────────────────
struct DriveInfo {
    std::string mountPoint;
    std::string label;
    std::string fileSystem;
    std::string driveType;
    std::string model;
    std::string serial;
    std::string firmware;
    std::string interface_;
    uint64_t totalBytes = 0;
    uint64_t freeBytes  = 0;
    uint64_t usedBytes  = 0;
    bool     smartOk    = true;
    bool     bitlocker  = false;
    // Performance metrics
    double   readSpeedMBps   = 0.0;
    double   writeSpeedMBps  = 0.0;
    double   accessTimeMs    = 0.0;
};

struct StorageResult {
    std::vector<DriveInfo> drives;
    uint64_t totalFiles   = 0;
    uint64_t totalFolders = 0;
    uint64_t hiddenFiles  = 0;
    uint64_t systemFiles  = 0;
    uint64_t readOnlyFiles= 0;
    double   avgFileSizeBytes = 0.0;
    uint64_t largestFileBytes = 0;
    std::string largestFile;
    std::string smallestFile;
    std::string longestPath;
    int      deepestFolder = 0;

    std::vector<std::pair<std::string, uint64_t>> topFiles;
    std::vector<std::pair<std::string, uint64_t>> topFolders;
    std::vector<std::pair<std::string, uint64_t>> duplicates;

    // Special paths
    uint64_t downloadsBytes     = 0;
    uint64_t desktopBytes       = 0;
    uint64_t documentsBytes     = 0;
    uint64_t picturesBytes      = 0;
    uint64_t videosBytes        = 0;
    uint64_t musicBytes         = 0;
    uint64_t tempBytes          = 0;
    uint64_t wuCacheBytes       = 0;
    uint64_t recycleBytes       = 0;
    uint64_t nodeModulesBytes   = 0;
    uint64_t dockerBytes        = 0;
    uint64_t wslBytes           = 0;
    uint64_t reclaimableBytes   = 0;
    // Fragmentation
    double   fragmentationPercent = 0.0;
    // Raw file data for parallel merge
    std::vector<std::pair<std::string, uint64_t>> allFileData;
    // Total size accumulator for parallel merge
    uint64_t totalSize = 0;
};

// ─────────────────────────────────────────────
//  Benchmark types
// ─────────────────────────────────────────────
struct DiskBenchResult {
    double sequentialReadMBps    = 0.0;
    double sequentialWriteMBps   = 0.0;
    double randomReadIOPS        = 0.0;
    double randomWriteIOPS       = 0.0;
    double averageLatencyMs      = 0.0;
    std::string drivePath;
    double      durationMs       = 0.0;
};

struct NetworkBenchResult {
    double downloadMbps     = 0.0;
    double uploadMbps       = 0.0;
    double latencyMs        = 0.0;
    double jitterMs         = 0.0;
    double packetLossPct    = 0.0;
    double durationMs       = 0.0;
};

struct SystemBenchResult {
    DiskBenchResult  disk;
    NetworkBenchResult network;
    double cpuScore          = 0.0;
    double memoryScore       = 0.0;
    double overallScore      = 0.0;
};

// ─────────────────────────────────────────────
//  Registry scan types
// ─────────────────────────────────────────────
struct RegistryIssue {
    std::string path;
    std::string key;
    std::string issue;   // "orphaned", "invalid", "broken"
    std::string details;
};

struct RegistryScanResult {
    std::vector<RegistryIssue> issues;
    int totalIssues = 0;
    int orphanedKeys = 0;
    int invalidValues = 0;
    int brokenReferences = 0;
};

// ─────────────────────────────────────────────
//  Driver info
// ─────────────────────────────────────────────
struct DriverInfo {
    std::string name;
    std::string displayName;
    std::string provider;
    std::string version;
    std::string date;
    std::string state;  // "Running", "Stopped", "Manual"
    bool signed_ = true;
    bool verified = false;
};

struct DriverScanResult {
    std::vector<DriverInfo> drivers;
    std::vector<DriverInfo> unsignedDrivers;
    std::vector<DriverInfo> problematicDrivers;
    int totalCount = 0;
    int unsignedCount = 0;
    int stoppedCritical = 0;
};

// ─────────────────────────────────────────────
//  Process tree types
// ─────────────────────────────────────────────
struct ProcessTreeNode {
    std::string name;
    int pid = 0;
    int ppid = 0;
    double cpuPercent = 0.0;
    uint64_t memoryBytes = 0;
    int threadCount = 0;
    int handleCount = 0;
    std::vector<ProcessTreeNode> children;
};
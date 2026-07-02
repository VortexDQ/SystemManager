/**
 * storage.cpp
 * Storage analyzer: drive enumeration, SMART data, top files/folders,
 * duplicate detection, special paths (Docker, WSL, Steam, etc.).
 */

#include "../include/scanners.h"
#include "../include/logger.h"
#include "../include/ui.h"

#include <algorithm>
#include <numeric>
#include <map>
#include <unordered_map>
#include <set>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <future>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#elif defined(__APPLE__)
  #include <sys/mount.h>
  #include <sys/param.h>
#else
  #include <sys/statvfs.h>
  #include <mntent.h>
#endif

namespace fs = std::filesystem;

// ─────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────
StorageAnalyzer::StorageAnalyzer(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

// ─────────────────────────────────────────────
//  Main entry point
// ─────────────────────────────────────────────
StorageResult StorageAnalyzer::analyze(bool deepScan,
                                       std::function<void(int, const std::string&)> progress) {
    StorageResult result;
    Logger::instance().info(std::string("Storage analysis started (") +
                            (deepScan ? "deep" : "quick") + ")", "storage");
    Timer total;

    if (progress) progress(5, "Enumerating drives...");
    enumerateDrives(result);

    if (progress) progress(20, "Scanning special paths...");
    detectSpecialPaths(result);

    // The full-volume file walk is expensive (can take minutes on a large
    // disk). Quick mode — used by the Health Scan — skips it and keeps
    // only drive totals + special-path sizes.
    if (deepScan && !result.drives.empty()) {
        const auto& primaryMount = result.drives[0].mountPoint;
        if (progress) progress(30, "Scanning " + primaryMount + "...");
        scanTopItems(result, primaryMount, [&](int p, const std::string& s) {
            if (progress) progress(30 + (p * 60) / 100, s);
        });
    }

    if (progress) progress(92, "Computing reclaimable space...");
    computeReclaimable(result);

    double ms = total.elapsedMs();
    if (progress) progress(100, "Storage analysis complete");
    Logger::instance().logScan("Storage Analysis", true, ms,
        "Drives=" + std::to_string(result.drives.size()) +
        " Files=" + std::to_string(result.totalFiles));
    return result;
}

// ─────────────────────────────────────────────
//  Drive enumeration
// ─────────────────────────────────────────────
void StorageAnalyzer::enumerateDrives(StorageResult& result) {
#ifdef _WIN32
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(drives & (1 << i))) continue;
        char letter = 'A' + i;
        std::string root = std::string(1, letter) + ":\\";

        UINT driveType = GetDriveTypeA(root.c_str());
        if (driveType == DRIVE_NO_ROOT_DIR) continue;

        DriveInfo di;
        di.mountPoint = root;

        ULARGE_INTEGER freeBytesAvail, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
            di.totalBytes = totalBytes.QuadPart;
            di.freeBytes  = totalFreeBytes.QuadPart;
            di.usedBytes  = di.totalBytes - di.freeBytes;
        }

        // Volume label and filesystem
        char volName[256]={}, fsName[32]={};
        DWORD serial=0, maxComp=0, flags=0;
        GetVolumeInformationA(root.c_str(), volName, sizeof(volName),
                              &serial, &maxComp, &flags, fsName, sizeof(fsName));
        di.label      = volName;
        di.fileSystem = fsName;

        // Drive type string
        switch (driveType) {
            case DRIVE_FIXED:    di.driveType = "Fixed"; break;
            case DRIVE_REMOVABLE:di.driveType = "Removable"; break;
            case DRIVE_REMOTE:   di.driveType = "Network"; break;
            case DRIVE_CDROM:    di.driveType = "Optical"; break;
            default:             di.driveType = "Unknown"; break;
        }

        result.drives.push_back(di);
    }

    // One PowerShell call enriches ALL drives with physical-disk data
    // (model, media type, SMART health) and BitLocker status. Partitions
    // map to physical disks via DiskNumber — Get-PhysicalDisk's DeviceId
    // is a disk number, never a drive letter.
    if (!result.drives.empty()) {
        auto r = Platform::exec(
            "powershell -NoProfile -Command \""
            "$pds=@{}; Get-PhysicalDisk -ErrorAction SilentlyContinue |"
            " ForEach-Object { $pds[[string]$_.DeviceId]=$_ };"
            "Get-Partition -ErrorAction SilentlyContinue | Where-Object DriveLetter |"
            " ForEach-Object { $pd=$pds[[string]$_.DiskNumber];"
            " if($pd){ 'DR='+$_.DriveLetter+'|'+$pd.Model+'|'+$pd.MediaType+'|'+$pd.HealthStatus+'|'+"
            "$pd.FirmwareVersion+'|'+$pd.SerialNumber+'|'+$pd.BusType } };"
            "Get-BitLockerVolume -ErrorAction SilentlyContinue |"
            " ForEach-Object { 'BL='+$_.MountPoint+'|'+$_.ProtectionStatus }\"", 30);

        std::istringstream iss(r.stdOut);
        std::string line;
        auto split = [](const std::string& s, char d) {
            std::vector<std::string> v; std::istringstream is2(s);
            std::string t; while (std::getline(is2, t, d)) v.push_back(t);
            return v;
        };
        auto trim = [](std::string s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
            return s;
        };
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.rfind("DR=", 0) == 0) {
                auto p = split(line.substr(3), '|');
                if (p.empty() || p[0].empty()) continue;
                char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(p[0][0])));
                for (auto& di : result.drives) {
                    if (di.mountPoint.empty() || std::toupper(static_cast<unsigned char>(di.mountPoint[0])) != letter)
                        continue;
                    if (p.size() > 1) di.model = trim(p[1]);
                    if (p.size() > 2 && !trim(p[2]).empty() && trim(p[2]) != "Unspecified")
                        di.driveType = trim(p[2]);
                    if (p.size() > 3) di.smartOk = (trim(p[3]) == "Healthy");
                    if (p.size() > 4) di.firmware = trim(p[4]);
                    if (p.size() > 5) di.serial = trim(p[5]);
                    if (p.size() > 6) di.interface_ = trim(p[6]);
                }
            } else if (line.rfind("BL=", 0) == 0) {
                auto p = split(line.substr(3), '|');
                if (p.size() < 2 || p[0].empty()) continue;
                char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(p[0][0])));
                for (auto& di : result.drives) {
                    if (!di.mountPoint.empty() &&
                        std::toupper(static_cast<unsigned char>(di.mountPoint[0])) == letter)
                        di.bitlocker = (trim(p[1]) == "On" || trim(p[1]) == "1");
                }
            }
        }
    }
#elif defined(__APPLE__)
    auto r = Platform::exec("df -k 2>/dev/null", 10);
    std::istringstream iss(r.stdOut);
    std::string line;
    std::getline(iss, line); // skip header
    while (std::getline(iss, line)) {
        std::istringstream ls(line);
        std::string fs; uint64_t total, used, avail; int cap; std::string mount;
        ls >> fs >> total >> used >> avail >> cap >> mount;
        if (fs.empty() || fs[0] != '/') continue;
        DriveInfo di;
        di.mountPoint = mount;
        di.totalBytes = total * 1024ULL;
        di.freeBytes  = avail * 1024ULL;
        di.usedBytes  = used  * 1024ULL;
        di.fileSystem = "APFS";
        di.driveType  = "SSD";
        result.drives.push_back(di);
    }
#else
    // Linux: parse /proc/mounts
    std::ifstream mounts("/proc/mounts");
    std::string line;
    std::set<std::string> seen;
    while (std::getline(mounts, line)) {
        std::istringstream ls(line);
        std::string dev, mp, fstype;
        ls >> dev >> mp >> fstype;
        if (mp.empty() || seen.count(mp)) continue;
        if (fstype == "tmpfs" || fstype == "devtmpfs" ||
            fstype == "sysfs" || fstype == "proc"    ||
            fstype == "cgroup"|| fstype == "pstore"  ||
            fstype == "efivarfs") continue;
        seen.insert(mp);

        DriveInfo di;
        di.mountPoint = mp;
        di.fileSystem = fstype;

        struct statvfs vfs{};
        if (statvfs(mp.c_str(), &vfs) == 0) {
            di.totalBytes = (uint64_t)vfs.f_blocks * vfs.f_frsize;
            di.freeBytes  = (uint64_t)vfs.f_bfree  * vfs.f_frsize;
            di.usedBytes  = di.totalBytes - di.freeBytes;
        }

        // Guess drive type from device name
        if (dev.find("nvme") != std::string::npos) {
            di.driveType  = "NVMe"; di.interface_ = "NVMe";
        } else if (dev.find("sd") != std::string::npos) {
            di.driveType  = "HDD/SSD"; di.interface_ = "SATA";
        } else if (dev.find("mmcblk") != std::string::npos) {
            di.driveType  = "eMMC";
        } else {
            di.driveType  = "Virtual/Unknown";
        }

        // Try to get model from sysfs
        std::string devName = dev.substr(dev.rfind('/') + 1);
        // Strip partition number (sda1 -> sda)
        while (!devName.empty() && std::isdigit(devName.back())) devName.pop_back();
        std::string modelPath = "/sys/block/" + devName + "/device/model";
        std::ifstream mf(modelPath);
        if (mf.is_open()) {
            std::getline(mf, di.model);
            // trim
            di.model.erase(di.model.find_last_not_of(" \n\r\t") + 1);
        }

        result.drives.push_back(di);
    }
#endif
}

// ─────────────────────────────────────────────
//  Parallel directory scanner worker
//  Splits directory tree into chunks for parallel processing
// ─────────────────────────────────────────────
struct ParallelScanChunk {
    std::vector<std::pair<std::string, uint64_t>> files; // only files >= 1 MB
    std::unordered_map<std::string, uint64_t> dirSizes;
    uint64_t totalSize = 0;
    uint64_t totalFiles = 0;
    uint64_t totalFolders = 0;
    uint64_t hiddenFiles = 0;
    uint64_t readOnlyFiles = 0;
    uint64_t systemFiles = 0;
    uint64_t smallFiles = 0;
    uint64_t largestFileBytes = 0;
    std::string largestFile;
    int deepestFolder = 0;
};

// Only files at least this large are recorded individually. Top-100 lists
// and duplicate detection only care about big files, and keeping every
// path of a multi-million-file volume in RAM can exhaust memory.
static constexpr uint64_t kRecordFileThreshold = 1024 * 1024;

// Scans a single top-level directory and returns its own chunk. Progress
// is batched locally and flushed to the shared atomic every 256 entries
// instead of once per entry, which matters once several threads are all
// hammering the same cache line on a fast NVMe scan.
static ParallelScanChunk scanChunk(const std::string& rootDir,
                                    std::atomic<uint64_t>& globalProgress) {
    ParallelScanChunk chunk;
    std::error_code ec;
    if (!std::filesystem::exists(rootDir, ec)) return chunk;

    fs::recursive_directory_iterator it(rootDir,
        fs::directory_options::skip_permission_denied, ec);
    if (ec) return chunk;
    fs::recursive_directory_iterator end;

    uint64_t localProgress = 0;
    while (it != end) {
        ++localProgress;
        if ((localProgress & 0xFF) == 0)
            globalProgress.fetch_add(256, std::memory_order_relaxed);

        // Everything per-entry is guarded: path narrowing can throw for
        // names that don't fit the ANSI code page, and one bad entry must
        // never abort the whole volume scan.
        try {
            std::error_code fec;
            auto path = it->path().string();

            int depth = static_cast<int>(
                std::count(path.begin(), path.end(), fs::path::preferred_separator));
            if (depth > chunk.deepestFolder) chunk.deepestFolder = depth;

            if (it->is_directory(fec)) {
                ++chunk.totalFolders;
            } else if (it->is_regular_file(fec)) {
                ++chunk.totalFiles;
                uint64_t sz = it->file_size(fec);
                if (fec) sz = 0;
                chunk.totalSize += sz;
                if (sz < 4096) ++chunk.smallFiles;

                if (sz > chunk.largestFileBytes) {
                    chunk.largestFileBytes = sz;
                    chunk.largestFile = path;
                }

                chunk.dirSizes[it->path().parent_path().string()] += sz;

#ifdef _WIN32
                DWORD attrs = GetFileAttributesA(path.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES) {
                    if (attrs & FILE_ATTRIBUTE_HIDDEN)   ++chunk.hiddenFiles;
                    if (attrs & FILE_ATTRIBUTE_READONLY) ++chunk.readOnlyFiles;
                    if (attrs & FILE_ATTRIBUTE_SYSTEM)   ++chunk.systemFiles;
                }
#else
                std::string fname = it->path().filename().string();
                if (!fname.empty() && fname[0] == '.') ++chunk.hiddenFiles;
#endif
                if (sz >= kRecordFileThreshold)
                    chunk.files.emplace_back(std::move(path), sz);
            }
        } catch (...) {
            // Unconvertible name or transient FS error — skip the entry.
        }

        it.increment(ec); // non-throwing advance
        if (ec) { ec.clear(); break; }
    }
    globalProgress.fetch_add(localProgress & 0xFF, std::memory_order_relaxed);
    return chunk;
}

// ─────────────────────────────────────────────
//  Scan top files/folders + file statistics (parallel optimized)
// ─────────────────────────────────────────────
void StorageAnalyzer::scanTopItems(StorageResult& result, const std::string& root,
                                    std::function<void(int, const std::string&)> progress) {
    Logger::instance().info("Parallel scanning " + root, "storage");

    if (progress) progress(5, "Discovering directory structure...");

    // Get top-level directories to distribute to threads
    std::vector<std::string> topDirs;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(root,
             fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_directory(ec))
            topDirs.push_back(entry.path().string());
    }

    // If few directories, include the root itself
    if (topDirs.empty()) topDirs.push_back(root);

    auto& pool = globalThreadPool();
    if (pool.threadCount() < 2 || topDirs.size() < 2) {
        // Fall back to single-threaded if there's no real parallelism to exploit
        return scanTopItemsSingleThreaded(result, root, progress);
    }

    std::atomic<uint64_t> globalProgress{0};
    Timer scanTimer;

    // One task per top-level directory instead of a fixed static split:
    // the pool's queue naturally load-balances, so one oversized folder
    // (e.g. C:\Windows) doesn't leave other threads sitting idle while a
    // single thread churns through it alone.
    std::vector<std::future<ParallelScanChunk>> futures;
    futures.reserve(topDirs.size());
    for (auto& dir : topDirs) {
        futures.push_back(pool.enqueue([dir, &globalProgress]() {
            return scanChunk(dir, globalProgress);
        }));
    }

    // Progress monitoring. Stop at the first not-yet-ready future each
    // tick rather than polling every future with its own 100ms wait --
    // with many more tasks than threads that would otherwise turn every
    // tick into "num_pending * 100ms".
    while (true) {
        bool allDone = true;
        for (auto& f : futures) {
            if (f.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
                allDone = false;
                break;
            }
        }
        uint64_t scanned = globalProgress.load(std::memory_order_relaxed);
        if (progress && scanned > 0) {
            int pct = std::min(80, static_cast<int>(scanned / 500));
            progress(pct, "Scanning " + std::to_string(scanned) + " items (" +
                     std::to_string(pool.threadCount()) + " threads)...");
        }
        if (allDone) break;
    }

    std::vector<ParallelScanChunk> chunks;
    chunks.reserve(futures.size());
    for (auto& f : futures) chunks.push_back(f.get());

    // Merge results from all chunks
    result.totalFolders = 0;
    result.totalFiles = 0;
    result.hiddenFiles = 0;
    result.readOnlyFiles = 0;
    result.systemFiles = 0;
    result.largestFileBytes = 0;
    int deepestFolder = 0;
    uint64_t smallFiles = 0;

    std::unordered_map<std::string, uint64_t> mergedDirSizes;
    // size -> indices into result.allFileData, used for O(n) duplicate detection
    std::unordered_map<uint64_t, std::vector<size_t>> sizeGroups;

    for (auto& chunk : chunks) {
        result.totalFolders += chunk.totalFolders;
        result.totalFiles += chunk.totalFiles;
        result.hiddenFiles += chunk.hiddenFiles;
        result.readOnlyFiles += chunk.readOnlyFiles;
        result.systemFiles += chunk.systemFiles;
        result.totalSize += chunk.totalSize;
        smallFiles += chunk.smallFiles;

        if (chunk.largestFileBytes > result.largestFileBytes) {
            result.largestFileBytes = chunk.largestFileBytes;
            result.largestFile = chunk.largestFile;
        }
        if (chunk.deepestFolder > deepestFolder) deepestFolder = chunk.deepestFolder;

        for (auto& [dir, sz] : chunk.dirSizes)
            mergedDirSizes[dir] += sz;
        for (auto& [path, sz] : chunk.files) {
            result.allFileData.emplace_back(path, sz);
            sizeGroups[sz].push_back(result.allFileData.size() - 1);
        }
    }

    result.deepestFolder = deepestFolder;

    if (result.totalFiles > 0)
        result.avgFileSizeBytes = static_cast<double>(result.totalSize) / result.totalFiles;

    if (progress) progress(85, "Sorting top items...");

    // Top 100 largest files (from merged data)
    auto& fileData = result.allFileData;
    std::partial_sort(fileData.begin(),
        fileData.begin() + std::min((size_t)100, fileData.size()),
        fileData.end(),
        [](const auto& a, const auto& b){ return a.second > b.second; });
    result.topFiles.assign(fileData.begin(),
        fileData.begin() + std::min((size_t)100, fileData.size()));

    // Top 100 largest directories
    std::vector<std::pair<std::string, uint64_t>> dirPairs(
        mergedDirSizes.begin(), mergedDirSizes.end());
    std::partial_sort(dirPairs.begin(),
        dirPairs.begin() + std::min((size_t)100, dirPairs.size()),
        dirPairs.end(),
        [](const auto& a, const auto& b){ return a.second > b.second; });
    result.topFolders.assign(dirPairs.begin(),
        dirPairs.begin() + std::min((size_t)100, dirPairs.size()));

    // Duplicate detection: files with same size (> 1 MB, multiple files).
    // Single pass over the pre-grouped size buckets instead of rescanning
    // the full file list once per duplicate-size group.
    for (auto& [sz, indices] : sizeGroups) {
        if (indices.size() > 1 && sz > 1024 * 1024) {
            for (size_t idx : indices)
                result.duplicates.emplace_back(fileData[idx].first, sz);
        }
    }

    // Estimate fragmentation (ratio of small files to total)
    if (result.totalFiles > 0)
        result.fragmentationPercent = 100.0 * smallFiles / result.totalFiles;

    if (progress) progress(95, "Finalizing merged results...");
}

// ─────────────────────────────────────────────
//  Single-threaded fallback scanner
// ─────────────────────────────────────────────
void StorageAnalyzer::scanTopItemsSingleThreaded(StorageResult& result,
    const std::string& root,
    std::function<void(int, const std::string&)> progress) {
    struct FileInfo {
        std::string path;
        uint64_t    size;
        bool        hidden    = false;
        bool        readOnly  = false;
        bool        system    = false;
    };

    std::vector<FileInfo> allFiles;
    std::unordered_map<std::string, uint64_t> dirSizes;
    // size -> indices into allFiles, used for O(n) duplicate detection
    std::unordered_map<uint64_t, std::vector<size_t>> sizeGroups;
    uint64_t totalSize = 0;
    std::string longestPath;
    uint64_t largestFileBytes = 0;
    std::string largestFile;
    uint64_t smallestFileBytes = UINT64_MAX;
    int deepestFolder = 0;

    std::error_code ec;
    fs::recursive_directory_iterator it(root,
        fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    if (ec) return;

    uint64_t smallFiles = 0;
    int scanned = 0;
    while (it != end) {
        ++scanned;
        if (scanned % 5000 == 0 && progress)
            progress(std::min(90, 30 + scanned / 500), "Scanned " + std::to_string(scanned) + " items...");

        try {
            std::error_code fec;
            auto path = it->path().string();
            int depth = static_cast<int>(std::count(path.begin(), path.end(), '\\'));
            if (depth > deepestFolder) deepestFolder = depth;
            if (path.size() > longestPath.size()) longestPath = path;

            if (it->is_directory(fec)) {
                ++result.totalFolders;
            } else if (it->is_regular_file(fec)) {
                ++result.totalFiles;
                uint64_t sz = it->file_size(fec);
                if (fec) sz = 0;
                totalSize += sz;
                if (sz < 4096) ++smallFiles;
                if (sz > largestFileBytes) { largestFileBytes = sz; largestFile = path; }

                dirSizes[it->path().parent_path().string()] += sz;

                FileInfo fi{path, sz};
#ifdef _WIN32
                DWORD attrs = GetFileAttributesA(path.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES) {
                    fi.hidden   = (attrs & FILE_ATTRIBUTE_HIDDEN)   != 0;
                    fi.readOnly = (attrs & FILE_ATTRIBUTE_READONLY) != 0;
                    fi.system   = (attrs & FILE_ATTRIBUTE_SYSTEM)   != 0;
                }
#else
                std::string fname = it->path().filename().string();
                fi.hidden = (!fname.empty() && fname[0] == '.');
#endif
                if (fi.hidden)   ++result.hiddenFiles;
                if (fi.readOnly) ++result.readOnlyFiles;
                if (fi.system)   ++result.systemFiles;
                // Only record large files individually (memory bound; the
                // top-100 and duplicate passes only look at files >= 1 MB).
                if (sz >= kRecordFileThreshold) {
                    sizeGroups[sz].push_back(allFiles.size());
                    allFiles.push_back(std::move(fi));
                }
            }
        } catch (...) {
            // Unconvertible name or transient FS error — skip the entry.
        }

        it.increment(ec);
        if (ec) { ec.clear(); break; }
    }

    result.largestFile = largestFile;
    result.largestFileBytes = largestFileBytes;
    result.deepestFolder = deepestFolder;
    result.longestPath = longestPath;
    if (result.totalFiles > 0) result.avgFileSizeBytes = static_cast<double>(totalSize) / result.totalFiles;

    // Build file data for main.cpp access
    for (auto& f : allFiles) result.allFileData.emplace_back(f.path, f.size);

    // Top 100 files
    std::partial_sort(result.allFileData.begin(),
        result.allFileData.begin() + std::min((size_t)100, result.allFileData.size()),
        result.allFileData.end(),
        [](const auto& a, const auto& b){ return a.second > b.second; });
    result.topFiles.assign(result.allFileData.begin(),
        result.allFileData.begin() + std::min((size_t)100, result.allFileData.size()));

    // Top 100 dirs
    std::vector<std::pair<std::string, uint64_t>> dirPairs(dirSizes.begin(), dirSizes.end());
    std::partial_sort(dirPairs.begin(),
        dirPairs.begin() + std::min((size_t)100, dirPairs.size()), dirPairs.end(),
        [](const auto& a, const auto& b){ return a.second > b.second; });
    result.topFolders.assign(dirPairs.begin(),
        dirPairs.begin() + std::min((size_t)100, dirPairs.size()));

    // Duplicates: single pass over the pre-grouped size buckets instead
    // of rescanning the full file list once per duplicate-size group.
    for (auto& [sz, indices] : sizeGroups) {
        if (indices.size() > 1 && sz > 1024 * 1024) {
            for (size_t idx : indices)
                result.duplicates.emplace_back(allFiles[idx].path, sz);
        }
    }

    // Fragmentation estimate
    if (result.totalFiles > 0) result.fragmentationPercent = 100.0 * smallFiles / result.totalFiles;
}

// ─────────────────────────────────────────────
//  Detect special paths
// ─────────────────────────────────────────────
void StorageAnalyzer::detectSpecialPaths(StorageResult& result) {
    std::string home = Platform::getHomePath();

    auto safeSize = [](const std::string& p) -> uint64_t {
        if (!Platform::dirExists(p)) return 0;
        return Platform::dirSize(p);
    };

    // Each of these is an independent recursive directory walk;
    // running them one after another serializes I/O that has no
    // reason to be serialized. Fan them out on the shared pool and
    // join at the end instead.
    auto& pool = globalThreadPool();

#ifdef _WIN32
    auto fDownloads = pool.enqueue(safeSize, home + "\\Downloads");
    auto fDesktop   = pool.enqueue(safeSize, home + "\\Desktop");
    auto fDocuments = pool.enqueue(safeSize, home + "\\Documents");
    auto fPictures  = pool.enqueue(safeSize, home + "\\Pictures");
    auto fVideos    = pool.enqueue(safeSize, home + "\\Videos");
    auto fMusic     = pool.enqueue(safeSize, home + "\\Music");
    auto fTemp      = pool.enqueue(safeSize, Platform::getTempPath());
    auto fWuCache   = pool.enqueue(safeSize, std::string("C:\\Windows\\SoftwareDistribution\\Download"));
    auto fRecycle   = pool.enqueue(safeSize, std::string("C:\\$Recycle.Bin"));

    std::string dockerPath = "C:\\ProgramData\\Docker";
    auto fDocker = pool.enqueue([safeSize, dockerPath]() -> uint64_t {
        return Platform::dirExists(dockerPath) ? safeSize(dockerPath) : 0;
    });

    std::string wslPath = home + "\\AppData\\Local\\Packages";
    auto fWsl = pool.enqueue([safeSize, wslPath]() -> uint64_t {
        uint64_t total = 0;
        if (Platform::dirExists(wslPath)) {
            auto entries = Platform::listDir(wslPath);
            for (auto& e : entries) {
                if (e.find("CanonicalGroup") != std::string::npos ||
                    e.find("Kali")          != std::string::npos ||
                    e.find("Ubuntu")        != std::string::npos)
                    total += safeSize(e);
            }
        }
        return total;
    });

    // Game launchers / special dirs
    std::vector<std::string> steamPaths = {
        "C:\\Program Files (x86)\\Steam\\steamapps",
        "C:\\Program Files\\Steam\\steamapps",
        home + "\\AppData\\Local\\Steam\\steamapps"
    };
    for (auto& p : steamPaths)
        if (Platform::dirExists(p)) { /* could sum up */ break; }

    result.downloadsBytes   = fDownloads.get();
    result.desktopBytes     = fDesktop.get();
    result.documentsBytes   = fDocuments.get();
    result.picturesBytes    = fPictures.get();
    result.videosBytes      = fVideos.get();
    result.musicBytes       = fMusic.get();
    result.tempBytes        = fTemp.get();
    result.wuCacheBytes     = fWuCache.get();
    result.recycleBytes     = fRecycle.get();
    result.dockerBytes      = fDocker.get();
    result.wslBytes         = fWsl.get();
    result.nodeModulesBytes = 0;
#else
    auto fDownloads = pool.enqueue(safeSize, home + "/Downloads");
    auto fDesktop   = pool.enqueue(safeSize, home + "/Desktop");
    auto fDocuments = pool.enqueue(safeSize, home + "/Documents");
    auto fPictures  = pool.enqueue(safeSize, home + "/Pictures");
    auto fVideos    = pool.enqueue(safeSize, home + "/Videos");
    auto fMusic     = pool.enqueue(safeSize, home + "/Music");
    auto fTemp      = pool.enqueue(safeSize, std::string("/tmp"));

  #ifdef __APPLE__
    auto fRecycle = pool.enqueue(safeSize, home + "/.Trash");
    auto fDocker  = pool.enqueue(safeSize, home + "/Library/Containers/com.docker.docker");
  #else
    auto fRecycle = pool.enqueue(safeSize, home + "/.local/share/Trash");
    auto fDocker  = pool.enqueue(safeSize, std::string("/var/lib/docker"));
  #endif
    auto fNodeModules = pool.enqueue(safeSize, home + "/node_modules");

    result.downloadsBytes   = fDownloads.get();
    result.desktopBytes     = fDesktop.get();
    result.documentsBytes   = fDocuments.get();
    result.picturesBytes    = fPictures.get();
    result.videosBytes      = fVideos.get();
    result.musicBytes       = fMusic.get();
    result.tempBytes        = fTemp.get();
    result.recycleBytes     = fRecycle.get();
    result.dockerBytes      = fDocker.get();
    result.nodeModulesBytes = fNodeModules.get();
#endif
}

// ─────────────────────────────────────────────
//  Reclaimable space estimate
// ─────────────────────────────────────────────
void StorageAnalyzer::computeReclaimable(StorageResult& result) {
    result.reclaimableBytes =
        result.tempBytes +
        result.recycleBytes +
        result.wuCacheBytes;
    // Add obvious duplicate storage (rough estimate)
    uint64_t dupBytes = 0;
    for (auto& [p, sz] : result.duplicates) dupBytes += sz;
    result.reclaimableBytes += dupBytes / 2; // one copy is needed, half is waste
}

// ─────────────────────────────────────────────
//  ScanDrive (single-drive scan entry point)
// ─────────────────────────────────────────────
ScanResult StorageAnalyzer::scanDrive(const std::string& root,
    std::function<void(int, const std::string&)> progress) {
    ScanResult sr;
    sr.category = "storage";
    Timer t;
    StorageResult tmp;
    scanTopItems(tmp, root, progress);
    sr.durationMs = t.elapsedMs();
    sr.success    = true;
    sr.detail     = "Scanned " + root + ": " +
                    std::to_string(tmp.totalFiles) + " files, " +
                    Platform::formatBytes(tmp.largestFileBytes) + " largest";
    return sr;
}
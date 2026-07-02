/**
 * benchmark.cpp
 * System benchmark suite: disk speed test, network speed test,
 * CPU performance scoring, memory bandwidth measurement.
 * Uses parallel processing via thread pool for maximum throughput.
 */

#include "../include/scanners.h"
#include "../include/logger.h"
#include "../include/ui.h"

#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/time.h>
  #include <sys/resource.h>
#endif

// ─────────────────────────────────────────────
//  DiskBenchmark
// ─────────────────────────────────────────────
DiskBenchmark::DiskBenchmark(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

DiskBenchResult DiskBenchmark::run(const std::string& drivePath,
    std::function<void(int, const std::string&)> progress) {
    DiskBenchResult result;
    result.drivePath = drivePath;
    Logger::instance().info("Disk benchmark started: " + drivePath, "bench");
    Timer total;

    // Use a temporary file in the target drive
    std::string tempFile = drivePath + "/.sht_bench.tmp";

    // ── Sequential Write ──────────────────────
    if (progress) progress(10, "Testing sequential write speed...");
    {
        Timer t;
        constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024; // 4 MB
        constexpr size_t TOTAL_SIZE  = 512 * 1024 * 1024; // 512 MB
        std::vector<char> buffer(BLOCK_SIZE, 'A');
        // Fill with random-ish data
        for (size_t i = 0; i < buffer.size(); ++i)
            buffer[i] = static_cast<char>((i * 257) % 256);

        std::ofstream f(tempFile, std::ios::binary | std::ios::trunc);
        if (f.is_open()) {
            size_t written = 0;
            while (written < TOTAL_SIZE) {
                f.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                written += BLOCK_SIZE;
            }
            f.close();
            double secs = t.elapsedSec();
            if (secs > 0) result.sequentialWriteMBps = (TOTAL_SIZE / (1024.0 * 1024.0)) / secs;
        }
    }

    // ── Sequential Read ───────────────────────
    if (progress) progress(30, "Testing sequential read speed...");
    {
        Timer t;
        constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024;
        std::vector<char> buffer(BLOCK_SIZE);

        std::ifstream f(tempFile, std::ios::binary);
        if (f.is_open()) {
            size_t totalRead = 0;
            while (f.read(buffer.data(), BLOCK_SIZE)) {
                totalRead += static_cast<size_t>(f.gcount());
            }
            totalRead += static_cast<size_t>(f.gcount());
            f.close();
            double secs = t.elapsedSec();
            if (secs > 0) result.sequentialReadMBps = (totalRead / (1024.0 * 1024.0)) / secs;
        }
    }

    // ── Random Read IOPS ──────────────────────
    if (progress) progress(50, "Testing random read IOPS...");
    {
        Timer t;
        constexpr size_t BLOCK_SIZE = 4096; // 4 KB
        constexpr int NUM_OPS = 2000;
        std::vector<char> buffer(BLOCK_SIZE);
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, (512 * 1024 * 1024 / BLOCK_SIZE) - 1);

        std::ifstream f(tempFile, std::ios::binary);
        if (f.is_open()) {
            int completed = 0;
            for (int i = 0; i < NUM_OPS; ++i) {
                size_t offset = dist(rng) * BLOCK_SIZE;
                f.seekg(static_cast<std::streamoff>(offset));
                if (f.read(buffer.data(), BLOCK_SIZE)) ++completed;
            }
            f.close();
            double secs = t.elapsedSec();
            if (secs > 0) result.randomReadIOPS = completed / secs;
            result.averageLatencyMs = (secs / completed) * 1000.0;
        }
    }

    // ── Random Write IOPS ─────────────────────
    if (progress) progress(70, "Testing random write IOPS...");
    {
        Timer t;
        constexpr size_t BLOCK_SIZE = 4096;
        constexpr int NUM_OPS = 1000;
        std::vector<char> buffer(BLOCK_SIZE, 'B');
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, (512 * 1024 * 1024 / BLOCK_SIZE) - 1);

        std::fstream f(tempFile, std::ios::binary | std::ios::in | std::ios::out);
        if (f.is_open()) {
            int completed = 0;
            for (int i = 0; i < NUM_OPS; ++i) {
                size_t offset = dist(rng) * BLOCK_SIZE;
                f.seekp(static_cast<std::streamoff>(offset));
                f.write(buffer.data(), BLOCK_SIZE);
                if (f.good()) ++completed;
            }
            f.close();
            double secs = t.elapsedSec();
            if (secs > 0) result.randomWriteIOPS = completed / secs;
        }
    }

    // Cleanup
    std::error_code ec;
    std::filesystem::remove(tempFile, ec);

    result.durationMs = total.elapsedMs();
    if (progress) progress(100, "Disk benchmark complete");

    Logger::instance().logScan("Disk Benchmark", true, result.durationMs,
        "SeqRead=" + std::to_string((int)result.sequentialReadMBps) +
        "MB/s SeqWrite=" + std::to_string((int)result.sequentialWriteMBps) +
        "MB/s RandIOPS=" + std::to_string((int)result.randomReadIOPS));
    return result;
}

// ─────────────────────────────────────────────
//  NetworkBenchmark
// ─────────────────────────────────────────────
NetworkBenchmark::NetworkBenchmark(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

NetworkBenchResult NetworkBenchmark::run(std::function<void(int, const std::string&)> progress) {
    NetworkBenchResult result;
    Logger::instance().info("Network benchmark started", "bench");
    Timer total;

    // ── Latency (ping multiple targets) ───────
    if (progress) progress(10, "Measuring network latency...");
    {
        std::vector<double> pings;
        std::vector<std::string> targets = {"8.8.8.8", "1.1.1.1", "208.67.222.222"};

        for (auto& target : targets) {
#ifdef _WIN32
            auto r = Platform::exec("ping -n 2 " + target + " 2>nul | findstr \"Average\"", 15);
#else
            auto r = Platform::exec("ping -c 2 " + target + " 2>/dev/null | tail -1", 15);
#endif
            if (!r.stdOut.empty()) {
                // Parse ping time
                std::string s = r.stdOut;
                auto pos = s.find('=');
                auto end = s.find("ms", pos);
                if (pos != std::string::npos && end != std::string::npos) {
                    try {
                        double ping = std::stod(s.substr(pos + 1, end - pos - 1));
                        pings.push_back(ping);
                    } catch (...) {}
                }
            }
        }

        if (!pings.empty()) {
            double sum = 0;
            for (auto p : pings) sum += p;
            result.latencyMs = sum / pings.size();

            // Jitter = standard deviation
            double mean = result.latencyMs;
            double sqSum = 0;
            for (auto p : pings) sqSum += (p - mean) * (p - mean);
            result.jitterMs = std::sqrt(sqSum / pings.size());
        }
    }

    // ── Download Speed ────────────────────────
    if (progress) progress(40, "Testing download speed...");
    {
        Timer t;
        constexpr size_t TARGET_SIZE = 10 * 1024 * 1024; // 10 MB
        // Use a known fast CDN file
#ifdef _WIN32
        auto r = Platform::exec(
            "powershell -NoProfile -Command \""
            "$wc = New-Object System.Net.WebClient; "
            "$data = $wc.DownloadData('https://speedtest.tele2.net/10MB.zip'); "
            "Write-Output $data.Length\"", 30);
#else
        auto r = Platform::exec("curl -s -o /dev/null -w '%{speed_download}' https://speedtest.tele2.net/10MB.zip 2>/dev/null || "
                                "wget -q -O /dev/null --timeout=10 https://speedtest.tele2.net/10MB.zip 2>&1", 30);
#endif
        double secs = t.elapsedSec();
        if (secs > 0) {
#ifdef _WIN32
            // Parse bytes downloaded
            if (!r.stdOut.empty()) {
                try {
                    size_t bytes = std::stoull(r.stdOut);
                    result.downloadMbps = (bytes * 8.0 / (1024.0 * 1024.0)) / secs;
                } catch (...) {}
            }
#else
            // curl outputs speed in bytes/sec
            if (!r.stdOut.empty()) {
                try {
                    double bytesPerSec = std::stod(r.stdOut);
                    result.downloadMbps = bytesPerSec * 8.0 / (1024.0 * 1024.0);
                } catch (...) {
                    // wget fallback
                    result.downloadMbps = (10.0 * 8.0) / secs; // 10 MB in secs
                }
            }
#endif
        }
    }

    // ── Packet Loss ───────────────────────────
    if (progress) progress(70, "Measuring packet loss...");
    {
#ifdef _WIN32
        auto r = Platform::exec("ping -n 10 8.8.8.8 2>nul | findstr \"Lost\"", 20);
#else
        auto r = Platform::exec("ping -c 10 8.8.8.8 2>/dev/null | grep -oP '\\d+(?=% loss)'", 20);
#endif
        if (!r.stdOut.empty()) {
            try {
                result.packetLossPct = std::stod(r.stdOut);
            } catch (...) {}
        }
    }

    result.durationMs = total.elapsedMs();
    if (progress) progress(100, "Network benchmark complete");

    Logger::instance().logScan("Network Benchmark", true, result.durationMs,
        "Latency=" + std::to_string((int)result.latencyMs) +
        "ms Download=" + std::to_string(result.downloadMbps) + "Mbps");
    return result;
}

// ─────────────────────────────────────────────
//  SystemBenchmark
// ─────────────────────────────────────────────
SystemBenchmark::SystemBenchmark(const AppConfig& cfg, const PlatformInfo& platform)
    : cfg_(cfg), platform_(platform) {}

SystemBenchResult SystemBenchmark::runAll(std::function<void(int, const std::string&)> progress) {
    SystemBenchResult result;
    Logger::instance().info("System benchmark started", "bench");

    // Run disk + network benchmarks in parallel via thread pool
    auto& pool = globalThreadPool();

    auto diskFuture = pool.enqueue([&]() -> DiskBenchResult {
        std::string root;
#ifdef _WIN32
        root = "C:";
#else
        root = "/tmp";
#endif
        return DiskBenchmark(cfg_, platform_).run(root);
    });

    auto netFuture = pool.enqueue([&]() -> NetworkBenchResult {
        return NetworkBenchmark(cfg_, platform_).run();
    });

    // CPU score (simple prime number computation)
    if (progress) progress(5, "Computing CPU score...");
    {
        Timer t;
        constexpr int PRIME_LIMIT = 500000;
        std::atomic<int> primeCount{0};
        // Parallel prime counting
        auto& cpuPool = globalThreadPool();
        int numThreads = static_cast<int>(cpuPool.threadCount());
        std::vector<std::future<int>> futures;

        int chunkSize = PRIME_LIMIT / numThreads;
        for (int t = 0; t < numThreads; ++t) {
            int start = t * chunkSize;
            int end = (t == numThreads - 1) ? PRIME_LIMIT : (t + 1) * chunkSize;
            futures.push_back(cpuPool.enqueue([start, end]() -> int {
                int count = 0;
                for (int n = start; n < end; ++n) {
                    if (n < 2) continue;
                    bool prime = true;
                    for (int d = 2; d * d <= n; ++d) {
                        if (n % d == 0) { prime = false; break; }
                    }
                    if (prime) ++count;
                }
                return count;
            }));
        }
        for (auto& f : futures) primeCount += f.get();
        double secs = t.elapsedSec();
        result.cpuScore = secs > 0 ? primeCount / secs / 1000.0 : 0;
    }

    // Memory score (bandwidth via buffer copy)
    if (progress) progress(50, "Measuring memory bandwidth...");
    {
        Timer t;
        constexpr size_t BUF_SIZE = 256 * 1024 * 1024; // 256 MB
        std::vector<char> src(BUF_SIZE, 'X');
        std::vector<char> dst(BUF_SIZE, 'Y');

        // Sequential copy
        memcpy(dst.data(), src.data(), BUF_SIZE);

        double secs = t.elapsedSec();
        double bw = secs > 0 ? (BUF_SIZE / (1024.0 * 1024.0)) / secs : 0;
        result.memoryScore = bw; // MB/s
    }

    // Collect async results
    if (progress) progress(80, "Collecting benchmark results...");
    result.disk = diskFuture.get();
    result.network = netFuture.get();

    // Compute overall score (0-100)
    double diskScore = std::min(100.0, result.disk.sequentialReadMBps / 50.0 * 100.0);
    double netScore  = std::min(100.0, result.network.downloadMbps / 10.0 * 100.0);
    double cpuScore  = std::min(100.0, result.cpuScore / 10.0 * 100.0);
    double memScore  = std::min(100.0, result.memoryScore / 100.0 * 100.0);
    result.overallScore = (diskScore + netScore + cpuScore + memScore) / 4.0;

    if (progress) progress(100, "Benchmark complete");
    Logger::instance().info("Benchmark complete. Overall: " + std::to_string((int)result.overallScore), "bench");
    return result;
}
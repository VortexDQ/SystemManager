/**
 * main.cpp
 * System Health Toolkit — Entry point with full interactive menu (12 options),
 * auto-elevation, logging init, scanning engines, repair pipelines,
 * export system, and health scoring.
 *
 * Compile: g++ -std=c++20 -O2 -pthread src/*.cpp -o SystemHealthToolkit
 */

#include "../include/toolkit.h"
#include "../include/logger.h"
#include "../include/platform.h"
#include "../include/ui.h"
#include "../include/repair.h"
#include "../include/scanners.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────
//  Global state
// ─────────────────────────────────────────────
static AppConfig     g_config;
static PlatformInfo  g_platform;
static std::string   g_sessionId;

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
static void menuMain();
static void optionFullScan();
static void optionHealthScan();
static void optionMalwareScan();
static void optionRepairWindows();
static void optionStartupAnalysis();
static void optionStorageAnalyzer();
static void optionNetworkDiagnostics();
static void optionHardwareReport();
static void optionWindowsUpdate();
static void optionExportReport();
static void optionAdvancedTools();
static void renderMalwareResult(const MalwareResult& mr);

// ─────────────────────────────────────────────
//  Scanner exception guard
// ─────────────────────────────────────────────
// Convert any exception thrown by a scanner into a default-constructed
// result plus a logged error. Scanners run on std::async threads; an
// escaped exception would otherwise re-throw from future::get() and
// terminate the whole application.
template <typename Fn>
static auto guarded(const char* name, Fn fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const std::exception& e) {
        Logger::instance().error(std::string(name) + " scanner failed: " + e.what(), "scan");
    } catch (...) {
        Logger::instance().error(std::string(name) + " scanner failed with unknown error", "scan");
    }
    return decltype(fn()){};
}

// ─────────────────────────────────────────────
//  Export subsystem
// ─────────────────────────────────────────────
static bool ensureReportDir() {
    std::error_code ec;
    fs::create_directories(g_config.reportDir, ec);
    return !ec;
}

static std::string uniqueReportName(const std::string& ext) {
    return g_config.reportDir + "/SystemHealth_" + g_sessionId + "." + ext;
}

static void exportTxt(const HealthScore& score,
                      const StorageResult& sr,
                      const StartupResult& start,
                      const HardwareResult& hw,
                      const NetworkResult& net,
                      const EventLogSummary& evt,
                      const PerformanceResult& perf,
                      const ReliabilityResult& rel,
                      const WindowsInfoResult& winfo,
                      const CorruptionResult& corr,
                      const RepairSession& repSession) {
    if (!ensureReportDir()) return;
    auto path = uniqueReportName("txt");
    std::ofstream f(path);
    if (!f.is_open()) { UI::printError("Failed to create TXT report: " + path); return; }

    auto t = Platform::timestampHuman();
    f << "╔══════════════════════════════════════════════════════════════╗\n";
    f << "║              SYSTEM HEALTH TOOLKIT REPORT                   ║\n";
    f << "╚══════════════════════════════════════════════════════════════╝\n";
    f << "Generated: " << t << "\n";
    f << "Platform:  " << g_platform.osName << " " << g_platform.osBuild << "\n";
    f << "Host:      " << g_platform.hostname << "\n";
    f << "User:      " << g_platform.username << "\n";
    f << "Elevated:  " << (g_platform.elevated ? "Yes" : "No") << "\n\n";

    f << "OVERALL HEALTH: " << score.overall << "% (Grade: " << score.grade << ")\n";
    f << std::string(60, '=') << "\n\n";

    for (auto& [cat, s] : score.categories) {
        auto cname = [](auto c) -> std::string {
            switch(c) {
                case HealthCategory::WindowsIntegrity: return "Windows Integrity";
                case HealthCategory::Storage: return "Storage";
                case HealthCategory::Memory: return "Memory";
                case HealthCategory::Network: return "Network";
                case HealthCategory::Security: return "Security";
                case HealthCategory::Drivers: return "Drivers";
                case HealthCategory::Startup: return "Startup";
                case HealthCategory::Updates: return "Updates";
                case HealthCategory::Hardware: return "Hardware";
                case HealthCategory::EventLogs: return "Event Logs";
                case HealthCategory::Performance: return "Performance";
                case HealthCategory::Reliability: return "Reliability";
                default: return "Unknown";
            }
        };
        f << "  " << cname(cat) << ": " << s << "%\n";
    }
    f << "\n";

    // Storage summary
    f << "STORAGE:\n";
    f << std::string(40, '-') << "\n";
    for (auto& d : sr.drives) {
        f << "  " << d.mountPoint << " (" << d.label << ") "
          << Platform::formatBytes(d.totalBytes) << " total, "
          << Platform::formatBytes(d.freeBytes) << " free, "
          << Platform::formatBytes(d.usedBytes) << " used\n";
    }
    f << "  Total files: " << sr.totalFiles << "\n";
    f << "  Total folders: " << sr.totalFolders << "\n";
    f << "  Largest file: " << sr.largestFile << " (" 
      << Platform::formatBytes(sr.largestFileBytes) << ")\n\n";

    // Startup
    f << "STARTUP\n";
    f << std::string(40, '-') << "\n";
    f << "  Total entries: " << start.totalCount << " | Enabled: " << start.enabledCount
      << " | Disabled: " << start.disabledCount << "\n";
    f << "  Broken: " << start.brokenCount << " | Unsigned: " << start.unsignedCount << "\n\n";

    // Hardware
    f << "HARDWARE:\n";
    f << std::string(40, '-') << "\n";
    f << "  CPU: " << hw.cpu.model << " (" << hw.cpu.cores << "C/" << hw.cpu.threads << "T)\n";
    f << "  RAM: " << Platform::formatBytes(hw.memory.totalBytes) << " ("
      << (100.0 - hw.memory.usagePercent) << "% available)\n";
    for (auto& gpu : hw.gpus) {
        f << "  GPU: " << gpu.name << " (" << Platform::formatBytes(gpu.vramBytes) << ")\n";
    }
    f << "  UEFI: " << (hw.uefi ? "Yes" : "No")
      << " | Secure Boot: " << (hw.secureBoot ? "Yes" : "No")
      << " | TPM: " << (hw.tpmPresent ? "Yes" : "No") << "\n\n";

    // Network
    f << "NETWORK:\n";
    f << std::string(40, '-') << "\n";
    for (auto& a : net.adapters) {
        if (!a.connected) continue;
        f << "  " << a.name << " (" << a.type << ") " << a.ipv4 << "\n";
    }
    f << "  Public IP: " << net.publicIp << "\n";
    f << "  Ping: " << net.pingMs << "ms | Packet Loss: " << net.packetLoss << "%\n\n";

    // Event logs
    f << "EVENT LOGS:\n";
    f << std::string(40, '-') << "\n";
    f << "  Critical: " << evt.critical << " | Errors: " << evt.error
      << " | Warnings: " << evt.warning << "\n\n";

    // Performance
    f << "PERFORMANCE:\n";
    f << std::string(40, '-') << "\n";
    f << "  CPU: " << perf.cpuUsagePercent << "% | RAM: " << perf.ramUsagePercent
      << "% | Processes: " << perf.processCount << "\n\n";

    // Reliability
    f << "RELIABILITY:\n";
    f << std::string(40, '-') << "\n";
    f << "  Blue Screens: " << rel.blueScreens << " | App Crashes: " << rel.appCrashes
      << " | Driver Crashes: " << rel.driverCrashes << "\n";
    f << "  Reliability Index: " << rel.reliabilityIndex << "/10\n\n";

    // Windows info
    f << "WINDOWS:\n";
    f << std::string(40, '-') << "\n";
    f << "  Edition: " << winfo.edition << "\n";
    f << "  Version: " << winfo.version << " (Build " << winfo.build << ")\n";
    f << "  Activated: " << (winfo.activated ? "Yes" : "No") << "\n";
    f << "  Defender: " << (winfo.defenderActive ? "Active" : "Inactive") << "\n\n";

    // Corruption
    f << "CORRUPTION DETECTION:\n";
    f << std::string(40, '-') << "\n";
    f << "  Total Issues: " << corr.totalIssues << "\n";
    for (auto& d : corr.details) f << "  - " << d << "\n";
    if (corr.totalIssues == 0) f << "  No corruption detected.\n";
    f << "\n";

    // Repair results
    f << "REPAIR SESSION:\n";
    f << std::string(40, '-') << "\n";
    f << "  Passed: " << repSession.totalPass
      << " | Failed: " << repSession.totalFail
      << " | Skipped: " << repSession.totalSkip << "\n";
    for (auto& r : repSession.results) {
        f << "  [" << (r.success ? "OK" : "FAIL") << "] " << r.action
          << " (" << Platform::formatDuration(r.durationMs) << ")\n";
    }
    f << "\n";

    // Warnings & recommendations
    f << "WARNINGS:\n";
    for (auto& w : score.warnings) f << "  - " << w << "\n";
    if (score.warnings.empty()) f << "  None\n";
    f << "\nRECOMMENDATIONS:\n";
    for (auto& r : score.recommendations) f << "  - " << r << "\n";
    if (score.recommendations.empty()) f << "  None\n";

    f << "\n" << std::string(60, '=') << "\n";
    f << "Report generated by System Health Toolkit v" << TOOLKIT_VERSION << "\n";
    f << TOOLKIT_COPYRIGHT << "\n";
    f.close();
    Logger::instance().info("TXT report saved: " + path, "export");
}

static void exportHtml(const HealthScore& score,
                       const StorageResult& sr,
                       const StartupResult& start,
                       const HardwareResult& hw,
                       const NetworkResult& net,
                       const EventLogSummary& evt,
                       const PerformanceResult& perf,
                       const ReliabilityResult& rel,
                       const WindowsInfoResult& winfo,
                       const CorruptionResult& corr,
                       const RepairSession& repSession) {
    if (!ensureReportDir()) return;
    auto path = uniqueReportName("html");
    std::ofstream f(path);
    if (!f.is_open()) { UI::printError("Failed to create HTML report: " + path); return; }

    auto gradeColor = [](int s) -> std::string {
        if (s >= 90) return "#00c853";
        if (s >= 75) return "#ffc107";
        if (s >= 50) return "#ff9800";
        return "#f44336";
    };

    // Escape user/system-derived text before embedding it in HTML
    auto h = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default: out += c;
            }
        }
        return out;
    };

    f << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    f << "<meta charset=\"UTF-8\">\n";
    f << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    f << "<title>System Health Report</title>\n";
    f << "<style>\n";
    f << "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
    f << "body { font-family: 'Segoe UI', system-ui, sans-serif; background: #0d1117; color: #c9d1d9; padding: 40px; }\n";
    f << "h1 { color: #58a6ff; font-size: 2em; margin-bottom: 5px; }\n";
    f << "h2 { color: #58a6ff; border-bottom: 2px solid #30363d; padding-bottom: 8px; margin: 30px 0 15px; cursor: pointer; }\n";
    f << "h2::before { content: '▼ '; }\n";
    f << "h2.collapsed::before { content: '▶ '; }\n";
    f << "h2:hover { color: #79c0ff; }\n";
    f << ".container { max-width: 1100px; margin: 0 auto; }\n";
    f << ".header { text-align: center; margin-bottom: 30px; }\n";
    f << ".score-ring { width: 180px; height: 180px; border-radius: 50%; display: inline-flex; align-items: center; justify-content: center; margin: 20px; position: relative; }\n";
    f << ".score-ring { background: conic-gradient(" << gradeColor(score.overall) << " 0deg, " << gradeColor(score.overall) << " " << (score.overall * 3.6) << "deg, #30363d " << (score.overall * 3.6) << "deg); }\n";
    f << ".score-inner { width: 140px; height: 140px; border-radius: 50%; background: #0d1117; display: flex; flex-direction: column; align-items: center; justify-content: center; }\n";
    f << ".score-number { font-size: 3em; font-weight: bold; color: " << gradeColor(score.overall) << "; }\n";
    f << ".score-grade { font-size: 1.5em; color: " << gradeColor(score.overall) << "; }\n";
    f << ".score-label { color: #8b949e; font-size: 0.9em; }\n";
    f << "table { width: 100%; border-collapse: collapse; margin: 10px 0; }\n";
    f << "th, td { padding: 10px 14px; text-align: left; border-bottom: 1px solid #21262d; }\n";
    f << "th { background: #161b22; color: #58a6ff; font-weight: 600; }\n";
    f << "tr:hover { background: #161b22; }\n";
    f << ".health-bar { height: 20px; border-radius: 10px; background: #30363d; overflow: hidden; min-width: 200px; }\n";
    f << ".health-bar-fill { height: 100%; border-radius: 10px; transition: width 0.5s; }\n";
    f << ".badge { padding: 3px 10px; border-radius: 12px; font-size: 0.85em; font-weight: 600; }\n";
    f << ".badge-ok { background: #00c85333; color: #00c853; }\n";
    f << ".badge-warn { background: #ffc10733; color: #ffc107; }\n";
    f << ".badge-err { background: #f4433633; color: #f44336; }\n";
    f << ".card { background: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 20px; margin: 10px 0; }\n";
    f << ".grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 15px; }\n";
    f << ".stat { text-align: center; }\n";
    f << ".stat-value { font-size: 1.5em; font-weight: bold; color: #58a6ff; }\n";
    f << ".stat-label { color: #8b949e; font-size: 0.85em; }\n";
    f << ".warning { border-left: 4px solid #ffc107; padding: 12px; margin: 8px 0; background: #ffc1070d; }\n";
    f << ".error-item { border-left: 4px solid #f44336; padding: 12px; margin: 8px 0; background: #f443360d; }\n";
    f << ".recommendation { border-left: 4px solid #00c853; padding: 12px; margin: 8px 0; background: #00c8530d; }\n";
    f << ".section-content { margin: 10px 0; }\n";
    f << ".meta { color: #8b949e; font-size: 0.9em; margin-bottom: 5px; }\n";
    f << "footer { text-align: center; margin-top: 40px; padding: 20px; border-top: 1px solid #30363d; color: #8b949e; }\n";
    f << "@media (max-width: 600px) { body { padding: 15px; } .grid { grid-template-columns: 1fr; } }\n";
    f << "</style>\n";
    f << "<script>\ndocument.addEventListener('click', function(e) {\n";
    f << "  if (e.target.tagName === 'H2') {\n";
    f << "    var next = e.target.nextElementSibling;\n";
    f << "    while (next && next.tagName !== 'H2') { next.style.display = next.style.display === 'none' ? '' : 'none'; next = next.nextElementSibling; }\n";
    f << "    e.target.classList.toggle('collapsed');\n  }\n});\n</script>\n";
    f << "</head>\n<body>\n<div class=\"container\">\n";

    // Header
    f << "<div class=\"header\">\n";
    f << "<h1>🩺 System Health Toolkit Report</h1>\n";
    f << "<p class=\"meta\">Generated: " << Platform::timestampHuman()
      << " | " << h(g_platform.osName) << " | " << h(g_platform.hostname) << "</p>\n";
    f << "<div class=\"score-ring\"><div class=\"score-inner\">\n";
    f << "  <div class=\"score-label\">Overall Health</div>\n";
    f << "  <div class=\"score-number\">" << score.overall << "%</div>\n";
    f << "  <div class=\"score-grade\">" << score.grade << "</div>\n";
    f << "</div></div>\n</div>\n";

    // Category scores
    f << "<h2>Category Scores</h2>\n<div class=\"section-content\">\n";
    for (auto& [cat, s] : score.categories) {
        auto cname = [](auto c) -> std::string {
            switch(c) {
                case HealthCategory::WindowsIntegrity: return "Windows Integrity";
                case HealthCategory::Storage: return "Storage";
                case HealthCategory::Memory: return "Memory";
                case HealthCategory::Network: return "Network";
                case HealthCategory::Security: return "Security";
                case HealthCategory::Drivers: return "Drivers";
                case HealthCategory::Startup: return "Startup";
                case HealthCategory::Updates: return "Updates";
                case HealthCategory::Hardware: return "Hardware";
                case HealthCategory::EventLogs: return "Event Logs";
                case HealthCategory::Performance: return "Performance";
                case HealthCategory::Reliability: return "Reliability";
                default: return "Unknown";
            }
        };
        f << "<div style=\"margin: 8px 0;\"><span style=\"display: inline-block; width: 160px;\">" 
          << cname(cat) << "</span>\n";
        f << "<div class=\"health-bar\" style=\"display: inline-block; vertical-align: middle;\">\n";
        f << "<div class=\"health-bar-fill\" style=\"width: " << s << "%; background: " 
          << gradeColor(s) << ";\"></div></div>\n";
        f << "<span style=\"margin-left: 10px; font-weight: bold; color: " << gradeColor(s) 
          << ";\">" << s << "%</span></div>\n";
    }
    f << "</div>\n";

    // Storage
    f << "<h2>Storage</h2>\n<div class=\"section-content\">\n";
    f << "<table><tr><th>Drive</th><th>Total</th><th>Used</th><th>Free</th><th>Type</th></tr>\n";
    for (auto& d : sr.drives) {
        double pct = d.totalBytes > 0 ? (100.0 * d.usedBytes / d.totalBytes) : 0;
        f << "<tr><td>" << d.mountPoint << "</td><td>" << Platform::formatBytes(d.totalBytes)
          << "</td><td>" << Platform::formatBytes(d.usedBytes) << " (" << (int)pct << "%)</td>"
          << "<td>" << Platform::formatBytes(d.freeBytes) << "</td><td>" << d.driveType << "</td></tr>\n";
    }
    f << "</table>\n";
    f << "<div class=\"grid\">\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << sr.totalFiles << "</div><div class=\"stat-label\">Total Files</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << sr.totalFolders << "</div><div class=\"stat-label\">Total Folders</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << Platform::formatBytes(sr.largestFileBytes) << "</div><div class=\"stat-label\">Largest File</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << Platform::formatBytes(sr.reclaimableBytes) << "</div><div class=\"stat-label\">Reclaimable Space</div></div>\n";
    f << "</div>\n</div>\n";

    // Hardware
    f << "<h2>Hardware</h2>\n<div class=\"section-content\">\n";
    f << "<table><tr><th>Component</th><th>Details</th></tr>\n";
    f << "<tr><td>CPU</td><td>" << h(hw.cpu.model) << " (" << hw.cpu.cores << "C/" << hw.cpu.threads << "T @ " << hw.cpu.frequencyGHz << "GHz)</td></tr>\n";
    f << "<tr><td>RAM</td><td>" << Platform::formatBytes(hw.memory.totalBytes) << " (" << (int)hw.memory.usagePercent << "% used)</td></tr>\n";
    for (size_t i = 0; i < hw.gpus.size(); ++i)
        f << "<tr><td>GPU " << (i+1) << "</td><td>" << h(hw.gpus[i].name) << " (" << Platform::formatBytes(hw.gpus[i].vramBytes) << ")</td></tr>\n";
    f << "<tr><td>Motherboard</td><td>" << h(hw.motherboard) << "</td></tr>\n";
    f << "<tr><td>BIOS</td><td>" << h(hw.biosVersion) << (hw.uefi ? " (UEFI)" : "") << "</td></tr>\n";
    f << "<tr><td>Secure Boot</td><td>" << (hw.secureBoot ? "✅ Enabled" : "❌ Disabled") << "</td></tr>\n";
    f << "<tr><td>TPM</td><td>" << (hw.tpmPresent ? "✅ Present" : "❌ Not Found") << "</td></tr>\n";
    f << "</table>\n</div>\n";

    // Network
    f << "<h2>Network</h2>\n<div class=\"section-content\">\n";
    f << "<table><tr><th>Adapter</th><th>Type</th><th>IP</th><th>Status</th></tr>\n";
    for (auto& a : net.adapters) {
        f << "<tr><td>" << h(a.name) << "</td><td>" << h(a.type) << "</td><td>" << h(a.ipv4) << "</td>"
          << "<td><span class=\"badge " << (a.connected ? "badge-ok" : "badge-err") << "\">"
          << (a.connected ? "Connected" : "Disconnected") << "</span></td></tr>\n";
    }
    f << "</table>\n";
    f << "<p>Public IP: " << net.publicIp << " | Ping: " << net.pingMs 
      << "ms | Packet Loss: " << net.packetLoss << "%</p>\n";
    f << "<p>VPN: " << (net.vpnActive ? "Active" : "Inactive") 
      << " | Firewall: " << (net.firewallActive ? "Active" : "Inactive") << "</p>\n";
    f << "</div>\n";

    // Startup
    f << "<h2>Startup Analysis</h2>\n<div class=\"section-content\">\n";
    f << "<div class=\"grid\">\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << start.totalCount << "</div><div class=\"stat-label\">Startup Items</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << start.enabledCount << "</div><div class=\"stat-label\">Enabled</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << start.brokenCount << "</div><div class=\"stat-label\" style=\"color: #f44336;\">Broken</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << start.unsignedCount << "</div><div class=\"stat-label\" style=\"color: #ffc107;\">Unsigned</div></div>\n";
    f << "</div>\n</div>\n";

    // Event Logs
    f << "<h2>Event Logs</h2>\n<div class=\"section-content\">\n";
    f << "<div class=\"grid\">\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\" style=\"color: #f44336;\">" << evt.critical << "</div><div class=\"stat-label\">Critical</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\" style=\"color: #ff9800;\">" << evt.error << "</div><div class=\"stat-label\">Errors</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\" style=\"color: #ffc107;\">" << evt.warning << "</div><div class=\"stat-label\">Warnings</div></div>\n";
    f << "</div>\n</div>\n";

    // Performance
    f << "<h2>Performance</h2>\n<div class=\"section-content\">\n";
    f << "<div class=\"grid\">\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << (int)perf.cpuUsagePercent << "%</div><div class=\"stat-label\">CPU Usage</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << (int)perf.ramUsagePercent << "%</div><div class=\"stat-label\">RAM Usage</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << perf.processCount << "</div><div class=\"stat-label\">Processes</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << perf.runningServices << "</div><div class=\"stat-label\">Services</div></div>\n";
    f << "</div>\n</div>\n";

    // Reliability
    f << "<h2>Reliability</h2>\n<div class=\"section-content\">\n";
    f << "<div class=\"grid\">\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\" style=\"color: #f44336;\">" << rel.blueScreens << "</div><div class=\"stat-label\">Blue Screens</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\" style=\"color: #ff9800;\">" << rel.appCrashes << "</div><div class=\"stat-label\">App Crashes</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << rel.reliabilityIndex << "</div><div class=\"stat-label\">Reliability /10</div></div>\n";
    f << "</div>\n</div>\n";

    // Windows
    f << "<h2>Windows Information</h2>\n<div class=\"section-content\">\n";
    f << "<table><tr><th>Property</th><th>Value</th></tr>\n";
    f << "<tr><td>Edition</td><td>" << h(winfo.edition) << "</td></tr>\n";
    f << "<tr><td>Version</td><td>" << h(winfo.version) << " (Build " << h(winfo.build) << ")</td></tr>\n";
    f << "<tr><td>Installed</td><td>" << h(winfo.installDate) << "</td></tr>\n";
    f << "<tr><td>Uptime</td><td>" << h(winfo.uptime) << "</td></tr>\n";
    f << "<tr><td>Activation</td><td>" << (winfo.activated ? "✅ Activated" : "❌ Not Activated") << "</td></tr>\n";
    f << "<tr><td>Defender</td><td>" << (winfo.defenderActive ? "✅ Active" : "❌ Inactive") << "</td></tr>\n";
    f << "<tr><td>Hyper-V</td><td>" << (winfo.hyperV ? "Enabled" : "Disabled") << "</td></tr>\n";
    f << "<tr><td>WSL</td><td>" << (winfo.wsl ? "Installed" : "Not installed") << "</td></tr>\n";
    f << "<tr><td>Secure Boot</td><td>" << (hw.secureBoot ? "Enabled" : "Disabled") << "</td></tr>\n";
    f << "</table>\n</div>\n";

    // Corruption
    f << "<h2>Corruption Detection</h2>\n<div class=\"section-content\">\n";
    f << "<div class=\"card\">\n";
    f << "<p><strong>Issues Found:</strong> " << corr.totalIssues << "</p>\n";
    if (corr.totalIssues > 0) {
        for (auto& d : corr.details)
            f << "<div class=\"error-item\">" << h(d) << "</div>\n";
    } else {
        f << "<p style=\"color: #00c853;\">✅ No corruption detected.</p>\n";
    }
    f << "</div>\n</div>\n";

    // Repair results
    f << "<h2>Repair Results</h2>\n<div class=\"section-content\">\n";
    f << "<div class=\"grid\">\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\" style=\"color: #00c853;\">" << repSession.totalPass << "</div><div class=\"stat-label\">Passed</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\" style=\"color: #f44336;\">" << repSession.totalFail << "</div><div class=\"stat-label\">Failed</div></div>\n";
    f << "<div class=\"card stat\"><div class=\"stat-value\">" << repSession.totalSkip << "</div><div class=\"stat-label\">Skipped</div></div>\n";
    f << "</div>\n<table><tr><th>Action</th><th>Result</th><th>Duration</th></tr>\n";
    for (auto& r : repSession.results) {
        f << "<tr><td>" << h(r.action) << "</td>"
          << "<td><span class=\"badge " << (r.success ? "badge-ok" : "badge-err") << "\">"
          << (r.success ? "OK" : "FAIL") << "</span></td>"
          << "<td>" << Platform::formatDuration(r.durationMs) << "</td></tr>\n";
    }
    f << "</table>\n</div>\n";

    // Warnings
    if (!score.warnings.empty()) {
        f << "<h2>Warnings</h2>\n<div class=\"section-content\">\n";
        for (auto& w : score.warnings) f << "<div class=\"warning\">⚠ " << h(w) << "</div>\n";
        f << "</div>\n";
    }

    // Errors
    if (!score.errors.empty()) {
        f << "<h2>Issues</h2>\n<div class=\"section-content\">\n";
        for (auto& e : score.errors) f << "<div class=\"error-item\">✗ " << h(e) << "</div>\n";
        f << "</div>\n";
    }

    // Recommendations
    if (!score.recommendations.empty()) {
        f << "<h2>Recommendations</h2>\n<div class=\"section-content\">\n";
        for (auto& r : score.recommendations) f << "<div class=\"recommendation\">→ " << h(r) << "</div>\n";
        f << "</div>\n";
    }

    f << "<footer>\n";
    f << "<p>Generated by System Health Toolkit v" << TOOLKIT_VERSION << "</p>\n";
    f << "<p>" << TOOLKIT_COPYRIGHT << "</p>\n";
    f << "</footer>\n</div>\n</body>\n</html>\n";
    f.close();
    Logger::instance().info("HTML report saved: " + path, "export");
}

static void exportJson(const HealthScore& score,
                       const StorageResult& sr,
                       const StartupResult& start,
                       const HardwareResult& hw,
                       const NetworkResult& net,
                       const EventLogSummary& evt,
                       const PerformanceResult& perf,
                       const ReliabilityResult& rel,
                       const WindowsInfoResult& winfo,
                       const CorruptionResult& corr,
                       const RepairSession& repSession) {
    if (!ensureReportDir()) return;
    auto path = uniqueReportName("json");
    std::ofstream f(path);
    if (!f.is_open()) { UI::printError("Failed to create JSON report: " + path); return; }

    auto esc = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    };

    f << "{\n";
    f << "  \"report\": {\n";
    f << "    \"tool\": \"System Health Toolkit\",\n";
    f << "    \"version\": \"" << TOOLKIT_VERSION << "\",\n";
    f << "    \"generated\": \"" << Platform::timestampHuman() << "\",\n";
    f << "    \"platform\": \"" << esc(g_platform.osName) << "\",\n";
    f << "    \"hostname\": \"" << esc(g_platform.hostname) << "\",\n";
    f << "    \"username\": \"" << esc(g_platform.username) << "\",\n";
    f << "    \"elevated\": " << (g_platform.elevated ? "true" : "false") << "\n";
    f << "  },\n";
    f << "  \"health_score\": {\n";
    f << "    \"overall\": " << score.overall << ",\n";
    f << "    \"grade\": \"" << score.grade << "\",\n";
    f << "    \"categories\": [\n";
    bool first = true;
    for (auto& [cat, s] : score.categories) {
        if (!first) f << ",\n";
        first = false;
        auto cname = [](auto c) -> std::string {
            switch(c) {
                case HealthCategory::WindowsIntegrity: return "WindowsIntegrity";
                case HealthCategory::Storage: return "Storage";
                case HealthCategory::Memory: return "Memory";
                case HealthCategory::Network: return "Network";
                case HealthCategory::Security: return "Security";
                case HealthCategory::Drivers: return "Drivers";
                case HealthCategory::Startup: return "Startup";
                case HealthCategory::Updates: return "Updates";
                case HealthCategory::Hardware: return "Hardware";
                case HealthCategory::EventLogs: return "EventLogs";
                case HealthCategory::Performance: return "Performance";
                case HealthCategory::Reliability: return "Reliability";
                default: return "Unknown";
            }
        };
        f << "      { \"name\": \"" << cname(cat) << "\", \"score\": " << s << " }";
    }
    f << "\n    ],\n";
    f << "    \"warnings\": [\n";
    for (size_t i = 0; i < score.warnings.size(); ++i) {
        if (i > 0) f << ",\n";
        f << "      \"" << esc(score.warnings[i]) << "\"";
    }
    f << "\n    ],\n";
    f << "    \"recommendations\": [\n";
    for (size_t i = 0; i < score.recommendations.size(); ++i) {
        if (i > 0) f << ",\n";
        f << "      \"" << esc(score.recommendations[i]) << "\"";
    }
    f << "\n    ]\n";
    f << "  },\n";

    // Storage
    f << "  \"storage\": {\n";
    f << "    \"totalFiles\": " << sr.totalFiles << ",\n";
    f << "    \"totalFolders\": " << sr.totalFolders << ",\n";
    f << "    \"largestFile\": \"" << esc(sr.largestFile) << "\",\n";
    f << "    \"largestFileBytes\": " << sr.largestFileBytes << ",\n";
    f << "    \"reclaimableBytes\": " << sr.reclaimableBytes << ",\n";
    f << "    \"drives\": [\n";
    for (size_t i = 0; i < sr.drives.size(); ++i) {
        if (i > 0) f << ",\n";
        auto& d = sr.drives[i];
        f << "      { \"mountPoint\": \"" << esc(d.mountPoint) << "\", \"total\": " << d.totalBytes
          << ", \"free\": " << d.freeBytes << ", \"used\": " << d.usedBytes
          << ", \"type\": \"" << esc(d.driveType) << "\", \"smart\": " << (d.smartOk ? "true" : "false")
          << ", \"bitlocker\": " << (d.bitlocker ? "true" : "false") << " }";
    }
    f << "\n    ]\n";
    f << "  },\n";

    // Hardware
    f << "  \"hardware\": {\n";
    f << "    \"cpu\": { \"model\": \"" << esc(hw.cpu.model) << "\", \"cores\": " << hw.cpu.cores
      << ", \"threads\": " << hw.cpu.threads << ", \"frequency\": " << hw.cpu.frequencyGHz << " },\n";
    f << "    \"memory\": { \"total\": " << hw.memory.totalBytes << ", \"available\": " << hw.memory.availableBytes
      << ", \"used\": " << hw.memory.usedBytes << " },\n";
    f << "    \"gpus\": [\n";
    for (size_t i = 0; i < hw.gpus.size(); ++i) {
        if (i > 0) f << ",\n";
        f << "      { \"name\": \"" << esc(hw.gpus[i].name) << "\", \"vram\": " << hw.gpus[i].vramBytes << " }";
    }
    f << "\n    ],\n";
    f << "    \"motherboard\": \"" << esc(hw.motherboard) << "\",\n";
    f << "    \"uefi\": " << (hw.uefi ? "true" : "false") << ",\n";
    f << "    \"secureBoot\": " << (hw.secureBoot ? "true" : "false") << ",\n";
    f << "    \"tpmPresent\": " << (hw.tpmPresent ? "true" : "false") << "\n";
    f << "  },\n";

    // Repair
    f << "  \"repair\": {\n";
    f << "    \"totalPass\": " << repSession.totalPass << ",\n";
    f << "    \"totalFail\": " << repSession.totalFail << ",\n";
    f << "    \"totalSkip\": " << repSession.totalSkip << ",\n";
    f << "    \"rebootNeeded\": " << (repSession.rebootNeeded ? "true" : "false") << ",\n";
    f << "    \"results\": [\n";
    for (size_t i = 0; i < repSession.results.size(); ++i) {
        if (i > 0) f << ",\n";
        auto& r = repSession.results[i];
        f << "      { \"action\": \"" << esc(r.action) << "\", \"success\": " << (r.success ? "true" : "false")
          << ", \"durationMs\": " << r.durationMs << " }";
    }
    f << "\n    ]\n";
    f << "  }\n";
    f << "}\n";
    f.close();
    Logger::instance().info("JSON report saved: " + path, "export");
}

static void exportCsv(const HealthScore& score,
                      const StorageResult& sr,
                      const StartupResult& start,
                      const HardwareResult& hw,
                      const NetworkResult& net,
                      const EventLogSummary& evt,
                      const PerformanceResult& perf,
                      const ReliabilityResult& rel,
                      const WindowsInfoResult& winfo,
                      const CorruptionResult& corr,
                      const RepairSession& repSession) {
    if (!ensureReportDir()) return;
    auto path = uniqueReportName("csv");
    std::ofstream f(path);
    if (!f.is_open()) { UI::printError("Failed to create CSV report: " + path); return; }

    auto csv = [](const std::string& s) -> std::string {
        if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
        std::string out = "\"";
        for (char c : s) {
            if (c == '"') out += "\"\"";  // RFC 4180: double embedded quotes
            else out += c;
        }
        out += "\"";
        return out;
    };

    f << "Section,Key,Value\n";
    f << "General,Timestamp," << Platform::timestampHuman() << "\n";
    f << "General,Platform," << csv(g_platform.osName) << "\n";
    f << "General,Hostname," << csv(g_platform.hostname) << "\n";
    f << "General,Elevated," << (g_platform.elevated ? "Yes" : "No") << "\n";
    f << "Health Score,Overall," << score.overall << "\n";
    f << "Health Score,Grade," << score.grade << "\n";
    for (auto& [cat, s] : score.categories) {
        auto cname = [](auto c) -> std::string {
            switch(c) {
                case HealthCategory::WindowsIntegrity: return "Windows Integrity";
                case HealthCategory::Storage: return "Storage";
                case HealthCategory::Memory: return "Memory";
                case HealthCategory::Network: return "Network";
                case HealthCategory::Security: return "Security";
                case HealthCategory::Drivers: return "Drivers";
                case HealthCategory::Startup: return "Startup";
                case HealthCategory::Updates: return "Updates";
                case HealthCategory::Hardware: return "Hardware";
                case HealthCategory::EventLogs: return "Event Logs";
                case HealthCategory::Performance: return "Performance";
                case HealthCategory::Reliability: return "Reliability";
                default: return "Unknown";
            }
        };
        f << "Health Score," << csv(cname(cat)) << "," << s << "\n";
    }
    for (auto& d : sr.drives)
        f << "Storage," << csv(d.mountPoint) << "," << Platform::formatBytes(d.totalBytes) << "\n";
    f << "Storage,Total Files," << sr.totalFiles << "\n";
    f << "Storage,Reclaimable," << sr.reclaimableBytes << "\n";
    f << "Startup,Total," << start.totalCount << "\n";
    f << "Startup,Broken," << start.brokenCount << "\n";
    f << "Hardware,CPU," << csv(hw.cpu.model) << "\n";
    f << "Hardware,RAM," << hw.memory.totalBytes << "\n";
    f << "Network,Ping," << net.pingMs << "\n";
    f << "Network,Packet Loss," << net.packetLoss << "\n";
    f << "Events,Critical," << evt.critical << "\n";
    f << "Events,Errors," << evt.error << "\n";
    f << "Performance,CPU Usage," << perf.cpuUsagePercent << "\n";
    f << "Performance,RAM Usage," << perf.ramUsagePercent << "\n";
    f << "Reliability,Blue Screens," << rel.blueScreens << "\n";
    f << "Reliability,Index," << rel.reliabilityIndex << "\n";
    f << "Windows,Version," << csv(winfo.version) << "\n";
    f << "Windows,Activated," << (winfo.activated ? "Yes" : "No") << "\n";
    f << "Corruption,Issues," << corr.totalIssues << "\n";
    f << "Repair,Passed," << repSession.totalPass << "\n";
    f << "Repair,Failed," << repSession.totalFail << "\n";
    f << "Repair,Skipped," << repSession.totalSkip << "\n";
    for (auto& w : score.warnings)
        f << "Warning,," << csv(w) << "\n";
    for (auto& r : score.recommendations)
        f << "Recommendation,," << csv(r) << "\n";
    f.close();
    Logger::instance().info("CSV report saved: " + path, "export");
}

static void exportAll(const HealthScore& score,
                      const StorageResult& sr,
                      const StartupResult& start,
                      const HardwareResult& hw,
                      const NetworkResult& net,
                      const EventLogSummary& evt,
                      const PerformanceResult& perf,
                      const ReliabilityResult& rel,
                      const WindowsInfoResult& winfo,
                      const CorruptionResult& corr,
                      const RepairSession& repSession) {
    Timer t;
    UI::printInfo("Exporting reports...");
    exportTxt(score, sr, start, hw, net, evt, perf, rel, winfo, corr, repSession);
    exportHtml(score, sr, start, hw, net, evt, perf, rel, winfo, corr, repSession);
    exportJson(score, sr, start, hw, net, evt, perf, rel, winfo, corr, repSession);
    exportCsv(score, sr, start, hw, net, evt, perf, rel, winfo, corr, repSession);
    UI::printSuccess("Reports exported to: " + g_config.reportDir + " (" + Platform::formatDuration(t.elapsedMs()) + ")");
}

// ─────────────────────────────────────────────
//  Health score computation engine
// ─────────────────────────────────────────────
static HealthScore computeHealthScore(
    const StorageResult& sr,
    const StartupResult& start,
    const HardwareResult& hw,
    const NetworkResult& net,
    const EventLogSummary& evt,
    const PerformanceResult& perf,
    const ReliabilityResult& rel,
    const WindowsInfoResult& winfo,
    const CorruptionResult& corr,
    const UpdateResult& upd,
    const RepairSession& repSession)
{
    HealthScore hs;

    // Storage score: based on free space percentage
    int storageScore = 100;
    for (auto& d : sr.drives) {
        if (d.totalBytes == 0) continue;
        double freePct = 100.0 * d.freeBytes / d.totalBytes;
        if (freePct < 5) storageScore -= 30;
        else if (freePct < 10) storageScore -= 15;
        else if (freePct < 20) storageScore -= 5;
    }
    storageScore = std::max(0, storageScore);
    hs.categories[HealthCategory::Storage] = storageScore;

    // Memory score
    int memoryScore = 100;
    if (hw.memory.totalBytes > 0) {
        double usedPct = hw.memory.usagePercent;
        if (usedPct > 90) memoryScore -= 30;
        else if (usedPct > 80) memoryScore -= 15;
        else if (usedPct > 70) memoryScore -= 5;
    }
    hs.categories[HealthCategory::Memory] = memoryScore;

    // Network score
    int netScore = 100;
    bool hasConnected = false;
    for (auto& a : net.adapters) if (a.connected) hasConnected = true;
    if (!hasConnected) netScore -= 50;
    if (net.packetLoss > 5) netScore -= 20;
    if (net.pingMs > 200) netScore -= 15;
    else if (net.pingMs > 100) netScore -= 5;
    hs.categories[HealthCategory::Network] = std::max(0, netScore);

    // Security score
    int secScore = 100;
    if (!winfo.defenderActive) secScore -= 30;
    if (!winfo.firewallActive) secScore -= 20;
    if (!hw.secureBoot) secScore -= 10;
    if (!winfo.coreIsolation) secScore -= 10;
    if (!winfo.smartScreen) secScore -= 5;
    hs.categories[HealthCategory::Security] = std::max(0, secScore);

    // Startup score
    int startupScore = 100;
    if (start.brokenCount > 0) startupScore -= start.brokenCount * 10;
    if (start.unsignedCount > 0) startupScore -= start.unsignedCount * 5;
    if (start.totalCount > 30) startupScore -= 10;
    hs.categories[HealthCategory::Startup] = std::max(0, startupScore);

    // Updates score
    int updScore = 100;
    if (upd.totalAvailable > 10) updScore -= 20;
    else if (upd.totalAvailable > 5) updScore -= 10;
    if (!upd.failedUpdates.empty()) updScore -= upd.failedUpdates.size() * 10;
    hs.categories[HealthCategory::Updates] = std::max(0, updScore);

    // Drivers (approximate from update data)
    int driversScore = 100;
    if (upd.drivers.size() > 5) driversScore -= 15;
    hs.categories[HealthCategory::Drivers] = std::max(0, driversScore);

    // Hardware score
    int hwScore = 100;
    if (hw.cpu.temperatureC > 90) hwScore -= 20;
    else if (hw.cpu.temperatureC > 80) hwScore -= 10;
    if (hw.batteryPercent >= 0 && hw.batteryPercent < 20) hwScore -= 15;
    hs.categories[HealthCategory::Hardware] = std::max(0, hwScore);

    // Event logs score
    int evtScore = 100;
    if (evt.critical > 0) evtScore -= evt.critical * 15;
    if (evt.error > 10) evtScore -= 20;
    else if (evt.error > 5) evtScore -= 10;
    hs.categories[HealthCategory::EventLogs] = std::max(0, evtScore);

    // Performance score
    int perfScore = 100;
    if (perf.cpuUsagePercent > 90) perfScore -= 20;
    else if (perf.cpuUsagePercent > 75) perfScore -= 10;
    if (perf.ramUsagePercent > 90) perfScore -= 15;
    else if (perf.ramUsagePercent > 75) perfScore -= 5;
    hs.categories[HealthCategory::Performance] = std::max(0, perfScore);

    // Reliability score
    int relScore = 100;
    relScore = std::max(0, static_cast<int>(rel.reliabilityIndex * 10));
    hs.categories[HealthCategory::Reliability] = relScore;

    // Windows integrity score
    int intScore = 100;
    if (corr.totalIssues > 0) intScore -= corr.totalIssues * 10;
    if (repSession.totalFail > 0) intScore -= repSession.totalFail * 5;
    if (!winfo.activated) intScore -= 20;
    hs.categories[HealthCategory::WindowsIntegrity] = std::max(0, intScore);

    // Build warnings
    for (auto& d : sr.drives) {
        if (d.totalBytes > 0) {
            double freePct = 100.0 * d.freeBytes / d.totalBytes;
            if (freePct < 5) hs.warnings.push_back("Critical: " + d.mountPoint + " has only " + std::to_string((int)freePct) + "% free space");
            else if (freePct < 10) hs.warnings.push_back("Low disk space on " + d.mountPoint + " (" + std::to_string((int)freePct) + "% free)");
        }
    }
    if (!winfo.activated) hs.warnings.push_back("Windows is not activated");
    if (!winfo.defenderActive) hs.warnings.push_back("Windows Defender is inactive");
    if (start.brokenCount > 0) hs.warnings.push_back(std::to_string(start.brokenCount) + " broken startup entries found");
    if (rel.blueScreens > 0) hs.warnings.push_back(std::to_string(rel.blueScreens) + " blue screen(s) recorded");
    if (corr.totalIssues > 0) hs.warnings.push_back(std::to_string(corr.totalIssues) + " corruption issue(s) found");
    if (upd.rebootPending) hs.warnings.push_back("Reboot is pending");
    if (net.packetLoss > 5) hs.warnings.push_back("Network packet loss: " + std::to_string(net.packetLoss) + "%");
    if (hw.cpu.temperatureC > 85) hs.warnings.push_back("CPU temperature is high: " + std::to_string((int)hw.cpu.temperatureC) + "°C");

    // Build recommendations
    if (sr.reclaimableBytes > 5ULL * 1024 * 1024 * 1024)
        hs.recommendations.push_back("Run disk cleanup to reclaim ~" + Platform::formatBytes(sr.reclaimableBytes));
    if (upd.totalAvailable > 0)
        hs.recommendations.push_back(std::to_string(upd.totalAvailable) + " updates available — install them");
    if (!winfo.memoryIntegrity)
        hs.recommendations.push_back("Enable Memory Integrity in Windows Security for better protection");
    if (start.brokenCount > 0)
        hs.recommendations.push_back("Review and remove broken startup entries");
    if (upd.rebootPending)
        hs.recommendations.push_back("Restart your system to complete pending updates");
    if (!winfo.activated)
        hs.recommendations.push_back("Activate Windows to receive security updates");
    if (repSession.rebootNeeded)
        hs.recommendations.push_back("A reboot is required to complete system repairs");
    if (corr.totalIssues > 0 && repSession.totalFail > 0)
        hs.recommendations.push_back("Run 'Repair Windows' option with elevated privileges for deeper repairs");
    if (net.packetLoss > 2)
        hs.recommendations.push_back("Check network cables / Wi-Fi signal for packet loss (" + std::to_string(net.packetLoss) + "%)");

    hs.compute();
    return hs;
}

// ─────────────────────────────────────────────
//  Menu: Full Scan (Option 1)
// ─────────────────────────────────────────────
static void optionFullScan() {
    UI::showSectionHeader("Full System Scan");
    Logger::instance().info("Full scan started", "scan");

    StorageResult sr;
    StartupResult start;
    HardwareResult hw;
    NetworkResult net;
    EventLogSummary evt;
    PerformanceResult perf;
    ReliabilityResult rel;
    WindowsInfoResult winfo;
    CorruptionResult corr;
    UpdateResult upd;
    RepairSession repSession;

    UI::printInfo("Running comprehensive system analysis — all scanners run in parallel...");

    // Every scanner is independent (each shells out to its own child
    // processes and only touches its own result), so run them all
    // concurrently. Wall time becomes max(scanner) instead of sum(scanners).
    MalwareResult mal;
    Timer scanTimer;
    {
        std::atomic<int> done{0};
        constexpr int totalScanners = 11;

        auto launch = [&](const char* name, auto fn) {
            return std::async(std::launch::async, [&done, name, fn]() {
                auto r = guarded(name, fn);
                ++done;
                return r;
            });
        };

        Spinner spin("Scanning system (0/" + std::to_string(totalScanners) + " scanners finished)");
        spin.start();

        auto fSr    = launch("Storage",     []{ StorageAnalyzer s(g_config, g_platform);   return s.analyze(); });
        auto fHw    = launch("Hardware",    []{ HardwareScanner s(g_config, g_platform);   return s.scan(); });
        auto fNet   = launch("Network",     []{ NetworkScanner s(g_config, g_platform);    return s.scan(); });
        auto fStart = launch("Startup",     []{ StartupAnalyzer s(g_config, g_platform);   return s.analyze(); });
        auto fEvt   = launch("EventLog",    []{ EventLogAnalyzer s(g_config, g_platform);  return s.analyze(); });
        auto fPerf  = launch("Performance", []{ PerformanceMonitor s(g_config, g_platform);return s.measure(); });
        auto fRel   = launch("Reliability", []{ ReliabilityAnalyzer s(g_config, g_platform);return s.analyze(); });
        auto fWin   = launch("WindowsInfo", []{ WindowsInfoScanner s(g_config, g_platform);return s.scan(); });
        auto fCorr  = launch("Corruption",  []{ CorruptionScanner s(g_config, g_platform); return s.scan(); });
        auto fUpd   = launch("Updates",     []{ UpdateManager s(g_config, g_platform);     return s.detectUpdates(); });
        auto fMal   = launch("Malware",     []{ MalwareScanner s(g_config, g_platform);    return s.scan(MalwareScanDepth::None); });

        while (done.load() < totalScanners) {
            spin.setLabel("Scanning system (" + std::to_string(done.load()) + "/" +
                          std::to_string(totalScanners) + " scanners finished, " +
                          std::to_string((int)scanTimer.elapsedSec()) + "s)");
            std::this_thread::sleep_for(200ms);
        }

        sr    = fSr.get();
        hw    = fHw.get();
        net   = fNet.get();
        start = fStart.get();
        evt   = fEvt.get();
        perf  = fPerf.get();
        rel   = fRel.get();
        winfo = fWin.get();
        corr  = fCorr.get();
        upd   = fUpd.get();
        mal   = fMal.get();

        spin.setLabel("Full scan finished in " + Platform::formatDuration(scanTimer.elapsedMs()));
        spin.stop();
    }

    // Compute and display health score
    auto hs = computeHealthScore(sr, start, hw, net, evt, perf, rel, winfo, corr, upd, repSession);

    // Fold the malware assessment into the Security category.
    {
        int sec = hs.categories.count(HealthCategory::Security)
                    ? hs.categories[HealthCategory::Security] : 100;
        sec -= mal.activeCount * 40;
        sec -= mal.heuristicFlags * 8;
        if (mal.enginePresent && !mal.realTimeProtection) sec -= 15;
        hs.categories[HealthCategory::Security] = std::max(0, sec);

        if (mal.activeCount > 0)
            hs.errors.push_back(std::to_string(mal.activeCount) + " active malware threat(s) detected");
        if (mal.heuristicFlags > 0)
            hs.warnings.push_back(std::to_string(mal.heuristicFlags) + " malware heuristic indicator(s) flagged");
        if (mal.enginePresent && !mal.realTimeProtection)
            hs.warnings.push_back("Antivirus real-time protection is off");
        for (auto& rec : mal.recommendations) hs.recommendations.push_back(rec);
        hs.compute(); // recompute overall with adjusted Security score
    }

    UI::showHealthScore(hs);

    // Malware summary
    renderMalwareResult(mal);

    // Export
    if (g_config.exportOnExit) {
        exportAll(hs, sr, start, hw, net, evt, perf, rel, winfo, corr, repSession);
    }

    Logger::instance().info("Full scan completed. Health score: " + std::to_string(hs.overall) + "%", "scan");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Health Scan (Option 2)
// ─────────────────────────────────────────────
static void optionHealthScan() {
    UI::showSectionHeader("Health Scan");
    Logger::instance().info("Health scan started", "scan");

    Timer scanTimer;
    Spinner spin("Running quick health assessment (parallel)...");
    spin.start();

    // Quick checks only, all in parallel. Storage runs in quick mode
    // (drive totals only — no full-volume file walk).
    auto fSr   = std::async(std::launch::async, []{ return guarded("Storage",     []{ StorageAnalyzer s(g_config, g_platform);    return s.analyze(false); }); });
    auto fHw   = std::async(std::launch::async, []{ return guarded("Hardware",    []{ HardwareScanner s(g_config, g_platform);    return s.scan(); }); });
    auto fNet  = std::async(std::launch::async, []{ return guarded("Network",     []{ NetworkScanner s(g_config, g_platform);     return s.scan(); }); });
    auto fPerf = std::async(std::launch::async, []{ return guarded("Performance", []{ PerformanceMonitor s(g_config, g_platform); return s.measure(); }); });
    auto fRel  = std::async(std::launch::async, []{ return guarded("Reliability", []{ ReliabilityAnalyzer s(g_config, g_platform);return s.analyze(); }); });
    auto fWin  = std::async(std::launch::async, []{ return guarded("WindowsInfo", []{ WindowsInfoScanner s(g_config, g_platform); return s.scan(); }); });
    auto fCorr = std::async(std::launch::async, []{ return guarded("Corruption",  []{ CorruptionScanner s(g_config, g_platform);  return s.scan(); }); });
    auto fUpd  = std::async(std::launch::async, []{ return guarded("Updates",     []{ UpdateManager s(g_config, g_platform);      return s.detectUpdates(); }); });

    auto sr    = fSr.get();
    auto hw    = fHw.get();
    auto net   = fNet.get();
    auto perf  = fPerf.get();
    auto rel   = fRel.get();
    auto winfo = fWin.get();
    auto corr  = fCorr.get();
    auto upd   = fUpd.get();

    StartupResult emptyStart;
    EventLogSummary emptyEvt;
    RepairSession emptyRep;

    spin.setLabel("Health scan finished in " + Platform::formatDuration(scanTimer.elapsedMs()));
    spin.stop();

    auto hsScore = computeHealthScore(sr, emptyStart, hw, net, emptyEvt, perf, rel, winfo, corr, upd, emptyRep);
    UI::showHealthScore(hsScore);

    if (g_config.exportOnExit) {
        exportAll(hsScore, sr, emptyStart, hw, net, emptyEvt, perf, rel, winfo, corr, emptyRep);
    }

    Logger::instance().info("Health scan completed. Score: " + std::to_string(hsScore.overall) + "%", "scan");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Render a MalwareResult to the console
// ─────────────────────────────────────────────
static void renderMalwareResult(const MalwareResult& mr) {
    // Verdict banner
    std::vector<std::string> lines;
    lines.push_back("Verdict: " + mr.verdict + "   (threat score " + std::to_string(mr.threatScore) + "/100)");
    lines.push_back("Scan type: " + mr.scanType);
    if (mr.scanRan)
        lines.push_back("Engine scan ran in " + std::to_string((int)mr.scanDurationSec) + "s");
    if (mr.verdict == "Infected")      UI::showErrorPanel("Malware Scan", lines);
    else if (mr.verdict == "Suspicious") UI::showWarningPanel("Malware Scan", lines);
    else                                 UI::showSuccessPanel("Malware Scan", lines);

    // Engine status
    UI::showSectionHeader("Antivirus Engine");
    UI::printKV("Engine", mr.engineName + (mr.enginePresent ? "" : " (not available)"));
    if (mr.enginePresent) {
        UI::printKVColored("Real-time protection", mr.realTimeProtection ? "On" : "OFF",
                           mr.realTimeProtection ? Color::BrightGreen : Color::BrightRed);
        UI::printKVColored("Antivirus enabled", mr.antivirusEnabled ? "Yes" : "NO",
                           mr.antivirusEnabled ? Color::BrightGreen : Color::BrightRed);
        if (mr.tamperProtection)
            UI::printKVColored("Tamper protection", "On", Color::BrightGreen);
        if (mr.definitionAgeDays >= 0)
            UI::printKVColored("Definitions age", std::to_string(mr.definitionAgeDays) + " day(s)",
                               mr.definitionAgeDays > 3 ? Color::Yellow : Color::BrightGreen);
        if (!mr.signatureVersion.empty()) UI::printKV("Signature version", mr.signatureVersion);
        if (!mr.engineVersion.empty())    UI::printKV("Engine version", mr.engineVersion);
        if (!mr.lastQuickScan.empty())    UI::printKV("Last quick scan", mr.lastQuickScan);
        if (!mr.lastFullScan.empty())     UI::printKV("Last full scan", mr.lastFullScan);
    }

    // Active threats
    if (!mr.activeThreats.empty()) {
        UI::showSectionHeader("Active Threats (" + std::to_string(mr.activeThreats.size()) + ")");
        for (auto& t : mr.activeThreats) {
            UI::printError(t.name + "  [" + t.severity + "]  " + t.status);
            if (!t.path.empty())     UI::printDim("    " + t.path);
            if (!t.detected.empty()) UI::printDim("    detected: " + t.detected);
        }
    } else if (mr.enginePresent) {
        UI::printSuccess("No active threats reported by " + mr.engineName);
    }

    // Heuristic indicators
    int heur = mr.heuristicFlags;
    if (heur > 0) {
        UI::showSectionHeader("Threat Hunting Indicators (" + std::to_string(heur) + ")");
        for (auto& p : mr.suspiciousProcesses)   UI::printError("Process running from Temp: " + p);
        for (auto& p : mr.unsignedProcesses)     UI::printError("Unsigned process in user path: " + p);
        for (auto& l : mr.lolbinProcesses)       UI::printError("Script host / LOLBin abuse: " + l);
        for (auto& s : mr.suspiciousServices)    UI::printError("Service from user-writable path: " + s);
        for (auto& w : mr.wmiPersistence)        UI::printError("WMI persistence: " + w);
        for (auto& a : mr.suspiciousAutoruns)    UI::printWarning("Suspicious autorun: " + a);
        for (auto& t : mr.suspiciousTasks)       UI::printWarning("Suspicious scheduled task: " + t);
        for (auto& c : mr.suspiciousConnections) UI::printWarning("Outbound connection from user path: " + c);
        for (auto& e : mr.defenderExclusions)    UI::printWarning("Defender exclusion: " + e);
        if (mr.defenderTampered)                 UI::printError("Defender protection layers are DISABLED");
        if (mr.hostsFileModified)                UI::printWarning("Hosts file contains non-loopback redirects");
    } else {
        UI::printSuccess("No suspicious processes, services, persistence, autoruns, tasks, or hosts entries found");
    }

    // System integrity (deep scan only)
    if (mr.integrityChecked) {
        UI::showSectionHeader("System Integrity");
        if (mr.integrityViolations) {
            for (auto& d : mr.integrityDetails) UI::printError(d);
        } else {
            for (auto& d : mr.integrityDetails) UI::printSuccess(d);
            if (mr.integrityDetails.empty())
                UI::printSuccess("No system-file or component-store corruption detected");
        }
    }

    // History (resolved) — brief
    if (!mr.history.empty()) {
        UI::showSectionHeader("Resolved Detections (" + std::to_string(mr.history.size()) + ")");
        size_t shown = std::min((size_t)5, mr.history.size());
        for (size_t i = 0; i < shown; ++i)
            UI::printDim("• " + mr.history[i].name + "  [" + mr.history[i].status + "]");
        if (mr.history.size() > shown)
            UI::printDim("  ...and " + std::to_string(mr.history.size() - shown) + " more");
    }

    // Recommendations
    if (!mr.recommendations.empty()) {
        UI::showSectionHeader("Recommendations");
        for (auto& rec : mr.recommendations) UI::printInfo("→ " + rec);
    }
}

// ─────────────────────────────────────────────
//  Menu: Malware Scan (Option 3)
// ─────────────────────────────────────────────
static void optionMalwareScan() {
    UI::showSectionHeader("Malware Scan");
    UI::printDim("Drives the system antivirus (Windows Defender) plus fast local heuristics.");
    UI::printDim("This tool only inspects — removal is handled by the AV engine, which you control.");
    UI::newLine();

    std::vector<std::string> modes = {
        "1. Quick Assessment  (status + threat history + heuristics — seconds)",
        "2. Quick Scan        (Defender quick scan of common infection points)",
        "3. Full System Scan  (Defender scans every file — can take a long time)",
        "4. Back to Main Menu"
    };
    for (auto& m : modes) UI::printDim(m);
    auto choice = UI::promptMenu({"1-4"}, "Select scan mode");
    if (choice == 4) return;

    MalwareScanDepth depth = MalwareScanDepth::None;
    if (choice == 2) depth = MalwareScanDepth::Quick;
    else if (choice == 3) {
        depth = MalwareScanDepth::Full;
        if (!UI::promptYesNo("A full system scan can take 30+ minutes and use significant CPU/disk. Continue?", false))
            return;
    }

    Logger::instance().info("Malware scan requested (mode " + std::to_string(choice) + ")", "malware");

    auto progress = [](int pct, const std::string& status) {
        static ProgressBar pb;
        if (pct <= 5) { pb = ProgressBar{0, 100, 40, "Malware scan"}; }
        pb.update(pct, status);
        if (pct >= 100) pb.finish("Malware scan complete");
    };

    MalwareScanner scanner(g_config, g_platform);
    auto result = scanner.scan(depth, progress);

    UI::newLine();
    renderMalwareResult(result);

    Logger::instance().info("Malware scan verdict: " + result.verdict, "malware");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Repair Windows (Option 4)
// ─────────────────────────────────────────────
static void optionRepairWindows() {
    UI::showSectionHeader("Windows Repair");

    if (!g_platform.elevated) {
        UI::printWarning("Repair operations require administrator privileges.");
        if (UI::promptYesNo("Attempt to re-launch as administrator?")) {
            if (Platform::requestElevation()) return;
        }
        UI::printError("Please re-run as Administrator.");
        UI::pause();
        return;
    }

    bool doRestorePoint = UI::promptYesNo("Create a system restore point before repairs?", true);

    WindowsRepair wr(g_config, g_platform);
    RepairSession session;

    UI::printInfo("Starting system repair pipeline...");

    auto progress = [](int pct, const std::string& status) {
        static ProgressBar pb;
        if (pct == 0) { pb = ProgressBar{0, 100, 40, "Repair Progress"}; }
        pb.update(pct, status);
        if (pct == 100) pb.finish("Repair pipeline complete");
    };

    session = wr.runFullRepair(doRestorePoint, progress);

    UI::newLine();
    UI::showRepairPanel("Repair Results", {
        "Passed: " + std::to_string(session.totalPass),
        "Failed: " + std::to_string(session.totalFail),
        "Skipped: " + std::to_string(session.totalSkip),
        "Duration: " + Platform::formatDuration(session.totalMs)
    });

    if (session.rebootNeeded)
        UI::showWarningPanel("Reboot Required", {"Some repairs require a system restart to take effect."});

    Logger::instance().info("Repair session: " + std::to_string(session.totalPass) + " passed, " +
                            std::to_string(session.totalFail) + " failed", "repair");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Startup Analysis (Option 4)
// ─────────────────────────────────────────────
static void optionStartupAnalysis() {
    UI::showSectionHeader("Startup Analysis");
    Logger::instance().info("Startup analysis started", "scan");

    Spinner spin("Analyzing startup entries...");
    spin.start();
    StartupAnalyzer sa(g_config, g_platform);
    auto result = sa.analyze();
    spin.stop();

    UI::printSuccess("Startup analysis complete");
    UI::printKV("Total entries", std::to_string(result.totalCount));
    UI::printKV("Enabled", std::to_string(result.enabledCount));
    UI::printKVColored("Disabled", std::to_string(result.disabledCount), Color::Yellow);
    if (result.brokenCount > 0)
        UI::printKVColored("Broken entries (missing exe)", std::to_string(result.brokenCount), Color::BrightRed);
    if (result.unsignedCount > 0)
        UI::printKVColored("Unsigned executables", std::to_string(result.unsignedCount), Color::BrightRed);
    UI::newLine();

    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Storage Analyzer (Option 5)
// ─────────────────────────────────────────────
static void optionStorageAnalyzer() {
    UI::showSectionHeader("Storage Analyzer");

    StorageAnalyzer sa(g_config, g_platform);
    auto progress = [](int pct, const std::string& status) {
        static ProgressBar pb;
        if (pct == 0) { pb = ProgressBar{0, 100, 40, "Scanning"}; }
        pb.update(pct, status);
        if (pct == 100) pb.finish();
    };

    UI::printInfo("Analyzing disk usage...");
    auto result = sa.analyze(true, progress);

    UI::showSectionHeader("Storage Overview");
    for (auto& d : result.drives) {
        double pct = d.totalBytes > 0 ? (100.0 * d.usedBytes / d.totalBytes) : 0;
        std::ostringstream line;
        line << d.mountPoint << " (" << d.label << ") — "
             << Platform::formatBytes(d.usedBytes) << " / " << Platform::formatBytes(d.totalBytes)
             << " (" << (int)pct << "% used)";
        std::string driveInfo = line.str();
        if (pct > 90) UI::printError(driveInfo);
        else if (pct > 75) UI::printWarning(driveInfo);
        else UI::printSuccess(driveInfo);
        UI::printDim("  Type: " + d.driveType + " | FS: " + d.fileSystem +
                     " | SMART: " + (d.smartOk ? "OK" : "BAD") +
                     " | BitLocker: " + (d.bitlocker ? "On" : "Off"));
    }

    UI::newLine();
    UI::printKV("Total files", std::to_string(result.totalFiles));
    UI::printKV("Total folders", std::to_string(result.totalFolders));
    UI::printKV("Hidden files", std::to_string(result.hiddenFiles));
    UI::printKV("System files", std::to_string(result.systemFiles));
    UI::printKV("Largest file", result.largestFile + " (" + Platform::formatBytes(result.largestFileBytes) + ")");
    if (result.reclaimableBytes > 0)
        UI::printKVColored("Estimated reclaimable", Platform::formatBytes(result.reclaimableBytes), Color::BrightGreen);

    UI::newLine();
    if (result.topFiles.size() > 0) {
        UI::showSectionHeader("Top 10 Largest Files");
        for (size_t i = 0; i < std::min((size_t)10, result.topFiles.size()); ++i)
            UI::printKV(std::to_string(i+1) + ".", Platform::formatBytes(result.topFiles[i].second) + "  " + result.topFiles[i].first);
    }

    Logger::instance().info("Storage analysis: " + std::to_string(result.totalFiles) + " files", "scan");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Network Diagnostics (Option 6)
// ─────────────────────────────────────────────
static void optionNetworkDiagnostics() {
    UI::showSectionHeader("Network Diagnostics");
    Logger::instance().info("Network diagnostics started", "scan");

    Spinner spin("Diagnosing network...");
    spin.start();
    NetworkScanner ns(g_config, g_platform);
    auto result = ns.scan();
    spin.stop();

    for (auto& a : result.adapters) {
        if (!a.connected) continue;
        UI::printSuccess(a.name + " (" + a.type + ")");
        UI::printKV("IP", a.ipv4, 20);
        UI::printKV("MAC", a.mac, 20);
        UI::printKV("DNS", a.dns, 20);
        UI::printKV("Speed", std::to_string((int)a.speedMbps) + " Mbps", 20);
        UI::newLine();
    }

    UI::printKV("Public IP", result.publicIp);
    UI::printKV("Gateway", result.gateway);
    if (result.pingMs > 0) {
        std::string pingStr = std::to_string((int)result.pingMs) + " ms";
        if (result.pingMs > 100) UI::printKVColored("Ping", pingStr, Color::BrightRed);
        else UI::printKV("Ping", pingStr);
    }
    UI::printKV("Packet Loss", std::to_string((int)result.packetLoss) + "%");
    UI::printKV("DNS Latency", std::to_string((int)result.dnsLatencyMs) + " ms");
    UI::printKV("VPN Active", result.vpnActive ? "Yes" : "No");
    UI::printKV("Firewall", result.firewallActive ? "Active" : "Inactive");

    Logger::instance().info("Network diagnostics complete", "scan");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Hardware Report (Option 7)
// ─────────────────────────────────────────────
static void optionHardwareReport() {
    UI::showSectionHeader("Hardware Report");
    Logger::instance().info("Hardware scan started", "scan");

    Spinner spin("Scanning hardware...");
    spin.start();
    HardwareScanner hs(g_config, g_platform);
    auto result = hs.scan();
    spin.stop();

    // CPU
    UI::printBold("CPU");
    UI::printKV("Model", result.cpu.model);
    UI::printKV("Architecture", result.cpu.architecture);
    UI::printKV("Cores", std::to_string(result.cpu.cores));
    UI::printKV("Threads", std::to_string(result.cpu.threads));
    {
        std::ostringstream freq;
        freq << std::fixed << std::setprecision(2) << result.cpu.frequencyGHz << " GHz";
        UI::printKV("Frequency", freq.str());
    }
    UI::printKV("Usage", std::to_string((int)result.cpu.usagePercent) + "%");
    if (result.cpu.temperatureC > 0)
        UI::printKVColored("Temperature", std::to_string((int)result.cpu.temperatureC) + "°C",
                          result.cpu.temperatureC > 85 ? Color::BrightRed : Color::BrightGreen);
    UI::newLine();

    // Memory
    UI::printBold("Memory");
    UI::printKV("Total", Platform::formatBytes(result.memory.totalBytes));
    UI::printKV("Available", Platform::formatBytes(result.memory.availableBytes));
    double memPct = result.memory.totalBytes > 0 ? (100.0 - 100.0 * result.memory.availableBytes / result.memory.totalBytes) : 0;
    if (memPct > 85) UI::printKVColored("Usage", std::to_string((int)memPct) + "%", Color::BrightRed);
    else UI::printKV("Usage", std::to_string((int)memPct) + "%");
    if (result.memory.slots > 0) UI::printKV("Slots", std::to_string(result.memory.slots));
    if (result.memory.speedMHz > 0) UI::printKV("Speed", std::to_string((int)result.memory.speedMHz) + " MHz");
    UI::newLine();

    // GPU
    for (size_t i = 0; i < result.gpus.size(); ++i) {
        UI::printBold("GPU " + std::to_string(i+1));
        UI::printKV("Model", result.gpus[i].name);
        UI::printKV("Driver", result.gpus[i].driverVersion);
        if (result.gpus[i].vramBytes > 0)
            UI::printKV("VRAM", Platform::formatBytes(result.gpus[i].vramBytes));
        UI::newLine();
    }

    // System
    UI::printBold("System");
    UI::printKV("Motherboard", result.motherboard);
    UI::printKV("BIOS", result.biosVersion);
    UI::printKV("UEFI", result.uefi ? "Yes" : "No");
    UI::printKV("Secure Boot", result.secureBoot ? "Enabled" : (g_platform.id == OS::Windows ? "Disabled" : "N/A"));
    UI::printKV("TPM", result.tpmPresent ? "Present" : "Not found");
    if (result.batteryPercent >= 0)
        UI::printKV("Battery", std::to_string((int)result.batteryPercent) + "%");

    Logger::instance().info("Hardware scan complete", "scan");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Windows Update (Option 8)
// ─────────────────────────────────────────────
static void optionWindowsUpdate() {
    UI::showSectionHeader("Windows Update");

    if (g_platform.id != OS::Windows) {
        UI::printWarning("Windows Update management is only available on Windows.");
        UI::pause();
        return;
    }

    if (!g_platform.elevated) {
        UI::printWarning("Admin rights are needed for update operations.");
        if (UI::promptYesNo("Elevate?")) {
            if (Platform::requestElevation()) return;
        }
    }

    Spinner spin("Checking for updates...");
    spin.start();
    UpdateManager um(g_config, g_platform);
    auto result = um.detectUpdates();
    spin.stop();

    UI::printSuccess("Update check complete");
    UI::printKV("Total available", std::to_string(result.totalAvailable));
    UI::printKV("Security", std::to_string(result.security.size()));
    UI::printKV("Quality", std::to_string(result.quality.size()));
    UI::printKV("Feature", std::to_string(result.feature.size()));
    UI::printKV("Driver", std::to_string(result.drivers.size()));
    UI::printKV("Optional", std::to_string(result.optional.size()));
    UI::printKV("Reboot pending", result.rebootPending ? "Yes" : "No");
    UI::printKV("Last checked", result.lastCheckDate);
    UI::newLine();

    if (!result.failedUpdates.empty()) {
        UI::showWarningPanel("Failed Updates", result.failedUpdates);
    }

    if (result.totalAvailable > 0 && g_platform.elevated) {
        if (UI::promptYesNo("Install available updates now?")) {
            Spinner installSpin("Installing updates...");
            installSpin.start();
            auto r = um.installAll();
            installSpin.stop(r.success);

            if (r.success) {
                UI::printSuccess("Updates installed successfully");
                if (r.rebootNeeded)
                    UI::showWarningPanel("Restart Required", {"Please restart your system to complete update installation."});
            } else {
                UI::printError("Update installation failed: " + r.errorDetail);
            }
        }
    } else if (result.totalAvailable == 0) {
        UI::printSuccess("Your system is up to date!");
    }

    Logger::instance().info("Windows Update: " + std::to_string(result.totalAvailable) + " available", "scan");
    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Export Report (Option 9)
// ─────────────────────────────────────────────
static void optionExportReport() {
    UI::showSectionHeader("Export Report");

    UI::printInfo("Collecting data for export (parallel)...");

    Spinner spin("Gathering system data...");
    spin.start();

    auto fSr   = std::async(std::launch::async, []{ return guarded("Storage",     []{ StorageAnalyzer s(g_config, g_platform);    return s.analyze(); }); });
    auto fHw   = std::async(std::launch::async, []{ return guarded("Hardware",    []{ HardwareScanner s(g_config, g_platform);    return s.scan(); }); });
    auto fNet  = std::async(std::launch::async, []{ return guarded("Network",     []{ NetworkScanner s(g_config, g_platform);     return s.scan(); }); });
    auto fPerf = std::async(std::launch::async, []{ return guarded("Performance", []{ PerformanceMonitor s(g_config, g_platform); return s.measure(); }); });
    auto fRel  = std::async(std::launch::async, []{ return guarded("Reliability", []{ ReliabilityAnalyzer s(g_config, g_platform);return s.analyze(); }); });
    auto fWin  = std::async(std::launch::async, []{ return guarded("WindowsInfo", []{ WindowsInfoScanner s(g_config, g_platform); return s.scan(); }); });
    auto fCorr = std::async(std::launch::async, []{ return guarded("Corruption",  []{ CorruptionScanner s(g_config, g_platform);  return s.scan(); }); });
    auto fUpd  = std::async(std::launch::async, []{ return guarded("Updates",     []{ UpdateManager s(g_config, g_platform);      return s.detectUpdates(); }); });

    auto sr    = fSr.get();
    auto hw    = fHw.get();
    auto net   = fNet.get();
    auto perf  = fPerf.get();
    auto rel   = fRel.get();
    auto winfo = fWin.get();
    auto corr  = fCorr.get();
    auto upd   = fUpd.get();
    spin.stop();

    StartupResult emptyStart;
    EventLogSummary emptyEvt;
    RepairSession emptyRep;

    auto hsScore = computeHealthScore(sr, emptyStart, hw, net, emptyEvt, perf, rel, winfo, corr, upd, emptyRep);
    exportAll(hsScore, sr, emptyStart, hw, net, emptyEvt, perf, rel, winfo, corr, emptyRep);

    UI::pause();
}

// ─────────────────────────────────────────────
//  Menu: Advanced Tools (Option 10)
// ─────────────────────────────────────────────
static void optionAdvancedTools() {
    UI::showSectionHeader("Advanced Tools");

    std::vector<std::string> tools = {
        "1. DISM CheckHealth (quick check)",
        "2. DISM ScanHealth (deep scan)",
        "3. DISM RestoreHealth (repair)",
        "4. SFC /scannow",
        "5. CHKDSK scan (read-only)",
        "6. Schedule CHKDSK /f /r (on next reboot)",
        "7. Flush DNS & Reset Network",
        "8. Clean Temp Files (all)",
        "9. Empty Recycle Bin",
        "10. Create Restore Point",
        "11. Check Restore Points",
        "12. Verify Services",
        "13. Verify Windows Activation",
        "14. Reset Windows Update Cache (requires confirmation)",
        "15. Registry Scan (orphaned/broken entries, read-only)",
        "16. Driver Scan (state + signatures)",
        "17. Process Tree",
        "18. Disk Benchmark (C:)",
        "19. Full System Benchmark (CPU / RAM / disk / network)",
        "20. Back to Main Menu"
    };

    WindowsRepair wr(g_config, g_platform);

    while (true) {
        for (auto& t : tools) UI::printDim(t);
        auto choice = UI::promptMenu({"1-20"}, "Select tool");

        auto doWithSpin = [](const std::string& label, auto func) {
            Spinner spin(label);
            spin.start();
            auto result = func();
            spin.stop(result.success);
            if (!result.success && !result.skipped)
                UI::printError(result.errorDetail.empty() ? "Operation failed" : result.errorDetail);
            if (result.rebootNeeded)
                UI::printWarning("A reboot may be needed");
        };

        switch (choice) {
            case 1: doWithSpin("DISM CheckHealth", [&]{ return wr.dismCheckHealth(); }); break;
            case 2: doWithSpin("DISM ScanHealth", [&]{ return wr.dismScanHealth(); }); break;
            case 3: doWithSpin("DISM RestoreHealth", [&]{ return wr.dismRestoreHealth(); }); break;
            case 4: doWithSpin("SFC /scannow", [&]{ return wr.runSfc(); }); break;
            case 5: doWithSpin("CHKDSK C:", [&]{ return wr.chkdskScan(); }); break;
            case 6: doWithSpin("Schedule CHKDSK", [&]{ return wr.scheduleChkdsk(); }); break;
            case 7:
                doWithSpin("Flush DNS", [&]{ return wr.flushDns(); });
                doWithSpin("Renew IP", [&]{ return wr.renewIp(); });
                doWithSpin("Reset Network Stack", [&]{ return wr.networkStackReset(); });
                break;
            case 8:
                doWithSpin("System Temp", [&]{ return wr.cleanWinTemp(); });
                doWithSpin("User Temp", [&]{ return wr.cleanUserTemp(); });
                doWithSpin("Temp Folders", [&]{ return wr.cleanTempFolders(); });
                break;
            case 9: doWithSpin("Empty Recycle Bin", [&]{ return wr.cleanRecycleBin(); }); break;
            case 10: doWithSpin("Create Restore Point", [&]{ return wr.createRestorePoint(); }); break;
            case 11: doWithSpin("Check Restore Points", [&]{ return wr.checkRestorePoints(); }); break;
            case 12: doWithSpin("Verify Services", [&]{ return wr.repairCommonServices(); }); break;
            case 13: doWithSpin("Verify Activation", [&]{ return wr.verifyActivation(); }); break;
            case 14:
                if (UI::promptYesNo("Reset Windows Update cache? This will stop WU services and rename SoftwareDistribution.", false))
                    doWithSpin("Reset WU Cache", [&]{ return wr.resetWindowsUpdateCache(true); });
                break;
            case 15: {
                Spinner spin("Scanning registry (read-only)...");
                spin.start();
                RegistryScanner rs(g_config, g_platform);
                auto res = rs.scan(true);
                spin.stop();
                UI::printKV("Total issues", std::to_string(res.totalIssues));
                UI::printKV("Orphaned uninstall entries", std::to_string(res.orphanedKeys));
                UI::printKV("Broken COM references", std::to_string(res.brokenReferences));
                UI::printKV("Invalid shell extensions", std::to_string(res.invalidValues));
                size_t shown = std::min<size_t>(15, res.issues.size());
                for (size_t i = 0; i < shown; ++i)
                    UI::printDim("• [" + res.issues[i].issue + "] " + res.issues[i].key);
                if (res.issues.size() > shown)
                    UI::printDim("  ...and " + std::to_string(res.issues.size() - shown) + " more");
                break;
            }
            case 16: {
                Spinner spin("Scanning drivers (this can take ~1 minute)...");
                spin.start();
                DriverScanner ds(g_config, g_platform);
                auto res = ds.scan();
                spin.stop();
                UI::printKV("Total drivers", std::to_string(res.totalCount));
                UI::printKVColored("Unsigned", std::to_string(res.unsignedCount),
                                   res.unsignedCount > 0 ? Color::BrightYellow : Color::BrightGreen);
                UI::printKVColored("Stopped critical", std::to_string(res.stoppedCritical),
                                   res.stoppedCritical > 0 ? Color::BrightRed : Color::BrightGreen);
                for (auto& d : res.problematicDrivers)
                    UI::printError("Stopped critical driver: " + d.name + " (" + d.displayName + ")");
                size_t shown = std::min<size_t>(10, res.unsignedDrivers.size());
                for (size_t i = 0; i < shown; ++i)
                    UI::printWarning("Unsigned: " + res.unsignedDrivers[i].name);
                break;
            }
            case 17: {
                Spinner spin("Building process tree...");
                spin.start();
                ProcessTreeAnalyzer pta(g_config, g_platform);
                auto forest = pta.buildTree();
                spin.stop();
                pta.printTree(forest);
                break;
            }
            case 18: {
                UI::printInfo("Benchmarking C: — writes a temporary 512 MB file.");
                if (!UI::promptYesNo("Continue?", true)) break;
                DiskBenchmark db(g_config, g_platform);
                ProgressBar pb{0, 100, 40, "Disk benchmark"};
                auto res = db.run("C:", [&](int pct, const std::string& s) { pb.update(pct, s); });
                pb.finish("Disk benchmark complete");
                UI::printKV("Sequential read",  std::to_string((int)res.sequentialReadMBps)  + " MB/s");
                UI::printKV("Sequential write", std::to_string((int)res.sequentialWriteMBps) + " MB/s");
                UI::printKV("Random read",      std::to_string((int)res.randomReadIOPS)      + " IOPS");
                UI::printKV("Random write",     std::to_string((int)res.randomWriteIOPS)     + " IOPS");
                UI::printKV("Avg latency",      std::to_string(res.averageLatencyMs)         + " ms");
                break;
            }
            case 19: {
                UI::printInfo("Runs disk, network, CPU and memory benchmarks (~1-2 minutes, downloads a 10 MB test file).");
                if (!UI::promptYesNo("Continue?", true)) break;
                SystemBenchmark sb(g_config, g_platform);
                Spinner spin("Running full system benchmark...");
                spin.start();
                auto res = sb.runAll();
                spin.stop();
                UI::printKV("CPU score",     std::to_string((int)res.cpuScore));
                UI::printKV("Memory",        std::to_string((int)res.memoryScore) + " MB/s");
                UI::printKV("Disk seq read", std::to_string((int)res.disk.sequentialReadMBps) + " MB/s");
                UI::printKV("Disk seq write",std::to_string((int)res.disk.sequentialWriteMBps) + " MB/s");
                UI::printKV("Download",      std::to_string((int)res.network.downloadMbps) + " Mbps");
                UI::printKV("Latency",       std::to_string((int)res.network.latencyMs) + " ms (jitter " +
                                             std::to_string((int)res.network.jitterMs) + " ms)");
                UI::printKVColored("Overall score", std::to_string((int)res.overallScore) + "/100",
                                   res.overallScore >= 75 ? Color::BrightGreen :
                                   res.overallScore >= 50 ? Color::BrightYellow : Color::BrightRed);
                break;
            }
            case 20: return;
            default: break;
        }
        if (choice != 20) UI::pause();
    }
}

// ─────────────────────────────────────────────
//  Main menu
// ─────────────────────────────────────────────
static void menuMain() {
    std::vector<std::string> mainOptions = {
        "1. Full Scan",
        "2. Health Scan",
        "3. Malware Scan  (full-system antivirus)",
        "4. Repair Windows",
        "5. Startup Analysis",
        "6. Storage Analyzer",
        "7. Network Diagnostics",
        "8. Hardware Report",
        "9. Windows Update",
        "10. Export Report",
        "11. Advanced Tools",
        "12. Exit"
    };

    while (true) {
        UI::showBanner();

        auto info = g_platform;
        UI::printDim("Platform: " + info.osName + " | Host: " + info.hostname + " | User: " + info.username);
        UI::printKV("Elevated", info.elevated ? "Yes ✓" : "No ✗");
        UI::printKV("Session", g_sessionId);
        UI::showSeparator();

        for (auto& opt : mainOptions)
            UI::printDim(opt);

        auto choice = UI::promptMenu({"1-12"}, "Enter choice");

        switch (choice) {
            case 1:  optionFullScan(); break;
            case 2:  optionHealthScan(); break;
            case 3:  optionMalwareScan(); break;
            case 4:  optionRepairWindows(); break;
            case 5:  optionStartupAnalysis(); break;
            case 6:  optionStorageAnalyzer(); break;
            case 7:  optionNetworkDiagnostics(); break;
            case 8:  optionHardwareReport(); break;
            case 9:  optionWindowsUpdate(); break;
            case 10: optionExportReport(); break;
            case 11: optionAdvancedTools(); break;
            case 12:
                UI::printInfo("Thank you for using System Health Toolkit v" TOOLKIT_VERSION);
                Logger::instance().info("Session ended by user", "main");
                Logger::instance().flush();
                return;
            default:
                UI::printError("Invalid choice. Please select 1-12.");
                break;
        }
    }
}

// ─────────────────────────────────────────────
//  entry point
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Handle --version / --help before creating any directories or logs
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--version" || a == "-V") {
            std::cout << "System Health Toolkit v" TOOLKIT_VERSION "\n";
            std::cout << TOOLKIT_COPYRIGHT "\n";
            return 0;
        }
        if (a == "--help" || a == "-h") {
            std::cout << "System Health Toolkit v" TOOLKIT_VERSION "\n\n";
            std::cout << "Usage: SystemHealthToolkit [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --verbose, -v        Verbose output\n";
            std::cout << "  --no-color, -n       Disable colored output\n";
            std::cout << "  --auto-repair, -r    Run automatic repair and exit\n";
            std::cout << "  --no-export          Do not auto-export reports\n";
            std::cout << "  --log-dir <path>     Custom log directory\n";
            std::cout << "  --report-dir <path>  Custom report directory\n";
            std::cout << "  --version, -V        Show version\n";
            std::cout << "  --help, -h           Show this help\n";
            return 0;
        }
    }

    // Initialize console for ANSI support
    UI::initConsole();

    // Parse config
    g_config = AppConfig::parseArgs(argc, argv);
    if (g_config.noColor) Color::disable();

    // Detect platform
    g_platform = Platform::detect();
    g_platform.elevated = Platform::isElevated();

    // Create directories
    Platform::ensureDirectories(g_config);

    // Init logger
    Logger::instance().init(g_config.logDir);

    // Session ID
    g_sessionId = Platform::timestamp();

    Logger::instance().info("System Health Toolkit v" TOOLKIT_VERSION " started", "main");
    Logger::instance().info("Platform: " + g_platform.osName + " " + g_platform.osVersion, "main");
    Logger::instance().info("Elevated: " + std::string(g_platform.elevated ? "Yes" : "No"), "main");

    // Handle --auto-repair mode (non-interactive)
    if (g_config.autoRepair) {
        UI::printInfo("Auto-repair mode enabled");
        if (!g_platform.elevated) {
            UI::printWarning("Auto-repair requires admin rights");
            if (Platform::requestElevation()) return 0;
        }
        optionRepairWindows();
        return 0;
    }

    // Start interactive menu
    menuMain();
    return 0;
}
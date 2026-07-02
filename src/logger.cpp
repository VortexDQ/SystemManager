/**
 * logger.cpp
 * Thread-safe singleton logger. Writes System.log, Repair.log,
 * Errors.log, and Scan.log with full timestamp + duration formatting.
 */

#include "../include/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────
//  Singleton
// ─────────────────────────────────────────────
Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    flush();
}

// ─────────────────────────────────────────────
//  init
// ─────────────────────────────────────────────
void Logger::init(const std::string& logDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    logDir_ = logDir;

    std::error_code ec;
    fs::create_directories(logDir, ec);

    auto open = [&](const std::string& name) {
        std::ofstream f;
        f.open(logDir + "/" + name, std::ios::app);
        return f;
    };

    systemLog_ = open("System.log");
    repairLog_ = open("Repair.log");
    errorsLog_ = open("Errors.log");
    scanLog_   = open("Scan.log");
    ready_ = true;

    // Write session separator
    std::string sep = "\n" + std::string(72, '=') + "\n";
    sep += " SESSION START  " + nowStr() + "\n";
    sep += std::string(72, '=') + "\n";
    systemLog_ << sep;
    systemLog_.flush();
}

// ─────────────────────────────────────────────
//  Public log methods
// ─────────────────────────────────────────────
void Logger::debug(const std::string& msg, const std::string& cat) {
    LogEntry e; e.timestamp = nowStr(); e.level = LogLevel::DEBUG;
    e.category = cat; e.message = msg;
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) write(e, systemLog_);
}

void Logger::info(const std::string& msg, const std::string& cat, double durationMs) {
    LogEntry e; e.timestamp = nowStr(); e.level = LogLevel::INFO;
    e.category = cat; e.message = msg; e.durationMs = durationMs;
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) write(e, systemLog_);
}

void Logger::warn(const std::string& msg, const std::string& cat) {
    LogEntry e; e.timestamp = nowStr(); e.level = LogLevel::WARN;
    e.category = cat; e.message = msg; e.success = true;
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) {
        write(e, systemLog_);
        write(e, errorsLog_);
    }
}

void Logger::error(const std::string& msg, const std::string& cat, const std::string& detail) {
    LogEntry e; e.timestamp = nowStr(); e.level = LogLevel::ERROR_;
    e.category = cat; e.message = msg; e.success = false; e.errorDetail = detail;
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) {
        write(e, systemLog_);
        write(e, errorsLog_);
    }
}

void Logger::fatal(const std::string& msg, const std::string& cat) {
    LogEntry e; e.timestamp = nowStr(); e.level = LogLevel::FATAL;
    e.category = cat; e.message = msg; e.success = false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) {
        write(e, systemLog_);
        write(e, errorsLog_);
    }
}

void Logger::logRepair(const std::string& action, bool success,
                       double durationMs, const std::string& detail) {
    LogEntry e; e.timestamp = nowStr();
    e.level      = success ? LogLevel::INFO : LogLevel::ERROR_;
    e.category   = "repair";
    e.message    = action;
    e.success    = success;
    e.durationMs = durationMs;
    e.errorDetail= detail;
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) {
        write(e, repairLog_);
        write(e, systemLog_);
        if (!success) write(e, errorsLog_);
    }
}

void Logger::logScan(const std::string& action, bool success,
                     double durationMs, const std::string& detail) {
    LogEntry e; e.timestamp = nowStr();
    e.level      = success ? LogLevel::INFO : LogLevel::WARN;
    e.category   = "scan";
    e.message    = action;
    e.success    = success;
    e.durationMs = durationMs;
    e.errorDetail= detail;
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_) {
        write(e, scanLog_);
        write(e, systemLog_);
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (systemLog_.is_open()) systemLog_.flush();
    if (repairLog_.is_open()) repairLog_.flush();
    if (errorsLog_.is_open()) errorsLog_.flush();
    if (scanLog_.is_open())   scanLog_.flush();
}

std::string Logger::errorsLogPath() const {
    return logDir_ + "/Errors.log";
}

// ─────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────
void Logger::write(const LogEntry& entry, std::ofstream& stream) {
    if (!stream.is_open()) return;
    stream << formatEntry(entry) << "\n";
    stream.flush();
}

std::string Logger::formatEntry(const LogEntry& entry) const {
    std::ostringstream oss;
    oss << "[" << entry.timestamp << "]";

    // Level tag
    switch (entry.level) {
        case LogLevel::DEBUG:  oss << " [DEBUG]"; break;
        case LogLevel::INFO:   oss << " [INFO ]"; break;
        case LogLevel::WARN:   oss << " [WARN ]"; break;
        case LogLevel::ERROR_: oss << " [ERROR]"; break;
        case LogLevel::FATAL:  oss << " [FATAL]"; break;
    }

    oss << " [" << std::setw(12) << std::left << entry.category << "] ";
    oss << entry.message;

    if (entry.durationMs >= 0.0) {
        std::ostringstream dur;
        if (entry.durationMs < 1000.0)
            dur << std::fixed << std::setprecision(1) << entry.durationMs << "ms";
        else
            dur << std::fixed << std::setprecision(2) << (entry.durationMs / 1000.0) << "s";
        oss << "  (" << dur.str() << ")";
    }

    if (!entry.success) oss << "  [FAILED]";

    if (!entry.errorDetail.empty())
        oss << "\n  >> " << entry.errorDetail;

    return oss.str();
}

std::string Logger::nowStr() const {
    using SC = std::chrono::system_clock;
    auto now = SC::now();
    auto t   = SC::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()).count() % 1000;
    std::tm tm_{};
#ifdef _WIN32
    localtime_s(&tm_, &t);
#else
    localtime_r(&t, &tm_);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_);
    std::ostringstream oss;
    oss << buf << "." << std::setw(3) << std::setfill('0') << ms;
    return oss.str();
}
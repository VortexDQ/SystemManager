/**
 * logger.h
 * Thread-safe singleton logger with log level, category filtering,
 * and automatic writing to System.log, Repair.log, Errors.log, Scan.log.
 */

#pragma once

#include "toolkit.h"
#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel { DEBUG, INFO, WARN, ERROR_, FATAL };

struct LogEntry {
    std::string timestamp;
    LogLevel    level     = LogLevel::INFO;
    std::string category  = "general";
    std::string message;
    bool        success   = true;
    double      durationMs = -1.0;
    std::string errorDetail;
};

class Logger {
public:
    static Logger& instance();

    void init(const std::string& logDir);

    // Log at various levels
    void debug(const std::string& msg, const std::string& cat = "debug");
    void info(const std::string& msg, const std::string& cat = "info",
              double durationMs = -1.0);
    void warn(const std::string& msg, const std::string& cat = "warn");
    void error(const std::string& msg, const std::string& cat = "error",
               const std::string& detail = "");
    void fatal(const std::string& msg, const std::string& cat = "fatal");

    // Semantic logging
    void logRepair(const std::string& action, bool success,
                   double durationMs, const std::string& detail = "");
    void logScan(const std::string& action, bool success,
                 double durationMs, const std::string& detail = "");

    void flush();
    std::string errorsLogPath() const;

    ~Logger();

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void write(const LogEntry& entry, std::ofstream& stream);
    std::string formatEntry(const LogEntry& entry) const;
    std::string nowStr() const;

    std::string     logDir_;
    std::ofstream   systemLog_;
    std::ofstream   repairLog_;
    std::ofstream   errorsLog_;
    std::ofstream   scanLog_;
    std::mutex      mutex_;
    bool            ready_ = false;
};
/**
 * platform.h
 * Cross-platform utilities: OS detection, privilege checks,
 * directory management, command execution, file/format helpers.
 */

#pragma once

#include "toolkit.h"
#include <string>
#include <vector>

namespace Platform {
    // Detect current platform
    PlatformInfo detect();

    // Privilege
    bool isElevated();
    bool requestElevation();

    // Directory management
    void ensureDirectories(const AppConfig& cfg);

    // Execute a shell command
    CmdResult exec(const std::string& cmd, int timeoutSeconds = 60);

    // Path helpers
    std::string getHomePath();
    std::string getTempPath();
    std::string getSystemRoot();
    std::string getExePath();
    std::string getBaseDir();

    // File helpers
    bool        fileExists(const std::string& path);
    bool        dirExists(const std::string& path);
    bool        createDir(const std::string& path);
    uint64_t    fileSize(const std::string& path);
    uint64_t    dirSize(const std::string& path);

    // Formatting
    std::string formatBytes(uint64_t bytes);
    std::string formatDuration(double ms);
    std::string timestamp();
    std::string timestampHuman();

    // Directory listing
    std::vector<std::string> listDir(const std::string& path);
    std::vector<std::string> walkDir(const std::string& path);
} // namespace Platform
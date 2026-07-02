/**
 * platform.cpp
 * Cross-platform implementation: OS detection, privilege checks,
 * directory management, command execution, and file utilities.
 */

#include "../include/platform.h"
#include "../include/toolkit.h"
#include "../include/logger.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <array>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────
//  Platform-specific includes
// ─────────────────────────────────────────────
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <shlobj.h>
  #include <tlhelp32.h>
  #include <lmcons.h>
  #pragma comment(lib, "shell32.lib")
#elif defined(__APPLE__)
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/utsname.h>
  #include <mach-o/dyld.h>
  #include <sys/statvfs.h>
  #include <pwd.h>
#else
  // Linux
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/utsname.h>
  #include <sys/statvfs.h>
  #include <pwd.h>
  #include <limits.h>
#endif

// ─────────────────────────────────────────────
//  AppConfig
// ─────────────────────────────────────────────
AppConfig makeDefaultConfig() {
    AppConfig cfg;
    cfg.baseDir   = Platform::getBaseDir();
    cfg.logDir    = cfg.baseDir + "/Logs";
    cfg.reportDir = cfg.baseDir + "/Reports";
    cfg.tempDir   = cfg.baseDir + "/Temp";
    cfg.configDir = cfg.baseDir + "/Config";
    cfg.assetsDir = cfg.baseDir + "/Assets";
    return cfg;
}

AppConfig AppConfig::defaults() {
    return makeDefaultConfig();
}

AppConfig AppConfig::parseArgs(int argc, char* argv[]) {
    AppConfig cfg = makeDefaultConfig();
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose"     || arg == "-v") cfg.verbose      = true;
        if (arg == "--no-color"    || arg == "-n") cfg.noColor      = true;
        if (arg == "--auto-repair" || arg == "-r") cfg.autoRepair   = true;
        if (arg == "--no-export"              )    cfg.exportOnExit = false;
        if ((arg == "--log-dir" || arg == "-l") && i + 1 < argc)
            cfg.logDir = argv[++i];
        if ((arg == "--report-dir") && i + 1 < argc)
            cfg.reportDir = argv[++i];
    }
    return cfg;
}

// ─────────────────────────────────────────────
//  Platform::detect
// ─────────────────────────────────────────────
PlatformInfo Platform::detect() {
    PlatformInfo info;

#ifdef _WIN32
    info.id        = OS::Windows;
    info.supported = true;

    // OS version via RtlGetVersion
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    // Use the registry as RtlGetVersion requires ntdll link
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR buf[256]; DWORD len = sizeof(buf);
        if (RegQueryValueExW(hKey, L"ProductName", nullptr, nullptr,
                             (LPBYTE)buf, &len) == ERROR_SUCCESS)
            info.osName = std::string(buf, buf + wcslen(buf));
        len = sizeof(buf);
        if (RegQueryValueExW(hKey, L"CurrentBuildNumber", nullptr, nullptr,
                             (LPBYTE)buf, &len) == ERROR_SUCCESS)
            info.osBuild = std::string(buf, buf + wcslen(buf));
        len = sizeof(buf);
        if (RegQueryValueExW(hKey, L"DisplayVersion", nullptr, nullptr,
                             (LPBYTE)buf, &len) == ERROR_SUCCESS)
            info.osVersion = std::string(buf, buf + wcslen(buf));
        RegCloseKey(hKey);
    }
    if (info.osName.empty()) info.osName = "Windows";

    // The registry ProductName still says "Windows 10" on Windows 11
    // (Microsoft never updated it) — derive the real name from the build.
    try {
        if (!info.osBuild.empty() && std::stoi(info.osBuild) >= 22000) {
            auto pos = info.osName.find("Windows 10");
            if (pos != std::string::npos)
                info.osName.replace(pos, 10, "Windows 11");
        }
    } catch (...) {}

    // Architecture
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: info.arch = "x86_64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: info.arch = "arm64";  break;
        case PROCESSOR_ARCHITECTURE_INTEL: info.arch = "x86";    break;
        default: info.arch = "unknown";
    }

    // Hostname
    WCHAR hname[MAX_COMPUTERNAME_LENGTH + 1]; DWORD hlen = sizeof(hname)/sizeof(WCHAR);
    if (GetComputerNameW(hname, &hlen))
        info.hostname = std::string(hname, hname + wcslen(hname));

    // Username
    WCHAR uname[UNLEN + 1]; DWORD ulen = sizeof(uname)/sizeof(WCHAR);
    if (GetUserNameW(uname, &ulen))
        info.username = std::string(uname, uname + wcslen(uname));

    // VM detection (very basic: check for VM-specific registry keys)
    HKEY hVm;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SYSTEM\\CurrentControlSet\\Services\\vmbus", 0, KEY_READ, &hVm) == ERROR_SUCCESS) {
        info.isVM = true;
        RegCloseKey(hVm);
    }

#elif defined(__APPLE__)
    info.id        = OS::macOS;
    info.supported = true;

    struct utsname u{};
    uname(&u);
    info.osVersion = u.release;
    info.arch      = u.machine;
    info.hostname  = u.nodename;

    // Get macOS version from sw_vers
    FILE* f = popen("sw_vers -productVersion 2>/dev/null", "r");
    if (f) {
        char buf[64]; if (fgets(buf, sizeof(buf), f)) {
            info.osVersion = buf;
            info.osVersion.erase(info.osVersion.find_last_not_of("\n\r ") + 1);
        }
        pclose(f);
    }
    info.osName = "macOS " + info.osVersion;

    // Username
    struct passwd* pw = getpwuid(getuid());
    if (pw) info.username = pw->pw_name;

#else
    // Linux
    info.id        = OS::Linux;
    info.supported = true;

    struct utsname u{};
    uname(&u);
    info.osBuild   = u.release;
    info.arch      = u.machine;
    info.hostname  = u.nodename;

    // Read /etc/os-release for distro name
    std::ifstream osrel("/etc/os-release");
    if (osrel.is_open()) {
        std::string line;
        while (std::getline(osrel, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                info.osName = line.substr(12);
                // Strip quotes
                info.osName.erase(remove(info.osName.begin(), info.osName.end(), '"'),
                                  info.osName.end());
            }
            if (line.rfind("VERSION_ID=", 0) == 0) {
                info.osVersion = line.substr(11);
                info.osVersion.erase(remove(info.osVersion.begin(), info.osVersion.end(), '"'),
                                     info.osVersion.end());
            }
        }
    }
    if (info.osName.empty()) info.osName = "Linux " + std::string(u.release);

    // Username
    struct passwd* pw = getpwuid(getuid());
    if (pw) info.username = pw->pw_name;

    // Detect container
    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string content((std::istreambuf_iterator<char>(cgroup)),
                             std::istreambuf_iterator<char>());
        if (content.find("docker") != std::string::npos ||
            content.find("lxc")    != std::string::npos)
            info.isContainer = true;
    }
    if (fileExists("/.dockerenv")) info.isContainer = true;
#endif

    return info;
}

// ─────────────────────────────────────────────
//  Platform::isElevated
// ─────────────────────────────────────────────
bool Platform::isElevated() {
#ifdef _WIN32
    BOOL elevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION te{};
        DWORD size = sizeof(te);
        if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &size))
            elevated = te.TokenIsElevated;
        CloseHandle(token);
    }
    return elevated == TRUE;
#else
    return geteuid() == 0;
#endif
}

// ─────────────────────────────────────────────
//  Platform::requestElevation (Windows only)
// ─────────────────────────────────────────────
bool Platform::requestElevation() {
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    SHELLEXECUTEINFOA sei{};
    sei.cbSize       = sizeof(sei);
    sei.lpVerb       = "runas";
    sei.lpFile       = exePath;
    sei.lpParameters = "";
    sei.nShow        = SW_NORMAL;

    if (ShellExecuteExA(&sei)) {
        return true; // New elevated process started; caller should exit
    }
    return false;
#else
    // On Unix, advise user to use sudo
    std::cout << "Please re-run with: sudo " << getExePath() << "\n";
    return false;
#endif
}

// ─────────────────────────────────────────────
//  Platform::ensureDirectories
// ─────────────────────────────────────────────
void Platform::ensureDirectories(const AppConfig& cfg) {
    std::vector<std::string> dirs = {
        cfg.logDir, cfg.reportDir, cfg.tempDir,
        cfg.configDir, cfg.assetsDir
    };
    for (const auto& d : dirs) {
        if (!d.empty()) {
            std::error_code ec;
            fs::create_directories(d, ec);
        }
    }
}

// ─────────────────────────────────────────────
//  Platform::exec
// ─────────────────────────────────────────────
#ifdef _WIN32
// Windows command execution with a REAL timeout. The child (and every
// process it spawns — powershell.exe forks conhost/wmiprvse helpers) is
// placed in a job object and killed together if it overruns. stdin is
// bound to NUL so a tool that blocks waiting for input can never hang us.
static CmdResult execWindows(const std::string& cmd, int timeoutSeconds) {
    CmdResult result;
    Timer t;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        result.exitCode = -1;
        result.stdErr = "CreatePipe failed";
        return result;
    }
    // The read end must not be inherited by the child.
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul = CreateFileA("NUL", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji{};
        ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji));
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = wr;
    si.hStdError  = wr;
    si.hStdInput  = (nul != INVALID_HANDLE_VALUE) ? nul : nullptr;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::string full = "cmd /C " + cmd;
    std::vector<char> cmdline(full.begin(), full.end());
    cmdline.push_back('\0');

    BOOL ok = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);

    CloseHandle(wr); // parent keeps only the read end
    if (!ok) {
        if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        if (job) CloseHandle(job);
        CloseHandle(rd);
        result.exitCode = -1;
        result.stdErr = "CreateProcess failed (" + std::to_string(GetLastError()) + ")";
        return result;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);

    // Drain stdout+stderr on a background thread so a full pipe buffer
    // can never deadlock the child while we wait.
    std::string output;
    std::thread reader([&]() {
        char buf[8192];
        DWORD n = 0;
        while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n > 0)
            output.append(buf, n);
    });

    DWORD waitMs = timeoutSeconds > 0
        ? static_cast<DWORD>(timeoutSeconds) * 1000u : INFINITE;
    DWORD w = WaitForSingleObject(pi.hProcess, waitMs);
    if (w == WAIT_TIMEOUT) {
        result.timedOut = true;
        if (job) TerminateJobObject(job, 1);
        else TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
    }

    // Reader returns once every write handle (child + our copy) is closed.
    if (reader.joinable()) reader.join();

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    result.exitCode = result.timedOut ? -2 : static_cast<int>(code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (job) CloseHandle(job);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    CloseHandle(rd);

    result.stdOut = output;
    result.durationMs = t.elapsedMs();
    if (result.timedOut)
        Logger::instance().warn("Command timed out after " +
            std::to_string(timeoutSeconds) + "s: " + cmd.substr(0, 80), "exec");
    return result;
}
#endif

CmdResult Platform::exec(const std::string& cmd, int timeoutSeconds) {
#ifdef _WIN32
    return execWindows(cmd, timeoutSeconds);
#else
    CmdResult result;
    Timer t;
    // Best-effort timeout via coreutils `timeout` when present.
    std::string fullCmd = "timeout " + std::to_string(timeoutSeconds > 0 ? timeoutSeconds : 60) +
                          " sh -c " + "'" + cmd + "' 2>&1";
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        // Fall back to running without the timeout wrapper.
        pipe = popen((cmd + " 2>&1").c_str(), "r");
    }
    if (!pipe) {
        result.exitCode = -1;
        result.stdErr   = "Failed to open pipe for command: " + cmd;
        return result;
    }
    std::array<char, 4096> buf;
    std::string output;
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr)
        output += buf.data();
    int status = pclose(pipe);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (result.exitCode == 124) result.timedOut = true; // `timeout` exit code
    result.stdOut    = output;
    result.durationMs = t.elapsedMs();
    return result;
#endif
}

// ─────────────────────────────────────────────
//  Path helpers
// ─────────────────────────────────────────────
std::string Platform::getHomePath() {
#ifdef _WIN32
    WCHAR path[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, path) == S_OK)
        return std::string(path, path + wcslen(path));
    return "C:\\Users\\Default";
#else
    const char* home = getenv("HOME");
    if (home) return home;
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/tmp";
#endif
}

std::string Platform::getTempPath() {
#ifdef _WIN32
    WCHAR path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    return std::string(path, path + wcslen(path));
#else
    const char* tmp = getenv("TMPDIR");
    return tmp ? tmp : "/tmp";
#endif
}

std::string Platform::getSystemRoot() {
#ifdef _WIN32
    WCHAR buf[MAX_PATH];
    GetWindowsDirectoryW(buf, MAX_PATH);
    return std::string(buf, buf + wcslen(buf));
#elif defined(__APPLE__)
    return "/";
#else
    return "/";
#endif
}

std::string Platform::getExePath() {
#ifdef _WIN32
    WCHAR buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::string(buf, buf + wcslen(buf));
#elif defined(__APPLE__)
    char buf[1024]; uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) return buf;
    return "";
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) { buf[len] = '\0'; return buf; }
    return "";
#endif
}

std::string Platform::getBaseDir() {
    std::string exe = getExePath();
    if (exe.empty()) return ".";
    fs::path p(exe);
    return p.parent_path().string();
}

// ─────────────────────────────────────────────
//  File helpers
// ─────────────────────────────────────────────
bool Platform::fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

bool Platform::dirExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_directory(path, ec);
}

bool Platform::createDir(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec);
}

uint64_t Platform::fileSize(const std::string& path) {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    return ec ? 0 : static_cast<uint64_t>(sz);
}

uint64_t Platform::dirSize(const std::string& path) {
    uint64_t total = 0;
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(path,
             fs::directory_options::skip_permission_denied, ec)) {
        if (e.is_regular_file(ec))
            total += e.file_size(ec);
    }
    return total;
}

// ─────────────────────────────────────────────
//  Format helpers
// ─────────────────────────────────────────────
std::string Platform::formatBytes(uint64_t bytes) {
    const char* units[] = {"B","KB","MB","GB","TB","PB"};
    double val = static_cast<double>(bytes);
    int i = 0;
    while (val >= 1024.0 && i < 5) { val /= 1024.0; ++i; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << val << " " << units[i];
    return oss.str();
}

std::string Platform::formatDuration(double ms) {
    if (ms < 1000.0)  return std::to_string(static_cast<int>(ms)) + "ms";
    double s = ms / 1000.0;
    if (s < 60.0)     return std::to_string(static_cast<int>(s)) + "s";
    int m = static_cast<int>(s) / 60;
    int rs = static_cast<int>(s) % 60;
    return std::to_string(m) + "m " + std::to_string(rs) + "s";
}

std::string Platform::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_{}; 
#ifdef _WIN32
    localtime_s(&tm_, &t);
#else
    localtime_r(&t, &tm_);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_);
    return buf;
}

std::string Platform::timestampHuman() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_{};
#ifdef _WIN32
    localtime_s(&tm_, &t);
#else
    localtime_r(&t, &tm_);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_);
    return buf;
}

std::vector<std::string> Platform::listDir(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(path,
             fs::directory_options::skip_permission_denied, ec)) {
        out.push_back(e.path().string());
    }
    return out;
}

std::vector<std::string> Platform::walkDir(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(path,
             fs::directory_options::skip_permission_denied, ec)) {
        if (e.is_regular_file(ec))
            out.push_back(e.path().string());
    }
    return out;
}

// ─────────────────────────────────────────────
//  HealthScore::compute
// ─────────────────────────────────────────────
void HealthScore::compute() {
    if (categories.empty()) { overall = 100; grade = "A+"; return; }
    int sum = 0;
    for (auto& [cat, score] : categories) sum += score;
    overall = sum / static_cast<int>(categories.size());
    overall = std::max(0, std::min(100, overall));
    grade = gradeStr();
}

std::string HealthScore::gradeStr() {
    if (overall >= 97) return "A+";
    if (overall >= 93) return "A";
    if (overall >= 90) return "A-";
    if (overall >= 87) return "B+";
    if (overall >= 83) return "B";
    if (overall >= 80) return "B-";
    if (overall >= 77) return "C+";
    if (overall >= 73) return "C";
    if (overall >= 70) return "C-";
    if (overall >= 60) return "D";
    return "F";
}
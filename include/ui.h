/**
 * ui.h
 * Console UI system: ANSI colors, progress bars, animated spinners,
 * formatted tables, panels, health score display, and input prompts.
 */

#pragma once

#include "toolkit.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace Color {
    extern const char* Reset;
    extern const char* Bold;
    extern const char* Dim;
    extern const char* Red;
    extern const char* Green;
    extern const char* Yellow;
    extern const char* Blue;
    extern const char* Magenta;
    extern const char* Cyan;
    extern const char* White;
    extern const char* Gray;
    extern const char* BgRed;
    extern const char* BgGreen;
    extern const char* BgYellow;
    extern const char* BgBlue;
    extern const char* BrightRed;
    extern const char* BrightGreen;
    extern const char* BrightYellow;
    extern const char* BrightCyan;
    extern const char* BrightWhite;

    void disable();
    bool enabled();
} // namespace Color

struct TableColumn {
    std::string header;
    int         width      = 20;
    bool        rightAlign = false;
};

class Table {
public:
    std::vector<TableColumn> columns;
    std::vector<std::vector<std::string>> rows;

    void addRow(const std::vector<std::string>& row);
    void print() const;
};

class ProgressBar {
public:
    int    current  = 0;
    int    total    = 100;
    int    width    = 30;
    std::string label;
    Timer  timer;

    void render() const;
    void update(int value, const std::string& status = "");
    void finish(const std::string& msg = "");
};

class Spinner {
public:
    explicit Spinner(const std::string& label);
    ~Spinner();

    void start();
    void stop(bool success = true);
    void setLabel(const std::string& label);

private:
    void spin();
    std::thread       thread_;
    std::string       label_;
    std::mutex        labelMutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<bool> success_{true};
};

// Console UI namespace
namespace UI {
    void initConsole();
    int  terminalWidth();

    // Separators & headers
    void showSeparator(const char* ch = "─", int width = 0);
    void showBanner();
    void showSectionHeader(const std::string& title);

    // Panels
    void showSuccessPanel(const std::string& title, const std::vector<std::string>& lines);
    void showWarningPanel(const std::string& title, const std::vector<std::string>& lines);
    void showErrorPanel(const std::string& title, const std::vector<std::string>& lines);
    void showRepairPanel(const std::string& title, const std::vector<std::string>& lines);
    void showInfoPanel(const std::string& title, const std::vector<std::string>& lines);

    // Single-line output
    void printSuccess(const std::string& msg);
    void printWarning(const std::string& msg);
    void printError(const std::string& msg);
    void printInfo(const std::string& msg);
    void printDim(const std::string& msg);
    void printBold(const std::string& msg);
    void printKV(const std::string& key, const std::string& value, int keyWidth = 28);
    void printKVColored(const std::string& key, const std::string& value,
                        const char* valueColor, int keyWidth = 28);

    // Health score
    void showHealthBar(const std::string& label, int score, int width = 40);
    void showHealthScore(const HealthScore& score);
    void showStatsSummary(const HealthScore& score,
                          const std::map<std::string, std::string>& stats);

    // Input
    bool promptYesNo(const std::string& question, bool defaultYes = true);
    int  promptMenu(const std::vector<std::string>& options,
                    const std::string& prompt = "Enter choice");
    std::string promptInput(const std::string& prompt);
    void clearLine();
    void newLine();
    void pause();
} // namespace UI
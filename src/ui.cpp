/**
 * ui.cpp
 * Console UI: ANSI colors, progress bars, animated spinners,
 * formatted tables, panels, health score display.
 */

#include "../include/ui.h"
#include "../include/toolkit.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <io.h>
#else
  #include <unistd.h>
  #include <sys/ioctl.h>
#endif

// ─────────────────────────────────────────────
//  Color definitions
// ─────────────────────────────────────────────
namespace Color {
    static bool s_enabled = true;

    const char* Reset         = "\033[0m";
    const char* Bold          = "\033[1m";
    const char* Dim           = "\033[2m";
    const char* Red           = "\033[31m";
    const char* Green         = "\033[32m";
    const char* Yellow        = "\033[33m";
    const char* Blue          = "\033[34m";
    const char* Magenta       = "\033[35m";
    const char* Cyan          = "\033[36m";
    const char* White         = "\033[37m";
    const char* Gray          = "\033[90m";
    const char* BgRed         = "\033[41m";
    const char* BgGreen       = "\033[42m";
    const char* BgYellow      = "\033[43m";
    const char* BgBlue        = "\033[44m";
    const char* BrightRed     = "\033[91m";
    const char* BrightGreen   = "\033[92m";
    const char* BrightYellow  = "\033[93m";
    const char* BrightCyan    = "\033[96m";
    const char* BrightWhite   = "\033[97m";

    void disable() { s_enabled = false; }
    bool enabled() { return s_enabled; }
}

// ─────────────────────────────────────────────
//  Internal color helper (respects disable flag)
// ─────────────────────────────────────────────
static std::string C(const char* code) {
    return Color::enabled() ? code : "";
}

// ─────────────────────────────────────────────
//  Approximate display width of a UTF-8 string.
//  std::string::size() counts bytes, so multi-byte glyphs (✓, ⚠, ⚡, emoji)
//  throw off box-drawing padding. Count code points instead, treating
//  emoji-range code points as double-width.
// ─────────────────────────────────────────────
static int displayWidth(const std::string& s) {
    int width = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp = 0;
        int len = 1;
        if      (c < 0x80)  { cp = c; len = 1; }
        else if (c < 0xE0)  { cp = c & 0x1F; len = 2; }
        else if (c < 0xF0)  { cp = c & 0x0F; len = 3; }
        else                { cp = c & 0x07; len = 4; }
        for (int k = 1; k < len && i + k < s.size(); ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        i += len;
        // Emoji & symbol blocks commonly rendered double-width
        if (cp >= 0x1F300 || (cp >= 0x2E80 && cp <= 0xA4CF)) width += 2;
        else width += 1;
    }
    return width;
}

// Truncate a UTF-8 string to at most `maxWidth` display columns
// without cutting a code point in half.
static std::string truncateToWidth(const std::string& s, int maxWidth) {
    int width = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        int w = 1;
        if (len == 4) w = 2;
        if (width + w > maxWidth) break;
        width += w;
        i += len;
    }
    return s.substr(0, i);
}

// ─────────────────────────────────────────────
//  UI::initConsole
// ─────────────────────────────────────────────
void UI::initConsole() {
#ifdef _WIN32
    // Enable Virtual Terminal Processing for ANSI sequences on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
            SetConsoleMode(hOut, mode);
        }
    }
    // Set console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// ─────────────────────────────────────────────
//  Terminal width
// ─────────────────────────────────────────────
int UI::terminalWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return w.ws_col;
#endif
    return 80;
}

// ─────────────────────────────────────────────
//  Separator
// ─────────────────────────────────────────────
void UI::showSeparator(const char* ch, int width) {
    if (width <= 0) width = std::min(terminalWidth(), 80);
    std::string line;
    line.reserve(width * 3);
    for (int i = 0; i < width; ++i) line += ch;
    std::cout << C(Color::Gray) << line << C(Color::Reset) << "\n";
}

// ─────────────────────────────────────────────
//  Banner
// ─────────────────────────────────────────────
void UI::showBanner() {
    const int w = 62; // inner width
    auto centered = [&](const std::string& text) {
        int pad = w - displayWidth(text);
        if (pad < 0) pad = 0;
        int left = pad / 2;
        return "  ║" + std::string(left, ' ') + text + std::string(pad - left, ' ') + "║\n";
    };
    auto edge = [&](const char* l, const char* r) {
        std::string s = std::string("  ") + l;
        for (int i = 0; i < w; ++i) s += "═";
        s += r;
        s += "\n";
        return s;
    };
    std::cout << "\n";
    std::cout << C(Color::BrightCyan) << C(Color::Bold);
    std::cout << edge("╔", "╗");
    std::cout << centered("SYSTEM HEALTH TOOLKIT  v" TOOLKIT_VERSION);
    std::cout << centered("Cross-Platform  ·  C++20  ·  Open Source");
    std::cout << edge("╚", "╝");
    std::cout << C(Color::Reset);
    std::cout << C(Color::Gray) << "  " << TOOLKIT_AUTHOR << C(Color::Reset) << "\n\n";
}

// ─────────────────────────────────────────────
//  Section header
// ─────────────────────────────────────────────
void UI::showSectionHeader(const std::string& title) {
    std::cout << "\n";
    showSeparator("─", 70);
    std::cout << C(Color::Bold) << C(Color::BrightCyan)
              << "  ▶  " << title
              << C(Color::Reset) << "\n";
    showSeparator("─", 70);
}

// ─────────────────────────────────────────────
//  Panels
// ─────────────────────────────────────────────
static void printPanel(const std::string& title,
                        const std::vector<std::string>& lines,
                        const char* borderColor,
                        const char* titleColor,
                        const char* icon) {
    int w = std::min(UI::terminalWidth() - 4, 76);
    auto bar = [&](const char* l, const char* r) {
        std::string s = l;
        for (int i = 0; i < w; ++i) s += "─";
        s += r;
        return s;
    };
    std::cout << C(borderColor) << "  " << bar("┌", "┐") << "\n";
    std::string header = std::string(icon) + " " + title;
    int pad = w - 2 - displayWidth(header);
    if (pad < 0) pad = 0;
    std::cout << "  │ " << C(titleColor) << C(Color::Bold) << header
              << C(Color::Reset) << C(borderColor) << std::string(pad + 1, ' ') << "│\n";
    std::cout << "  " << bar("├", "┤") << "\n";
    for (const auto& line : lines) {
        std::string display = line;
        if (displayWidth(display) > w - 2)
            display = truncateToWidth(display, w - 5) + "...";
        int lpad = w - 2 - displayWidth(display);
        if (lpad < 0) lpad = 0;
        std::cout << "  │ " << C(Color::Reset) << display
                  << C(borderColor) << std::string(lpad + 1, ' ') << "│\n";
    }
    std::cout << "  " << bar("└", "┘") << "\n" << C(Color::Reset);
}

void UI::showSuccessPanel(const std::string& title, const std::vector<std::string>& lines) {
    printPanel(title, lines, Color::BrightGreen, Color::BrightGreen, "✓");
}
void UI::showWarningPanel(const std::string& title, const std::vector<std::string>& lines) {
    printPanel(title, lines, Color::BrightYellow, Color::BrightYellow, "⚠");
}
void UI::showErrorPanel(const std::string& title, const std::vector<std::string>& lines) {
    printPanel(title, lines, Color::BrightRed, Color::BrightRed, "✗");
}
void UI::showRepairPanel(const std::string& title, const std::vector<std::string>& lines) {
    printPanel(title, lines, Color::Cyan, Color::Cyan, "🔧");
}
void UI::showInfoPanel(const std::string& title, const std::vector<std::string>& lines) {
    printPanel(title, lines, Color::Blue, Color::BrightCyan, "ℹ");
}

// ─────────────────────────────────────────────
//  Line-level output
// ─────────────────────────────────────────────
void UI::printSuccess(const std::string& msg) {
    std::cout << C(Color::BrightGreen) << "  ✓  " << C(Color::Reset) << msg << "\n";
}
void UI::printWarning(const std::string& msg) {
    std::cout << C(Color::BrightYellow) << "  ⚠  " << C(Color::Reset) << msg << "\n";
}
void UI::printError(const std::string& msg) {
    std::cout << C(Color::BrightRed) << "  ✗  " << C(Color::Reset) << msg << "\n";
}
void UI::printInfo(const std::string& msg) {
    std::cout << C(Color::BrightCyan) << "  ℹ  " << C(Color::Reset) << msg << "\n";
}
void UI::printDim(const std::string& msg) {
    std::cout << C(Color::Dim) << "     " << msg << C(Color::Reset) << "\n";
}
void UI::printBold(const std::string& msg) {
    std::cout << C(Color::Bold) << "  " << msg << C(Color::Reset) << "\n";
}

void UI::printKV(const std::string& key, const std::string& value, int keyWidth) {
    std::cout << "  " << C(Color::Cyan) << std::left << std::setw(keyWidth)
              << key << C(Color::Reset) << "  " << value << "\n";
}

void UI::printKVColored(const std::string& key, const std::string& value,
                         const char* valueColor, int keyWidth) {
    std::cout << "  " << C(Color::Cyan) << std::left << std::setw(keyWidth)
              << key << C(Color::Reset) << "  "
              << C(valueColor) << value << C(Color::Reset) << "\n";
}

// ─────────────────────────────────────────────
//  Health bar
// ─────────────────────────────────────────────
void UI::showHealthBar(const std::string& label, int score, int width) {
    score = std::max(0, std::min(100, score));
    int filled = (score * width) / 100;

    // Choose color by score
    const char* barColor = Color::BrightGreen;
    if (score < 50)      barColor = Color::BrightRed;
    else if (score < 75) barColor = Color::BrightYellow;
    else if (score < 90) barColor = Color::Cyan;

    std::cout << "  " << C(Color::Cyan) << std::left << std::setw(26) << label
              << C(Color::Reset) << " [";
    std::cout << C(barColor);
    for (int i = 0; i < width; ++i)
        std::cout << (i < filled ? "█" : "░");
    std::cout << C(Color::Reset) << "] ";
    std::cout << C(barColor) << C(Color::Bold) << std::right << std::setw(3) << score << "%"
              << C(Color::Reset) << "\n";
}

// ─────────────────────────────────────────────
//  Health score display
// ─────────────────────────────────────────────
void UI::showHealthScore(const HealthScore& score) {
    showSectionHeader("Overall Health Score");

    // Big score display
    const char* gradeColor = Color::BrightGreen;
    if (score.overall < 50)      gradeColor = Color::BrightRed;
    else if (score.overall < 75) gradeColor = Color::BrightYellow;
    else if (score.overall < 90) gradeColor = Color::Cyan;

    std::cout << "\n";
    std::cout << "          " << C(gradeColor) << C(Color::Bold)
              << std::setw(3) << score.overall << "% "
              << C(Color::Reset) << C(gradeColor) << " Grade: "
              << C(Color::Bold) << score.grade
              << C(Color::Reset) << "\n\n";

    // Per-category bars
    auto catName = [](HealthCategory c) -> std::string {
        switch(c) {
            case HealthCategory::WindowsIntegrity: return "Windows Integrity";
            case HealthCategory::Storage:          return "Storage";
            case HealthCategory::Memory:           return "Memory";
            case HealthCategory::Network:          return "Network";
            case HealthCategory::Security:         return "Security";
            case HealthCategory::Drivers:          return "Drivers";
            case HealthCategory::Startup:          return "Startup";
            case HealthCategory::Updates:          return "Updates";
            case HealthCategory::Hardware:         return "Hardware";
            case HealthCategory::EventLogs:        return "Event Logs";
            case HealthCategory::Performance:      return "Performance";
            case HealthCategory::Reliability:      return "Reliability";
            default: return "Unknown";
        }
    };

    for (auto& [cat, s] : score.categories)
        showHealthBar(catName(cat), s);

    std::cout << "\n";

    // Warnings
    if (!score.warnings.empty()) {
        showWarningPanel("Warnings", score.warnings);
    }
    // Errors
    if (!score.errors.empty()) {
        showErrorPanel("Issues Found", score.errors);
    }
    // Recommendations
    if (!score.recommendations.empty()) {
        showRepairPanel("Recommendations", score.recommendations);
    }
}

// ─────────────────────────────────────────────
//  Stats summary
// ─────────────────────────────────────────────
void UI::showStatsSummary(const HealthScore& score,
                           const std::map<std::string, std::string>& stats) {
    showSectionHeader("Session Summary");
    std::cout << "\n";
    for (auto& [k, v] : stats)
        printKV(k, v);
    std::cout << "\n";
    showHealthScore(score);
}

// ─────────────────────────────────────────────
//  Prompt helpers
// ─────────────────────────────────────────────
bool UI::promptYesNo(const std::string& question, bool defaultYes) {
    std::string hint = defaultYes ? "[Y/n]" : "[y/N]";
    std::cout << C(Color::BrightYellow) << "\n  ? " << C(Color::Reset)
              << question << " " << C(Color::Dim) << hint << C(Color::Reset) << " ";
    std::string input;
    std::getline(std::cin, input);
    if (input.empty()) return defaultYes;
    char c = static_cast<char>(std::tolower(input[0]));
    return c == 'y';
}

int UI::promptMenu(const std::vector<std::string>& options, const std::string& prompt) {
    // Callers pass the accepted range as a single "lo-hi" string (e.g. {"1-12"}).
    // Parse it so any value in [lo, hi] is accepted; fall back to [1, count]
    // when a real list of option labels is supplied instead.
    int lo = 1, hi = static_cast<int>(options.size());
    if (options.size() == 1) {
        const std::string& spec = options[0];
        auto dash = spec.find('-');
        if (dash != std::string::npos) {
            try {
                lo = std::stoi(spec.substr(0, dash));
                hi = std::stoi(spec.substr(dash + 1));
            } catch (...) { lo = 1; hi = 1; }
        }
    }

    while (true) {
        std::cout << C(Color::BrightCyan) << "\n  " << prompt << ": " << C(Color::Reset);
        std::string input;
        if (!std::getline(std::cin, input)) return hi; // EOF: treat as last option (exit)
        // Trim surrounding whitespace.
        size_t a = input.find_first_not_of(" \t\r\n");
        size_t b = input.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        input = input.substr(a, b - a + 1);
        try {
            int choice = std::stoi(input);
            if (choice >= lo && choice <= hi)
                return choice;
        } catch (...) {}
        std::cout << C(Color::BrightRed) << "  Invalid selection. Try again.\n" << C(Color::Reset);
    }
}

std::string UI::promptInput(const std::string& prompt) {
    std::cout << C(Color::BrightCyan) << "  " << prompt << ": " << C(Color::Reset);
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void UI::clearLine() {
    std::cout << "\r\033[2K" << std::flush;
}

void UI::newLine() {
    std::cout << "\n";
}

void UI::pause() {
    std::cout << C(Color::Dim) << "\n  Press Enter to continue..." << C(Color::Reset);
    std::string tmp;
    std::getline(std::cin, tmp);
}

// ─────────────────────────────────────────────
//  Progress bar
// ─────────────────────────────────────────────
void ProgressBar::render() const {
    int filled = total > 0 ? (current * width) / total : 0;
    filled = std::max(0, std::min(width, filled));

    const char* barColor = Color::enabled() ? "\033[96m" : "";
    const char* dimColor = Color::enabled() ? "\033[90m" : "";
    const char* reset    = Color::enabled() ? "\033[0m"  : "";
    const char* bold     = Color::enabled() ? "\033[1m"  : "";

    double elapsed = timer.elapsedSec();
    double eta     = 0.0;
    if (current > 0 && total > current)
        eta = (elapsed / current) * (total - current);

    int pct = total > 0 ? (current * 100) / total : 0;

    // Build progress string
    std::ostringstream oss;
    oss << "\r  " << bold << std::left << std::setw(20) << label << reset
        << " " << barColor << "[";
    for (int i = 0; i < width; ++i)
        oss << (i < filled ? "█" : dimColor + std::string("░") + barColor);
    oss << reset << barColor << "]" << reset
        << bold << " " << std::right << std::setw(3) << pct << "%" << reset;

    // Elapsed / ETA
    oss << dimColor << "  " << std::fixed << std::setprecision(1)
        << elapsed << "s elapsed";
    if (eta > 0.0) oss << " | ~" << std::setprecision(0) << eta << "s remaining";
    oss << reset;

    std::cout << oss.str() << std::flush;
}

void ProgressBar::update(int value, const std::string& status) {
    current = std::max(0, std::min(total, value));
    if (!status.empty()) label = status;
    render();
}

void ProgressBar::finish(const std::string& msg) {
    current = total;
    render();
    std::cout << "\n";
    if (!msg.empty())
        std::cout << C(Color::BrightGreen) << "  ✓  " << C(Color::Reset) << msg << "\n";
}

// ─────────────────────────────────────────────
//  Spinner
// ─────────────────────────────────────────────
Spinner::Spinner(const std::string& label) : label_(label) {}

Spinner::~Spinner() {
    // Only clean up if the caller forgot to stop() — never double-print.
    if (running_) stop(true);
}

void Spinner::start() {
    if (running_) return;
    stopped_ = false;
    running_ = true;
    thread_  = std::thread(&Spinner::spin, this);
}

void Spinner::stop(bool success) {
    if (stopped_.exchange(true)) return; // already reported
    success_ = success;
    running_ = false;
    if (thread_.joinable()) thread_.join();

    std::string label;
    {
        std::lock_guard<std::mutex> lock(labelMutex_);
        label = label_;
    }
    UI::clearLine();
    if (success)
        std::cout << C(Color::BrightGreen) << "  ✓  " << C(Color::Reset) << label << "\n";
    else
        std::cout << C(Color::BrightRed)   << "  ✗  " << C(Color::Reset) << label << "\n";
    std::cout << std::flush;
}

void Spinner::setLabel(const std::string& label) {
    std::lock_guard<std::mutex> lock(labelMutex_);
    label_ = label;
}

void Spinner::spin() {
    static const char* frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
    int i = 0;
    while (running_) {
        std::string label;
        {
            std::lock_guard<std::mutex> lock(labelMutex_);
            label = label_;
        }
        std::cout << "\r\033[2K  " << C(Color::Cyan) << frames[i % 10]
                  << C(Color::Reset) << "  " << label << std::flush;
        i++;
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

// ─────────────────────────────────────────────
//  Table
// ─────────────────────────────────────────────
void Table::addRow(const std::vector<std::string>& row) {
    rows.push_back(row);
}

void Table::print() const {
    if (columns.empty()) return;

    // Compute actual widths
    std::vector<int> widths;
    for (auto& col : columns)
        widths.push_back(col.width);

    // Header
    std::cout << C(Color::Gray) << "  ";
    for (size_t i = 0; i < columns.size(); ++i) {
        std::cout << C(Color::Bold) << C(Color::Cyan)
                  << std::left << std::setw(widths[i]) << columns[i].header
                  << C(Color::Reset) << C(Color::Gray);
        if (i + 1 < columns.size()) std::cout << " │ ";
    }
    std::cout << "\n";

    // Separator
    std::cout << "  ";
    for (size_t i = 0; i < columns.size(); ++i) {
        for (int k = 0; k < widths[i]; ++k) std::cout << "─";
        if (i + 1 < columns.size()) std::cout << "─┼─";
    }
    std::cout << C(Color::Reset) << "\n";

    // Rows
    for (auto& row : rows) {
        std::cout << "  ";
        for (size_t i = 0; i < columns.size() && i < row.size(); ++i) {
            std::string cell = row[i];
            if (static_cast<int>(cell.size()) > widths[i])
                cell = cell.substr(0, widths[i] - 3) + "...";
            if (columns[i].rightAlign)
                std::cout << std::right << std::setw(widths[i]) << cell;
            else
                std::cout << std::left << std::setw(widths[i]) << cell;
            if (i + 1 < columns.size()) std::cout << C(Color::Gray) << " │ " << C(Color::Reset);
        }
        std::cout << "\n";
    }
    std::cout << C(Color::Reset);
}
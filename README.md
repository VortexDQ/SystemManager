<div align="center">
  <h1>🩺 System Health Toolkit</h1>
  <p>
    <strong>Professional-Grade Cross-Platform System Health & Repair Toolkit</strong>
  </p>
  <p>
    <a href="#-features"><img src="https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square&logo=c%2B%2B" alt="C++20"/></a>
    <a href="#-quick-start"><img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey?style=flat-square" alt="Platform"/></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="MIT License"/></a>
    <a href="#-build-from-source"><img src="https://img.shields.io/badge/build-CMake-red?style=flat-square" alt="CMake"/></a>
  </p>
  <br/>
</div>

---

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Quick Start (Windows)](#-quick-start-windows)
- [Build from Source](#-build-from-source)
- [Usage](#-usage)
- [Menu Options](#-menu-options)
- [Command-Line Arguments](#-command-line-arguments)
- [Reports & Logging](#-reports--logging)
- [Project Structure](#-project-structure)
- [Cross-Platform Notes](#-cross-platform-notes)
- [Contributing](#-contributing)
- [License](#-license)

---

## 🔍 Overview

**System Health Toolkit** is a professional-grade, cross-platform diagnostic and repair utility written in **C++20**. It provides comprehensive system health analysis, automated repair pipelines, hardware detection, network diagnostics, storage analysis, and exportable reports — all from a beautiful, interactive terminal interface.

Originally designed for Windows 10/11 with full DISM/SFC/CHKDSK support, it also runs on **Linux** and **macOS** with platform-appropriate functionality.

### Why C++?

- **10x faster** than equivalent PowerShell or Python implementations
- **Native performance** for disk scanning, file enumeration, and real-time monitoring
- **Zero dependencies** — compiles with any C++20 compiler (MSVC, GCC, Clang)
- **Single binary** deployment — no runtime required

---

## ✨ Features

### 🔬 System Scanning
- **Full Scan** — Comprehensive analysis of every subsystem (now includes a malware assessment)
- **Health Scan** — Quick health assessment with overall score
- **Malware Scan** — Full-system antivirus, driven by **Microsoft Defender** (Windows) or **ClamAV** (Linux/macOS), plus fast local heuristics:
  - AV engine status: real-time protection, tamper protection, definition age, engine/signature versions
  - Threat history: active vs. resolved detections with severity and affected paths
  - Quick, Full-system, or fast Assessment modes (Defender `Start-MpScan`)
  - Heuristic indicators: processes running from Temp, suspicious autoruns/scheduled tasks, hosts-file tampering
  - Verdict (Clean / Suspicious / Infected) with a 0–100 threat score and remediation guidance
  - **Inspection-only** — removal is delegated to the AV engine you control; the tool never deletes anything itself
- **Storage Analyzer** — Drive usage, top files/folders, duplicates, reclaimable space
- **Hardware Report** — CPU, RAM, GPU, motherboard, BIOS, TPM, Secure Boot
- **Network Diagnostics** — Adapters, IP, ping, packet loss, VPN detection, firewall
- **Startup Analysis** — Registry, folders, scheduled tasks, broken entries
- **Event Log Analysis** — Critical, error, warning counts with details
- **Performance Monitor** — CPU/RAM usage, top processes, running services
- **Reliability Analysis** — Blue screens, crashes, shutdowns, reliability index
- **Corruption Detection** — CBS, DISM, SFC, store, servicing stack, pending reboots
- **Windows Information** — Edition, version, activation, Defender, Hyper-V, WSL, Sandbox

### 🛠️ Repair Pipeline
- **DISM** — CheckHealth, ScanHealth, RestoreHealth
- **SFC** — System File Checker (`/scannow`)
- **CHKDSK** — Scan and schedule repair
- **Network** — Flush DNS, renew IP, Winsock reset, network stack reset
- **Cleanup** — Temp files, Windows temp, user temp, Recycle Bin, browser cache, Delivery Optimization
- **Services** — Verify and restart critical services (BITS, WU, CryptSvc, MSI)
- **Windows Update** — Detect, classify, and install updates
- **Restore Points** — Create and check system restore points
- **Defender** — Update virus definitions

### 📊 Health Scoring
- **12 categories** scored 0–100%
- **Overall score** with letter grade (A+ through F)
- **Color-coded** health bars
- **Warnings** and **recommendations** generated automatically

### 📈 Export & Reporting
- **TXT** — Formatted plain-text report
- **HTML** — Beautiful dark-mode report with:
  - Conic gradient health score ring
  - Collapsible sections
  - Color-coded health bars
  - Data tables and stat cards
  - Warnings, errors, and recommendations panels
- **JSON** — Machine-readable structured data
- **CSV** — Spreadsheet-compatible format
- All reports include timestamps and unique session IDs

### 🎨 Console UI
- ASCII banner with version info
- ANSI color support (Windows 10+ VT processing)
- Animated spinners for long operations
- Progress bars with elapsed/ETA time
- Formatted panels (success, warning, error, repair, info)
- Key-value display with colored values
- Interactive menu system with input validation

---

## ⚡ Quick Start (Windows)

### Method 1: Download Pre-built Binary (Easiest)

1. Go to the [Releases](https://github.com/your-username/SystemHealthToolkit/releases) page
2. Download `SystemHealthToolkit-Windows-x64.zip`
3. Extract to any folder (e.g., `C:\Tools\SystemHealthToolkit`)
4. Double-click `launcher.bat`

### Method 2: Zero-Setup — Auto-Installer (No Compiler Required)

The launcher **automatically detects missing dependencies** and installs them for you:

1. Double-click `launcher.bat`
2. If CMake is missing → **auto-downloads + installs CMake silently**
3. If no C++ compiler is found → **prompts to install MinGW-w64 or VS Build Tools**
4. Downloads and installs the chosen compiler automatically
5. Builds the toolkit from source
6. Launches the application

No manual setup, no searching for compilers — the launcher handles everything.

### Method 3: Run via Win+R (Windows Key + R)

1. Extract the toolkit to a permanent location, e.g. `C:\Tools\SHT`
2. Press **`Win + R`**, type:
   ```
   C:\Tools\SHT\launcher.bat
   ```
3. Press Enter

**Pro tip:** Add the folder to your PATH, then you can run from Win+R with just:
```
launcher.bat
```

Or create a shortcut:
1. Right-click `launcher.bat` → **Create shortcut**
2. Move shortcut to `C:\Users\YourName\AppData\Roaming\Microsoft\Windows\Start Menu`
3. Now press **`Win + R`**, type `launcher`, press Enter

### Method 3: Run as Administrator (for repairs)

Right-click `launcher.bat` → **Run as administrator**

Or from an elevated Command Prompt:
```cmd
cd C:\Path\To\SystemHealthToolkit
launcher.bat
```

### Method 4: PowerShell
```powershell
.\launcher.ps1
# Or with options:
.\launcher.ps1 -AutoRepair -NoColor
```

---

## 🔧 Build from Source

### Prerequisites

| Tool | Minimum Version | Installation |
|------|----------------|--------------|
| **CMake** | 3.16+ | [cmake.org](https://cmake.org/download/) |
| **C++ Compiler** | C++20 support | MSVC 2022, GCC 11+, Clang 14+ |
| **Git** (optional) | Any | [git-scm.com](https://git-scm.com/) |

### Windows (MSVC)

```cmd
git clone https://github.com/your-username/SystemHealthToolkit.git
cd SystemHealthToolkit
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release --target SystemHealthToolkit
```

### Windows (MinGW/GCC)

```cmd
git clone https://github.com/your-username/SystemHealthToolkit.git
cd SystemHealthToolkit
g++ -std=c++20 -O2 -pthread -I include src/*.cpp -o SystemHealthToolkit.exe
```

### Linux

```bash
git clone https://github.com/your-username/SystemHealthToolkit.git
cd SystemHealthToolkit
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target SystemHealthToolkit -- -j$(nproc)
```

### macOS

```bash
git clone https://github.com/your-username/SystemHealthToolkit.git
cd SystemHealthToolkit
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target SystemHealthToolkit -- -j$(sysctl -n hw.ncpu)
```

### Quick Compile (all platforms, no CMake)

```bash
cd SystemHealthToolkit
g++ -std=c++20 -O2 -pthread -I include src/*.cpp -o SystemHealthToolkit
```

### Run After Build

```bash
# Windows
SystemHealthToolkit.exe

# Linux/macOS
./SystemHealthToolkit

# Or use the launcher
./run.sh
```

---

## 🎮 Usage

### Interactive Mode

Simply run the executable or launcher without arguments to enter the interactive menu:

```
╔══════════════════════════════════════════════════════════════╗
║          SYSTEM HEALTH TOOLKIT  v3.0.0                      ║
║          Cross-Platform  ·  C++20  ·  Open Source           ║
╚══════════════════════════════════════════════════════════════╝

  Platform: Windows 11 Pro | Host: DESKTOP-ABC123 | User: User
  Elevated: No ✗
  Session:  20260701_104500

──────────────────────────────────────────────────────────────

  1. Full Scan
  2. Health Scan
  3. Malware Scan  (full-system antivirus)
  4. Repair Windows
  5. Startup Analysis
  6. Storage Analyzer
  7. Network Diagnostics
  8. Hardware Report
  9. Windows Update
  10. Export Report
  11. Advanced Tools
  12. Exit

  Enter choice:
```

### Non-Interactive Mode

```bash
# Auto-repair (requires admin)
SystemHealthToolkit --auto-repair

# Show version
SystemHealthToolkit --version

# Show help
SystemHealthToolkit --help
```

---

## 📖 Menu Options

| # | Option | Description |
|---|--------|-------------|
| 1 | **Full Scan** | Comprehensive analysis: storage, hardware, network, startup, events, performance, reliability, Windows info, corruption, updates, malware assessment |
| 2 | **Health Scan** | Quick health assessment with overall score |
| 3 | **Malware Scan** | Full-system antivirus via Defender/ClamAV + heuristics. Modes: Assessment (seconds), Quick Scan, Full System Scan |
| 4 | **Repair Windows** | Full repair pipeline: DISM → SFC → CHKDSK → Network → Cleanup → Services → Updates |
| 5 | **Startup Analysis** | Analyze startup programs, registry entries, broken items |
| 6 | **Storage Analyzer** | Drive usage, top files/folders, duplicates, reclaimable space |
| 7 | **Network Diagnostics** | Adapters, IP, ping, packet loss, VPN, firewall |
| 8 | **Hardware Report** | CPU, RAM, GPU, motherboard, BIOS, TPM, Secure Boot |
| 9 | **Windows Update** | Check, classify, and install updates |
| 10 | **Export Report** | Generate TXT, HTML, JSON, CSV reports |
| 11 | **Advanced Tools** | Individual repair tools (DISM, SFC, CHKDSK, DNS, cleanup, etc.) |
| 12 | **Exit** | Exit the toolkit |

---

## 🚩 Command-Line Arguments

| Argument | Short | Description |
|----------|-------|-------------|
| `--verbose` | `-v` | Enable verbose output |
| `--no-color` | `-n` | Disable ANSI color output |
| `--auto-repair` | `-r` | Run automatic repair and exit |
| `--no-export` | | Do not auto-export reports on scan |
| `--log-dir <path>` | `-l` | Custom log directory |
| `--report-dir <path>` | | Custom report directory |
| `--version` | `-V` | Show version and exit |
| `--help` | `-h` | Show help and exit |

---

## 📁 Reports & Logging

### Log Files (`Logs/`)

| File | Contents |
|------|----------|
| `System.log` | All system events with timestamps, categories, durations |
| `Repair.log` | All repair actions with success/failure status |
| `Errors.log` | All warnings and errors |
| `Scan.log` | All scan results |

Each log entry format:
```
[2026-07-01 10:45:00.123] [INFO ] [storage     ] Storage analysis started
[2026-07-01 10:45:30.456] [INFO ] [storage     ] Storage analysis complete  (30.3s)
[2026-07-01 10:45:31.000] [WARN ] [storage     ] Low disk space on C:\ (8% free)
```

### Report Files (`Reports/`)

Reports are automatically generated with unique filenames:
```
Reports/
  SystemHealth_20260701_104500.txt
  SystemHealth_20260701_104500.html
  SystemHealth_20260701_104500.json
  SystemHealth_20260701_104500.csv
```

The **HTML report** features:
- Dark theme (GitHub-inspired)
- Conic gradient health score ring
- Collapsible sections (click headers to expand/collapse)
- Color-coded health bars for each category
- Data tables for drives, hardware, network, etc.
- Stat cards for quick overview
- Warnings, errors, and recommendations panels
- Responsive design (mobile-friendly)

---

## 📂 Project Structure

```
SystemHealthToolkit/
│
├── CMakeLists.txt          # Cross-platform build system
├── README.md               # This file
├── LICENSE                 # MIT License
│
├── launcher.bat            # Windows batch launcher
├── launcher.ps1            # PowerShell launcher (cross-platform)
├── run.sh                  # Unix shell launcher (Linux/macOS)
│
├── include/                # C++ headers
│   ├── toolkit.h           # Core types, config, health score, version
│   ├── logger.h            # Thread-safe singleton logger
│   ├── platform.h          # OS detection, exec, file/format helpers
│   ├── ui.h                # Console UI: colors, progress, spinners, tables
│   ├── repair.h            # Windows repair operations & update manager
│   └── scanners.h          # All scanner classes (storage, hardware, network, etc.)
│
├── src/                    # C++ source files
│   ├── main.cpp            # Entry point, menu system, export subsystem
│   ├── logger.cpp          # Logger implementation
│   ├── platform.cpp        # Platform utilities implementation
│   ├── ui.cpp              # Console UI implementation
│   ├── repair.cpp          # Repair operations implementation
│   ├── storage.cpp         # Storage analyzer implementation
│   └── scanners.cpp        # All scanner implementations
│
├── Logs/                   # Auto-created log directory
├── Reports/                # Auto-created report directory
├── Temp/                   # Auto-created temp directory
├── Config/                 # Auto-created config directory
└── Assets/                 # Auto-created assets directory
```

---

## 🌐 Cross-Platform Notes

| Feature | Windows | Linux | macOS |
|---------|---------|-------|-------|
| Full Scan | ✅ | ✅ (subset) | ✅ (subset) |
| Health Scan | ✅ | ✅ | ✅ |
| Storage Analysis | ✅ | ✅ | ✅ |
| Hardware Detection | ✅ | ✅ | ✅ |
| Network Diagnostics | ✅ | ✅ | ✅ |
| Startup Analysis | ✅ | ⚠️ (partial) | ⚠️ (partial) |
| Event Logs | ✅ | ⚠️ (syslog) | ⚠️ (system.log) |
| Performance | ✅ | ✅ | ✅ |
| Reliability | ✅ | ⚠️ (partial) | ⚠️ (partial) |
| Windows Info | ✅ | N/A | N/A |
| Corruption Detection | ✅ | N/A | N/A |
| DISM / SFC / CHKDSK | ✅ | N/A | N/A |
| Windows Update | ✅ | N/A | N/A |
| Network Repairs | ✅ | ✅ | ✅ |
| Temp Cleanup | ✅ | ✅ | ✅ |
| Recycle Bin | ✅ | ✅ | ✅ |
| HTML Reports | ✅ | ✅ | ✅ |

**Legend:** ✅ Full support | ⚠️ Partial support | N/A Not applicable

### iOS & Android

System Health Toolkit is a **native CLI application** and cannot run directly on iOS or Android, as those platforms do not provide a terminal/shell environment with the necessary system access. A future mobile companion app could provide remote monitoring capabilities via the JSON export.

---

## 🤝 Contributing

Contributions are welcome! Here's how to get started:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Development Guidelines

- Follow the existing code style (documented, modular, error-handled)
- Use C++20 features appropriately
- Ensure cross-platform compatibility
- Add logging for all operations
- Update the README if adding new features
- Test on at least one platform before submitting

---

## 📄 License

Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for more information.

---

<div align="center">
  <sub>Built with ❤️ using C++20, CMake, and a lot of ☕</sub>
  <br/>
  <sub>© 2026 System Health Toolkit Team</sub>
</div>
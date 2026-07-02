# 🩺 System Health Toolkit — Complete Usage Guide

**Version 2.1.0** | Cross-Platform (Windows, Linux, macOS)

---

## Table of Contents

1. [Installation](#installation)
2. [Getting Started](#getting-started)
3. [Interactive Menu Reference](#interactive-menu-reference)
4. [Command-Line Reference](#command-line-reference)
5. [Full Scan Deep Dive](#full-scan-deep-dive)
6. [Repair Pipeline Deep Dive](#repair-pipeline-deep-dive)
7. [Storage Analyzer Guide](#storage-analyzer-guide)
8. [Understanding Reports](#understanding-reports)
9. [Health Score System](#health-score-system)
10. [Log Files Explained](#log-files-explained)
11. [Advanced Tools Reference](#advanced-tools-reference)
12. [Benchmarking Guide](#benchmarking-guide)
13. [Troubleshooting](#troubleshooting)
14. [FAQ](#faq)
15. [Tips & Best Practices](#tips--best-practices)

---

## Installation

### Windows

#### Option A: Download Pre-built Binary
1. Download the latest release from GitHub Releases
2. Extract to a permanent folder (e.g., `C:\Tools\SystemHealthToolkit`)
3. Run `launcher.bat`

#### Option B: Build from Source
Requirements: CMake 3.16+, C++20 compiler (MSVC, GCC, or Clang)

```cmd
git clone https://github.com/your-username/SystemHealthToolkit.git
cd SystemHealthToolkit
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release --target SystemHealthToolkit
```

#### Option C: Quick Compile (no CMake)
```cmd
cd SystemHealthToolkit
g++ -std=c++20 -O2 -pthread -I include src/*.cpp -o SystemHealthToolkit.exe
```

### Linux / macOS

```bash
git clone https://github.com/your-username/SystemHealthToolkit.git
cd SystemHealthToolkit
chmod +x run.sh
./run.sh
```

Or build manually:
```bash
cd SystemHealthToolkit
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target SystemHealthToolkit -- -j$(nproc)
```

---

## Getting Started

### First Launch

Run the toolkit without any arguments:

```bash
# Windows
launcher.bat

# Or directly
SystemHealthToolkit.exe

# Linux/macOS
./run.sh
```

You'll see the main menu:

```
╔══════════════════════════════════════════════════════════════╗
║          SYSTEM HEALTH TOOLKIT  v2.1.0                      ║
║          Cross-Platform  ·  C++20  ·  Open Source           ║
╚══════════════════════════════════════════════════════════════╝

  Platform: Windows 11 Pro | Host: DESKTOP-ABC123 | User: User
  Elevated: No ✗
  Session:  20260701_104500

──────────────────────────────────────────────────────────────

  1. Full Scan
  2. Health Scan
  3. Repair Windows
  4. Startup Analysis
  5. Storage Analyzer
  6. Network Diagnostics
  7. Hardware Report
  8. Windows Update
  9. Export Report
  10. Advanced Tools
  11. System Benchmark
  12. Exit

  Enter choice:
```

### Command-Line Quick Start

```bash
# Quick health check (non-interactive)
SystemHealthToolkit --auto-repair

# Show version
SystemHealthToolkit --version

# Get help
SystemHealthToolkit --help

# Custom directories
SystemHealthToolkit --log-dir "C:\Logs" --report-dir "C:\Reports"
```

---

## Interactive Menu Reference

### Option 1: Full Scan

**Purpose:** Complete system analysis. Scans every subsystem.

**What it checks:**
- ✅ Storage (drives, file statistics, top files/folders, duplicates)
- ✅ Hardware (CPU, RAM, GPU, motherboard, BIOS, TPM, Secure Boot)
- ✅ Network (adapters, IP, ping, packet loss, VPN, firewall)
- ✅ Startup programs (registry, folders, scheduled tasks, broken entries)
- ✅ Event logs (critical, error, warning counts)
- ✅ Performance (CPU/RAM usage, top processes, running services)
- ✅ Reliability (blue screens, crashes, shutdowns, reliability index)
- ✅ Windows information (edition, activation, Defender, Hyper-V, WSL)
- ✅ Corruption detection (CBS, DISM, SFC, store corruption)
- ✅ Windows Update status (available updates)

**Output:** Health score with per-category breakdown, warnings, recommendations

**Duration:** 1-5 minutes depending on system

**Example output:**
```
  ▶  Overall Health Score
  ─────────────────────────────────────────────────────

          97%   Grade: A+

  Windows Integrity     [████████████████████████████████░░] 95%
  Storage               [██████████████████████████████░░░░] 87%
  Memory                [██████████████████████████████████] 100%
  Network               [████████████████████████████████████] 100%
  Security              [████████████████████████████░░░░░░] 82%
  Drivers               [██████████████████████████████████] 100%
  Startup               [██████████████████████████████░░░░] 90%
  Updates               [████████████████████████████████░░] 92%
  Hardware              [████████████████████████████████░░] 95%
  Event Logs            [████████████████████████████████░░] 90%
  Performance           [██████████████████████████████████] 100%
  Reliability           [██████████████████████████████████] 98%
```

---

### Option 2: Health Scan

**Purpose:** Quick health assessment (faster than Full Scan)

**What it checks:**
- ✅ Storage (drive usage only)
- ✅ Hardware (CPU, RAM, GPU basics)
- ✅ Network (ping, packet loss)
- ✅ Performance (CPU/RAM usage)
- ✅ Reliability (blue screens, crashes)
- ✅ Windows information
- ✅ Corruption (quick CBS/DISM check)
- ✅ Windows Update (available updates)

**Duration:** 10-30 seconds

**When to use:**
- Daily health checks
- Before/after installing software
- Quick system assessment for troubleshooting

---

### Option 3: Repair Windows

**Purpose:** Automated system repair pipeline with optional restore point.

**⚠️ Requires Administrator privileges**

**Repair sequence:**
1. **(Optional)** Create system restore point
2. DISM CheckHealth
3. DISM ScanHealth
4. DISM RestoreHealth
5. SFC /scannow
6. CHKDSK scan (read-only)
7. Flush DNS cache
8. Renew IP configuration
9. Winsock reset
10. Network stack reset
11. Clean temp folders
12. Clean Windows temp
13. Clean user temp
14. Empty Recycle Bin
15. Clean Delivery Optimization cache
16. Update Defender definitions
17. Verify critical services
18. Verify page file
19. Verify Windows activation
20. Component cleanup (DISM StartComponentCleanup)

**Before you start:**
```bash
# Run as Administrator!
launcher.bat          # Then select option 3

# Or command line:
SystemHealthToolkit --auto-repair   # Must be run as admin
```

**What you'll see:**
```
  ▶  Windows Repair
  ─────────────────────────────────────────────────────

  ℹ  Starting system repair pipeline...

  Repair Progress      [████████████████████████████░░░░] 75%
  12.3s elapsed | ~4.1s remaining

  🔧 Repair Results
  ┌─────────────────────────────────────────────────────┐
  │ 🔧 Repair Results                                   │
  ├─────────────────────────────────────────────────────┤
  │ Passed:  18                                         │
  │ Failed:  0                                          │
  │ Skipped: 2                                          │
  │ Duration: 2m 34s                                    │
  └─────────────────────────────────────────────────────┘
```

---

### Option 4: Startup Analysis

**Purpose:** Analyze all programs that start automatically with Windows.

**Checks:**
- ✅ User Startup Folder
- ✅ Common Startup Folder
- ✅ Registry: `HKLM\Software\Microsoft\Windows\CurrentVersion\Run`
- ✅ Registry: `HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce`
- ✅ Registry: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- ✅ Registry: `HKLM\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Run`
- ✅ Task Manager startup entries (via WMI)
- ✅ Reports broken entries (missing executables)
- ✅ Detects duplicates

**Output:**
```
  ▶  Startup Analysis
  ─────────────────────────────────────────────────────

  ✓  Startup analysis complete
  Total entries              24
  Enabled                    24
  Disabled                   0
  Broken entries (missing)   1     ← Red flag!
  Unsigned executables       3     ← Security risk
```

**When to use:**
- Computer boots slowly
- Suspicious programs starting automatically
- After installing many applications

---

### Option 5: Storage Analyzer

**Purpose:** Detailed disk usage analysis with file statistics.

**Parallel Scanning:** Uses all CPU cores for 3-8x faster scanning on multi-core systems.

**What it displays:**
- Each drive with usage percentage (color-coded)
- Drive type, file system, SMART status, BitLocker status
- **Total files** (exact count)
- **Total folders** (exact count)
- **Hidden files** count
- **System files** count
- **Largest file** path and size
- **Estimated reclaimable** space
- **Fragmentation** estimate (ratio of small files < 4KB)
- Top 10 largest files with sizes
- Top 100 largest folders (internal)
- Duplicate file detection (by size)

**Example output:**
```
  ▶  Storage Overview
  ─────────────────────────────────────────────────────

  ✓  C:\ (Windows) — 245.50 GB / 512.00 GB (48% used)
     Type: NVMe | FS: NTFS | SMART: OK | BitLocker: Off

  Total files              1,234,567
  Total folders              234,567
  Hidden files                12,345
  System files                 8,765
  Largest file              4.50 GB  C:\pagefile.sys
  Estimated reclaimable    12.34 GB          ← Green highlight

  ▶  Top 10 Largest Files
  ─────────────────────────────────────────────────────

  1.  4.50 GB   C:\pagefile.sys
  2.  3.20 GB   C:\hiberfil.sys
  3.  2.10 GB   C:\Windows\Installer\...msi
  4.  1.80 GB   C:\Program Files\...\game.exe
  5.  1.50 GB   C:\Users\User\Downloads\...iso
```

**When to use:**
- Running out of disk space
- Finding large files to clean up
- Before upgrading storage
- Identifying fragmentation issues

---

### Option 6: Network Diagnostics

**Purpose:** Comprehensive network connectivity analysis.

**What it checks:**
- ✅ All connected network adapters (name, type, IP, MAC, DNS, speed)
- ✅ Public IP address (via api.ipify.org)
- ✅ Default gateway
- ✅ Ping latency (to 8.8.8.8)
- ✅ Packet loss percentage
- ✅ DNS latency
- ✅ VPN detection
- ✅ Firewall status

**Example output:**
```
  ▶  Network Diagnostics
  ─────────────────────────────────────────────────────

  ✓  Wi-Fi (Wireless)
  IP                      192.168.1.100
  MAC                     00-11-22-33-44-55
  DNS                     192.168.1.1
  Speed                   866 Mbps

  Public IP               203.0.113.42
  Gateway                 192.168.1.1
  Ping                    12 ms
  Packet Loss             0%
  DNS Latency             5 ms
  VPN Active              No
  Firewall                Active
```

**When to use:**
- Internet connectivity problems
- Slow network speeds
- VPN connection issues
- Troubleshooting DNS problems

---

### Option 7: Hardware Report

**Purpose:** Complete hardware inventory and status.

**What it displays:**
- **CPU:** Model, architecture, cores, threads, frequency, usage, temperature
- **Memory:** Total, available, usage, slots, speed
- **GPU(s):** Model, driver version, VRAM (for each GPU)
- **Motherboard:** Model/manufacturer
- **BIOS:** Version, UEFI status
- **Security:** Secure Boot, TPM presence
- **Battery:** Charge percentage (laptops)

**Example output:**
```
  ▶  Hardware Report
  ─────────────────────────────────────────────────────

  **CPU**
  Model                   Intel(R) Core(TM) i7-13700K
  Architecture            x64
  Cores                   16
  Threads                 24
  Frequency               5.40 GHz
  Usage                   12%
  Temperature             45°C

  **Memory**
  Total                   32.00 GB
  Available               24.50 GB
  Usage                   23%
  Slots                   2
  Speed                   5600 MHz

  **GPU 1**
  Model                   NVIDIA GeForce RTX 4080
  Driver                  546.17
  VRAM                    16.00 GB

  **System**
  Motherboard             MAG Z790 TOMAHAWK (MS-7D91)
  BIOS                    1.30 (UEFI)
  Secure Boot             Enabled
  TPM                     Present
  Battery                 85%
```

**When to use:**
- Before hardware upgrades
- Checking if system meets game/software requirements
- Diagnosing hardware issues
- System inventory

---

### Option 8: Windows Update

**Purpose:** Check and install Windows updates.

**⚠️ Install requires Administrator privileges**

**What it shows:**
- ✅ Total available updates
- ✅ Security updates count
- ✅ Quality updates count
- ✅ Feature updates count
- ✅ Driver updates count
- ✅ Optional updates count
- ✅ Reboot pending status
- ✅ Failed updates list

**Example output:**
```
  ▶  Windows Update
  ─────────────────────────────────────────────────────

  ✓  Update check complete
  Total available          8
  Security                 3
  Quality                  2
  Feature                  1
  Driver                   1
  Optional                 1
  Reboot pending           No
  Last checked             2026-07-01 10:45:00

  ?  Install available updates now? [Y/n]:
```

**When to use:**
- Weekly system maintenance
- Security vulnerability concerns
- After reformatting/reinstalling Windows

---

### Option 9: Export Report

**Purpose:** Generate complete system report in all formats.

**What it exports:**
- **TXT** — Formatted plain-text report for printing/sharing
- **HTML** — Dark-mode interactive report with:
  - Conic gradient health score ring
  - Collapsible sections (click to expand/collapse)
  - Color-coded health bars
  - Tables for drives, hardware, network
  - Stat cards with key metrics
  - Warnings, errors, recommendations panels
  - Responsive design (works on mobile)
- **JSON** — Machine-readable structured data
- **CSV** — Spreadsheet-compatible format

**Files are saved to:** `Reports/SystemHealth_YYYYMMDD_HHMMSS.*`

---

### Option 10: Advanced Tools

**Purpose:** Individual repair and diagnostic tools.

**Sub-menu:**
```
  1. DISM CheckHealth (quick check)
  2. DISM ScanHealth (deep scan)
  3. DISM RestoreHealth (repair)
  4. SFC /scannow
  5. CHKDSK scan (read-only)
  6. Schedule CHKDSK /f /r (on next reboot)
  7. Flush DNS & Reset Network
  8. Clean Temp Files (all)
  9. Empty Recycle Bin
  10. Create Restore Point
  11. Check Restore Points
  12. Verify Services
  13. Verify Windows Activation
  14. Reset Windows Update Cache (requires confirmation)
  15. Back to Main Menu
```

**Each tool shows:**
- Animated spinner while running
- Success/failure result
- Duration
- Any reboot requirements

**Example: Running SFC**
```
  ⠹  SFC /scannow...
  ✓  SFC /scannow
  ℹ  Windows Resource Protection found corrupt files and
     successfully repaired them.
```

---

### Option 11: System Benchmark

**Purpose:** Performance testing and scoring.

**What it measures:**
- **Disk:** Sequential read/write (MB/s), random IOPS (4KB), latency
- **Network:** Download speed (Mbps), latency, jitter, packet loss
- **CPU:** Prime computation score (primes found per second)
- **Memory:** Bandwidth via large buffer copy (MB/s)

**Overall Score:** 0-100 weighted average of all sub-scores

**Example output:**
```
  ▶  System Benchmark
  ─────────────────────────────────────────────────────

  ⠙  Computing CPU score...
  ⠹  Testing sequential write speed...
  ⠧  Testing random read IOPS...
  ⠋  Measuring network latency...
  ⠙  Testing download speed...

  **Disk Performance**
  Sequential Read             3400.50 MB/s
  Sequential Write            2800.20 MB/s
  Random Read IOPS            850,000
  Random Write IOPS           620,000
  Average Latency             0.05 ms

  **Network Performance**
  Download Speed              850.30 Mbps
  Latency                     12 ms
  Jitter                      1.5 ms
  Packet Loss                 0%

  **CPU Score**               15,420 primes/s
  **Memory Bandwidth**        45,200 MB/s

  **Overall Score:**          87/100
```

**Duration:** 30-90 seconds depending on disk speed

---

## Command-Line Reference

### Available Arguments

| Argument | Short | Description | Example |
|----------|-------|-------------|---------|
| `--verbose` | `-v` | Enable verbose output | `toolkit -v` |
| `--no-color` | `-n` | Disable colors | `toolkit -n` |
| `--auto-repair` | `-r` | Run repair, then exit | `toolkit -r` |
| `--no-export` | | Skip report export | `toolkit --no-export` |
| `--log-dir <path>` | `-l` | Custom logs folder | `toolkit -l ./logs` |
| `--report-dir <path>` | | Custom reports folder | `toolkit --report-dir ./reports` |
| `--version` | `-V` | Show version | `toolkit -V` |
| `--help` | `-h` | Show help | `toolkit -h` |

### Usage Examples

```bash
# Basic health check with auto-repair (requires admin)
SystemHealthToolkit --auto-repair

# With custom directories
SystemHealthToolkit --log-dir "D:\SHT\Logs" --report-dir "D:\SHT\Reports"

# Silent mode (no colors) for CI/automation
SystemHealthToolkit --no-color --auto-repair

# Just get version info
SystemHealthToolkit --version

# Verbose logging for debugging
SystemHealthToolkit --verbose
```

### Windows Run (Win+R) Shortcuts

1. Extract toolkit to `C:\Tools\SHT`
2. Press **Win+R**, type:
   ```
   C:\Tools\SHT\launcher.bat
   ```

**Pro tip:** Add `C:\Tools\SHT` to your PATH:
1. Right-click **This PC** → **Properties** → **Advanced system settings**
2. Click **Environment Variables**
3. Edit **Path** → **New** → Add `C:\Tools\SHT`
4. Now you can press **Win+R** and just type:
   ```
   launcher.bat
   ```

**Quick administration:**
```bash
# Create a desktop shortcut:
# 1. Right-click desktop → New → Shortcut
# 2. Location: C:\Tools\SHT\launcher.bat
# 3. Click Advanced → "Run as administrator"
# 4. Name: "System Health Toolkit"
```

---

## Full Scan Deep Dive

### What Happens During a Full Scan

When you select Option 1, the toolkit runs **10 scanners sequentially** with progress spinners:

```
Full System Scan

  ⠙  Analyzing storage...          ← enumerates drives, counts files, finds largest
  ⠹  Scanning hardware...          ← CPU, RAM, GPU, motherboard, BIOS, TPM
  ⠧  Diagnosing network...         ← adapters, ping, packet loss, public IP
  ⠋  Analyzing startup...          ← registry, folders, scheduled tasks
  ⠙  Reading event logs...         ← critical errors, warnings from System/Application logs
  ⠹  Measuring performance...      ← real-time CPU/RAM usage, top processes
  ⠧  Analyzing reliability...      ← blue screens, crashes, shutdowns
  ⠋  Gathering Windows info...     ← edition, activation, Defender, Hyper-V
  ⠙  Detecting corruption...       ← CBS log, DISM, pending reboots
  ⠹  Checking for updates...       ← Windows Update API via COM
```

### Health Score Categories & Scoring Logic

| Category | Scoring Factors |
|----------|----------------|
| **Windows Integrity** | Corruption issues (-10 each), repair failures (-5 each), not activated (-20) |
| **Storage** | Free space < 5% (-30), < 10% (-15), < 20% (-5) per drive |
| **Memory** | Usage > 90% (-30), > 80% (-15), > 70% (-5) |
| **Network** | No connected adapters (-50), packet loss > 5% (-20), high ping (-15) |
| **Security** | Defender off (-30), firewall off (-20), no secure boot (-10), no core isolation (-10) |
| **Drivers** | >5 driver updates pending (-15) |
| **Startup** | Broken entries (-10 each), unsigned (-5 each), >30 total (-10) |
| **Updates** | >10 available (-20), >5 (-10), failed updates (-10 each) |
| **Hardware** | CPU temp > 90°C (-20), battery < 20% (-15) |
| **Event Logs** | Critical events (-15 each), >10 errors (-20), >5 errors (-10) |
| **Performance** | CPU > 90% (-20), RAM > 90% (-15) |
| **Reliability** | Direct mapping from reliability index (0-10 → 0-100%) |

---

## Repair Pipeline Deep Dive

### The Complete Repair Sequence

| Step | Command | Purpose | Time | Reboot? |
|------|---------|---------|------|---------|
| 0 | Create Restore Point | Safety snapshot | 30s | No |
| 1 | DISM CheckHealth | Quick component health check | 10s | No |
| 2 | DISM ScanHealth | Deep component scan | 2-5m | No |
| 3 | DISM RestoreHealth | Repair corrupted components | 10-30m | No |
| 4 | SFC /scannow | Fix protected system files | 15-30m | Maybe |
| 5 | CHKDSK (read-only) | Check disk for errors | 1-5m | No |
| 6 | Flush DNS | Clear DNS resolver cache | 5s | No |
| 7 | Renew IP | Get fresh IP from DHCP | 30s | No |
| 8 | Winsock Reset | Fix network socket issues | 10s | Yes |
| 9 | Network Stack Reset | Reset TCP/IP and firewall | 30s | Yes |
| 10 | Clean Temp Folders | Remove system temp files | 30s | No |
| 11 | Clean Windows Temp | Remove Windows temp files | 30s | No |
| 12 | Clean User Temp | Remove user temp files | 30s | No |
| 13 | Empty Recycle Bin | Free up disk space | 10s | No |
| 14 | Clean Delivery Opt | Remove update cache files | 60s | No |
| 15 | Update Defender | Get latest virus definitions | 2-5m | No |
| 16 | Verify Services | Restart critical services | 30s | No |
| 17 | Verify Page File | Check virtual memory config | 10s | No |
| 18 | Verify Activation | Check Windows license | 15s | No |
| 19 | Component Cleanup | Shrink WinSxS store | 5-30m | No |

**Total time:** 30 minutes to 1.5 hours depending on system.

### When to Use Full Repair vs Advanced Tools

| Scenario | Recommended |
|----------|-------------|
| System running slow | Option 3 (Full Repair) |
| Blue screens/crashes | Option 3 (Full Repair) → then check logs |
| Can't install updates | Advanced → Reset WU Cache |
| Network problems | Advanced → Flush DNS & Reset Network |
| Corrupted files after crash | Options 3 or Advanced → DISM + SFC |
| Daily maintenance | Option 2 (Health Scan) |
| Before selling PC | Option 3 (Create restore point first) |

---

## Storage Analyzer Guide

### Understanding the Output

```
  ✓  C:\ (Windows) — 245.50 GB / 512.00 GB (48% used)
     Type: NVMe | FS: NTFS | SMART: OK | BitLocker: Off
```

- **48% used** — Color-coded: <75% green, 75-90% yellow, >90% red
- **NVMe** — Interface type (NVMe > SATA SSD > HDD for speed)
- **SMART: OK** — Drive health self-monitoring passed
- **BitLocker: Off** — Encryption status

### Reclaimable Space

The toolkit estimates how much space you can recover:

```
  Estimated reclaimable    12.34 GB
```

This includes:
- ✅ Temp files (Windows + User + System)
- ✅ Recycle Bin contents
- ✅ Windows Update cache (SoftwareDistribution)
- ✅ ~50% of duplicate file sizes

### Finding Large Files

The Top 10 Largest Files list helps you identify space hogs:

```
  1.  4.50 GB   C:\pagefile.sys           ← System file (don't delete)
  2.  3.20 GB   C:\hiberfil.sys            ← Can disable hibernation
  3.  2.10 GB   C:\Windows\Installer\...   ← Sometimes safe to clean
  4.  1.80 GB   C:\Program Files\...       ← Application
  5.  1.50 GB   C:\Users\User\Downloads\   ← User file (safe to delete)
```

**⚠️ Warning:** Don't delete system files like `pagefile.sys` or `hiberfil.sys` manually. Use the Cleanup tools in Advanced Options instead.

---

## Understanding Reports

### HTML Report

The HTML report is the most feature-rich format:

```
Reports/
  SystemHealth_20260701_104500.html   ← ← Open this in browser
  SystemHealth_20260701_104500.txt
  SystemHealth_20260701_104500.json
  SystemHealth_20260701_104500.csv
```

**HTML Report Features:**

1. **Health Score Ring** — Large circular gauge showing overall score with A+ grade
2. **Category Scores** — 12 health bars (color-coded) with percentage
3. **Storage Section** — Drive table + stat cards (Total Files, Folders, Largest, Reclaimable)
4. **Hardware Section** — Component details table
5. **Network Section** — Adapters table with status badges
6. **Startup Section** — Startup statistics cards
7. **Event Logs** — Critical/Error/Warning counts
8. **Performance** — CPU/RAM usage, processes, services
9. **Reliability** — Blue screens, crashes, reliability index
10. **Windows Info** — Edition, version, activation, features table
11. **Corruption Detection** — Issue list with error cards
12. **Repair Results** — Pass/Fail counts with detailed table
13. **Warnings Panel** — All detected warnings
14. **Recommendations Panel** — Actionable suggestions

**How to use:**
- Click any section header (▶) to collapse/expand
- Sections are collapsed by default for easy navigation
- The page is responsive — works on phones and tablets
- Dark theme is easy on the eyes

### TXT Report

Plain-text format suitable for:
- Printing
- Sharing in emails
- Archiving
- Importing into document editors

### JSON Report

Machine-readable format for:
- Automation scripts
- Importing into databases
- Custom dashboards
- Programmatic analysis

Structure:
```json
{
  "report": {
    "tool": "System Health Toolkit",
    "version": "2.1.0",
    "generated": "2026-07-01 10:45:00",
    "platform": "Windows 11 Pro"
  },
  "health_score": {
    "overall": 97,
    "grade": "A+",
    "categories": [
      { "name": "Storage", "score": 87 },
      ...
    ]
  },
  "storage": { ... },
  "hardware": { ... },
  "repair": { ... }
}
```

### CSV Report

Spreadsheet-compatible format for:
- Excel/Google Sheets
- Data analysis
- Trend tracking over time

---

## Health Score System

### Grade Scale

| Score Range | Grade | Meaning |
|-------------|-------|---------|
| 97-100% | A+ | Excellent — system is in top condition |
| 93-96% | A | Very good — minor issues |
| 90-92% | A- | Good — some attention needed |
| 87-89% | B+ | Fair — several areas need review |
| 83-86% | B | Below average — take action |
| 80-82% | B- | Concerning — investigate issues |
| 77-79% | C+ | Poor — repairs recommended |
| 73-76% | C | Bad — immediate action needed |
| 70-72% | C- | Critical — major problems |
| 60-69% | D | Very critical — system unstable |
| <60% | F | Failing — professional help advised |

### Improving Your Score

| Issue | How to Fix |
|-------|-----------|
| Low storage score | Run disk cleanup, uninstall unused apps, move files |
| Memory score low | Close unnecessary programs, add more RAM |
| Network score low | Check cables, restart router, update drivers |
| Security score low | Enable Defender, turn on firewall, enable Secure Boot |
| Startup score high | Disable unnecessary startup programs in Task Manager |
| Low updates score | Run Windows Update (Option 8) |
| Corruption detected | Run Option 3 (Repair Windows) |
| Many event log errors | Check specific events, run SFC/DISM |
| Performance issues | Close background apps, check for malware |
| Low reliability | Check for failing hardware, update drivers |

---

## Log Files Explained

### Log Location

```
SystemHealthToolkit/
  Logs/
    System.log       ← Main operational log
    Repair.log       ← All repair actions
    Errors.log       ← All warnings + errors
    Scan.log         ← All scan results
```

### Log Entry Format

```
[2026-07-01 10:45:00.123] [INFO ] [storage     ] Storage analysis started
[2026-07-01 10:45:30.456] [INFO ] [storage     ] Storage analysis complete  (30.3s)
[2026-07-01 10:45:31.000] [WARN ] [storage     ] Low disk space on C:\ (8% free)
[2026-07-01 10:45:32.000] [ERROR] [repair      ] DISM RestoreHealth failed  (120.5s)
  >> Exit code 2 - Component store corruption detected
```

**Fields:**
- `[timestamp]` — Precise time with milliseconds
- `[LEVEL]` — DEBUG, INFO, WARN, ERROR, FATAL
- `[category]` — Which module generated the entry
- Message — Description of what happened
- `(duration)` — How long the operation took
- `>> detail` — Additional error context on failures

### Log Levels

| Level | Meaning | When It Appears |
|-------|---------|-----------------|
| DEBUG | Detailed diagnostic info | Verbose mode only |
| INFO | Normal operations | Standard operations |
| WARN | Potential issue | Low disk space, minor problems |
| ERROR | Operation failed | Repair failures, scan errors |
| FATAL | Critical failure | Unexpected crashes, serious errors |

### Reading Errors.log for Troubleshooting

When something goes wrong, check `Errors.log`:

```
[2026-07-01 10:46:00.000] [WARN ] [storage     ] Low disk space on C:\ (3% free)
[2026-07-01 10:47:00.000] [ERROR] [repair      ] SFC /scannow failed  (900.0s)
  >> Windows Resource Protection could not perform the requested operation
[2026-07-01 10:48:00.000] [ERROR] [network     ] Public IP lookup failed
  >> Unable to connect to remote server
```

Each error gives you the category and specific details to help diagnose.

---

## Advanced Tools Reference

### DISM Commands

| Tool | What It Does | Duration |
|------|-------------|----------|
| **CheckHealth** | Quick check if component store is healthy | ~10 seconds |
| **ScanHealth** | Deep scan for component corruption | 2-10 minutes |
| **RestoreHealth** | Repair corrupted components (needs internet) | 10-30 minutes |

**When to use:**
- After Windows Update failures
- System file corruption suspected
- Before/after feature updates

### SFC (System File Checker)

Scans all protected system files and replaces corrupted versions.

**Duration:** 15-30 minutes
**After SFC:** May require reboot

**Common results:**
- "Windows Resource Protection did not find any integrity violations" ✅
- "Windows Resource Protection found corrupt files and successfully repaired them" ✅
- "Windows Resource Protection found corrupt files but was unable to fix some of them" → Run DISM first, then SFC again

### CHKDSK

| Mode | What It Does | Duration |
|------|-------------|----------|
| **Scan (read-only)** | Reports file system errors without fixing | 1-5 minutes |
| **Schedule Repair** | Fixes errors on next reboot (with /f /r) | Reboot required |

**Scheduling CHKDSK:**
```
  ?  Schedule CHKDSK /f /r on next reboot? [y/N]:
```

Selecting yes means:
- On next restart, CHKDSK will run during boot (before Windows loads)
- It will fix file system errors and recover bad sectors
- This can take 30 minutes to several hours

### Network Tools

| Tool | What It Fixes |
|------|---------------|
| **Flush DNS** | Clears DNS resolver cache — fixes "website not found" errors |
| **Renew IP** | Gets new IP from DHCP — fixes IP conflicts |
| **Winsock Reset** | Fixes socket/network API corruption |
| **Network Stack Reset** | Resets TCP/IP, IPv6, firewall — fixes network connectivity |

**When to use:**
- "No internet access" but Wi-Fi is connected
- DNS errors (website not resolving)
- After installing/uninstalling VPN software
- IP address conflicts

### Cleanup Tools

| Tool | Space Freed | Safe? |
|------|-------------|-------|
| **System Temp** | 100MB–2GB | ✅ Always safe |
| **User Temp** | 100MB–5GB | ✅ Always safe |
| **Temp Folders** | 50MB–1GB | ✅ Always safe |
| **Recycle Bin** | Varies | ✅ Always safe |
| **Browser Cache** | 100MB–10GB | ⚠️ Will clear saved passwords/forms |
| **WU Cache** | 2–10GB | ⚠️ May cause next update to re-download |

### Service Management

**Verifies these critical services:**
- Windows Installer (msiserver)
- Background Intelligent Transfer (BITS)
- Windows Update (wuauserv)
- Cryptographic Services (cryptSvc)
- Event Log (EventLog)
- Task Scheduler (Schedule)
- DNS Client (Dnscache)

---

## Benchmarking Guide

### What Each Benchmark Tests

#### Disk Benchmark

```
Sequential Read    3400.50 MB/s    ← Large file read speed (important for games/video)
Sequential Write   2800.20 MB/s    ← Large file write speed (important for file transfers)
Random Read IOPS   850,000         ← Small file read speed (important for OS boot)
Random Write IOPS  620,000         ← Small file write speed (important for app installs)
Average Latency    0.05 ms         ← Access time (lower is better)
```

**Interpretation:**
- **NVMe SSD:** Seq Read 3000-7000 MB/s, IOPS 500K-1M+
- **SATA SSD:** Seq Read 500-550 MB/s, IOPS 80K-100K
- **HDD:** Seq Read 100-200 MB/s, IOPS 100-200

#### Network Benchmark

```
Download Speed     850.30 Mbps     ← How fast you can download
Latency            12 ms           ← Ping time (lower = more responsive)
Jitter             1.5 ms          ← Latency variation (lower = more stable)
Packet Loss        0%              ← Lost packets (should be 0%)
```

**Interpretation:**
- **Fiber:** 500-1000 Mbps, latency <20ms
- **Cable:** 100-500 Mbps, latency 10-30ms
- **DSL:** 10-50 Mbps, latency 20-60ms
- **5G:** 50-500 Mbps, latency 20-50ms

#### CPU Benchmark

Tests computational throughput by counting primes up to 500,000 across all cores.

**Score:** Higher is better. A modern i7 scores ~15,000-25,000 primes/s.

#### Memory Benchmark

Measures sequential copy bandwidth (256MB buffer).

**Score:** Higher is better.
- DDR4-3200: ~20,000-30,000 MB/s
- DDR5-5600: ~40,000-55,000 MB/s
- DDR5-6400: ~50,000-65,000 MB/s

### Interpreting Overall Score

| Score | Meaning |
|-------|---------|
| 90-100 | Excellent — high-performance system |
| 75-89 | Good — solid performance |
| 50-74 | Average — meets basic needs |
| 25-49 | Below average — consider upgrades |
| 0-24 | Poor — system may feel slow |

---

## Troubleshooting

### Common Issues & Solutions

#### "Access Denied" / "Requires Administrator"

```
  ⚠  Repair operations require administrator privileges.
  ?  Attempt to re-launch as administrator? [Y/n]:
```

**Solution:**
1. Right-click `launcher.bat`
2. Select **Run as administrator**
3. Or run from elevated Command Prompt:
   ```cmd
   cd C:\Path\To\Toolkit
   SystemHealthToolkit.exe
   ```

#### Toolkit Won't Start

**Symptoms:**
- `launcher.bat` opens and closes immediately
- "The code execution cannot proceed because MSVCP140.dll was not found"

**Solutions:**
1. Install [Microsoft Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)
2. Rebuild from source with:
   ```cmd
   g++ -std=c++20 -O2 -pthread -I include src/*.cpp -o SystemHealthToolkit.exe -static
   ```
3. Or use the `-static` flag in CMake:
   ```cmd
   cmake .. -DSHT_BUILD_STATIC=ON
   ```

#### Scan is Taking Too Long

**Storage scan** may take a while on large drives with many files:

- **Expected:** ~100,000 files/minute with parallel scanning
- **If slow:** Try excluding specific folders via the next update

**DISM RestoreHealth** can take 30+ minutes:
- This is normal — it's repairing system files
- Ensure you have a stable internet connection
- Don't interrupt the process

#### Network Tests Fail

```
  ✗  Public IP lookup failed
  >> Unable to connect to remote server
```

**Solutions:**
1. Check your internet connection
2. The toolkit uses `api.ipify.org` — ensure it's not blocked
3. If behind a corporate firewall, API calls may be restricted

#### DISM / SFC Fails

```
  ✗  DISM RestoreHealth failed
  >> Exit code 2 - Component store corruption detected
```

**Solutions:**
1. Run with internet connection (DISM needs Windows Update to repair)
2. Try running with a Windows installation media:
   ```cmd
   DISM /Online /Cleanup-Image /RestoreHealth /Source:C:\RepairSource\Windows /LimitAccess
   ```
3. If still failing, consider Windows repair install or reset

#### "No restore points found"

This is not an error — it just means no system restore points exist.

**To enable restore points:**
1. Press Win+R, type `SystemPropertiesProtection`
2. Select your system drive
3. Click **Configure**
4. Select **Turn on system protection**
5. Adjust max usage slider

### Log File Analysis

When encountering issues, always check `Logs/Errors.log`:

```bash
# Windows
type Logs\Errors.log

# Linux/macOS
cat Logs/Errors.log
```

Look for:
- `[ERROR]` entries with the module name
- The `>>` detail line explaining the failure
- `[WARN]` entries that may indicate related problems

---

## FAQ

### General

**Q: Does this tool modify my registry?**
A: No. The Registry Scanner is **detection-only**. It reports issues but does not modify the registry.

**Q: Will this work on Windows 10?**
A: Yes. Windows 10 (build 1903+) and Windows 11 are fully supported.

**Q: Does it work on Linux/macOS with the same features?**
A: Most features work. Windows-specific tools (DISM, SFC, CHKDSK, Registry, Windows Update) are unavailable on non-Windows platforms.

**Q: Is it safe to run?**
A: Yes. Read operations are always safe. Repair operations create a restore point before making changes. Cleanup operations only affect temporary files.

**Q: Can I automate this?**
A: Yes. Use `--auto-repair` for non-interactive repair. Use `--no-color` for clean output in scripts.

### Performance

**Q: Why is it faster than PowerShell alternatives?**
A: This is compiled C++20 code running natively, ~10x faster than PowerShell/Python equivalents. The new parallel scanner uses all CPU cores.

**Q: How much memory does it use?**
A: Typically 10-50MB during normal operation. Up to 500MB during storage benchmark (256MB test buffer).

**Q: Will the benchmark wear out my SSD?**
A: No. The disk benchmark writes ~512MB and reads ~512MB. This is negligible for modern SSDs (typical endurance: 300-600 TBW).

### Troubleshooting

**Q: The toolkit says "not elevated" but I'm an admin?**
A: Being an admin user ≠ running as admin. You must explicitly choose "Run as administrator."

**Q: Can I cancel a running scan?**
A: Press Ctrl+C in the terminal. Logs up to that point are preserved.

**Q: How do I update the toolkit?**
A: Pull the latest source and rebuild:
```bash
git pull origin main
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target SystemHealthToolkit
```

---

## Tips & Best Practices

### Regular Maintenance Schedule

| Frequency | Action |
|-----------|--------|
| **Daily** | Option 2 (Health Scan) — quick check |
| **Weekly** | Option 1 (Full Scan) + Option 8 (Updates) |
| **Monthly** | Option 3 (Repair Windows) |
| **Quarterly** | Option 11 (System Benchmark) for performance baselines |

### Before Major Changes

Always run a Full Scan + Export Report before:
- Installing Windows feature updates
- Upgrading hardware
- Selling/giving away the computer
- Major software installations

### After System Issues

1. Run **Option 3 (Repair Windows)** with restore point
2. Check `Logs/Errors.log` for specifics
3. If repairs fail, use **Advanced Tools** for targeted operations
4. Run **Option 1 (Full Scan)** to verify fixes
5. Export report for reference

### For IT Professionals

- Use `SystemHealthToolkit --auto-repair --no-color >> repair_log.txt` for batch deployment
- Parse JSON reports with `jq` for monitoring dashboards:
  ```bash
  cat Reports/*.json | jq '.health_score.overall'
  ```
- Set up scheduled tasks to run weekly full scans:
  ```powershell
  # Windows Task Scheduler
  $action = New-ScheduledTaskAction -Execute "SystemHealthToolkit.exe" -Argument "--auto-repair --no-export"
  $trigger = New-ScheduledTaskTrigger -Weekly -DaysOfWeek Sunday -At 3am
  Register-ScheduledTask -TaskName "SHT Weekly Repair" -Action $action -Trigger $trigger -RunLevel Highest
  ```

### Disk Space Recovery Tips

1. Run **Advanced → Clean Temp Files** (cleans 1-10GB)
2. Run **Empty Recycle Bin** (cleans 0.1-50GB)
3. Use **Storage Analyzer** to find large files
4. Delete old Windows Update cache (Advanced → Reset WU Cache)
5. Run Windows Disk Cleanup as well:
   ```cmd
   cleanmgr /sageset:1   # Configure settings
   cleanmgr /sagerun:1   # Run cleanup
   ```

### Building for Maximum Performance

```bash
# Windows with MSVC (fastest binary)
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# With LTO (Link-Time Optimization)
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

# Static binary (no DLL dependencies)
cmake .. -DSHT_BUILD_STATIC=ON -DCMAKE_BUILD_TYPE=Release
```

---

<div align="center">
  <sub>© 2026 System Health Toolkit Team | Version 2.1.0</sub>
  <br/>
  <sub>For issues or contributions, visit our GitHub repository</sub>
</div>
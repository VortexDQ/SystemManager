#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
#  System Health Toolkit — Unix Launcher (Linux / macOS)
#  Usage:
#    chmod +x run.sh
#    ./run.sh [--auto-repair] [--no-color] [--no-export]
#    ./run.sh --help
# ═══════════════════════════════════════════════════════════════════

set -euo pipefail

SHT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHT_EXE=""
BINARY_NAME="SystemHealthToolkit"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

# ── Find the binary ─────────────────────────────
search_paths=(
    "${SHT_DIR}/bin/${BINARY_NAME}"
    "${SHT_DIR}/${BINARY_NAME}"
    "${SHT_DIR}/build/Release/${BINARY_NAME}"
    "${SHT_DIR}/build/Debug/${BINARY_NAME}"
)

for p in "${search_paths[@]}"; do
    if [[ -x "$p" ]]; then
        SHT_EXE="$p"
        break
    fi
done

# Try PATH
if [[ -z "$SHT_EXE" ]]; then
    SHT_EXE="$(command -v "$BINARY_NAME" 2>/dev/null || true)"
fi

# ── Build if not found ─────────────────────────
if [[ -z "$SHT_EXE" ]]; then
    echo -e "${YELLOW}[SHT] Executable not found. Building from source...${NC}"
    echo ""

    # Check for cmake
    if ! command -v cmake &>/dev/null; then
        echo -e "${RED}[SHT] ERROR: CMake is not installed.${NC}"
        echo -e "${YELLOW}Install CMake: brew install cmake (macOS) or apt install cmake (Linux)${NC}"
        exit 1
    fi

    # Check for compiler
    compiler=""
    if command -v g++ &>/dev/null; then
        compiler="g++"
    elif command -v clang++ &>/dev/null; then
        compiler="clang++"
    else
        echo -e "${RED}[SHT] ERROR: No C++ compiler found. Install GCC or Clang.${NC}"
        exit 1
    fi

    echo -e "${GREEN}[SHT] Building with ${compiler}...${NC}"
    BUILD_DIR="${SHT_DIR}/build"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    if [[ "$(uname)" == "Darwin" ]]; then
        cmake .. -DCMAKE_BUILD_TYPE=Release \
                 -DCMAKE_CXX_COMPILER="${compiler}"
    else
        cmake .. -DCMAKE_BUILD_TYPE=Release \
                 -DCMAKE_CXX_COMPILER="${compiler}"
    fi

    if cmake --build . --target "${BINARY_NAME}" -- -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"; then
        SHT_EXE="${BUILD_DIR}/${BINARY_NAME}"
    else
        echo -e "${YELLOW}[SHT] CMake build failed. Trying direct compilation...${NC}"
        cd "$SHT_DIR"
        ${compiler} -std=c++20 -O2 -pthread -I include src/*.cpp -o "${BINARY_NAME}"
        SHT_EXE="${SHT_DIR}/${BINARY_NAME}"
    fi
    cd "$SHT_DIR"
fi

if [[ ! -x "$SHT_EXE" ]]; then
    echo -e "${RED}[SHT] ERROR: Could not find or build the executable.${NC}"
    exit 1
fi

# ── Create directory structure ─────────────────
for d in Logs Reports Temp Config Assets; do
    mkdir -p "${SHT_DIR}/${d}"
done

# ── Check elevation ────────────────────────────
if [[ $EUID -eq 0 ]]; then
    ELEVATED="Yes"
else
    ELEVATED="No"
fi

# ── Banner ─────────────────────────────────────
echo ""
echo -e "${CYAN}  ╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}  ║          SYSTEM HEALTH TOOLKIT  v2.0.0                      ║${NC}"
echo -e "${CYAN}  ║          Unix Launcher                                      ║${NC}"
echo -e "${CYAN}  ╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GRAY}  Launcher:   $(basename "${BASH_SOURCE[0]}")${NC}"
echo -e "${GRAY}  Executable: ${SHT_EXE}${NC}"
echo -e "${GRAY}  Elevated:   ${ELEVATED}${NC}"
echo -e "${GRAY}  Platform:   $(uname -srm)${NC}"
echo ""

# ── Launch ─────────────────────────────────────
echo -e "${GREEN}[SHT] Starting System Health Toolkit...${NC}"
echo ""

"${SHT_EXE}" "$@"
EXIT_CODE=$?

if [[ $EXIT_CODE -ne 0 ]]; then
    echo ""
    echo -e "${RED}[SHT] The toolkit exited with code ${EXIT_CODE}.${NC}"
    echo -e "${YELLOW}Troubleshooting:${NC}"
    echo -e "${YELLOW}  - Run with sudo for full functionality: sudo ./run.sh${NC}"
    echo -e "${YELLOW}  - Check Logs/Errors.log for details${NC}"
fi

exit $EXIT_CODE
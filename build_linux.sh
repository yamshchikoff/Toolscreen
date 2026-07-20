#!/bin/bash
# Toolscreen Linux build script
# Builds libtoolscreen.so for use with Minecraft Java Edition on Linux
#
# Usage: ./build_linux.sh [--debug] [--clean]
#   --debug   Build with debug symbols
#   --clean   Clean build directory before building

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/out/build/linux"
INSTALL_DIR="${SCRIPT_DIR}/out/install/linux"
BUILD_TYPE="Release"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            rm -rf "$BUILD_DIR"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--debug] [--clean]"
            exit 1
            ;;
    esac
done

echo "===== Toolscreen Linux Build ====="
echo "Build type: $BUILD_TYPE"
echo "Build dir:  $BUILD_DIR"
echo ""

# Check dependencies
missing_deps=()
for pkg in build-essential cmake libx11-dev libxext-dev libxfixes-dev libxi-dev \
           libgl1-mesa-dev libglew-dev libglfw3-dev libxrandr-dev libxinerama-dev \
           libxcursor-dev libxxf86vm-dev zlib1g-dev; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        missing_deps+=("$pkg")
    fi
done

if [[ ${#missing_deps[@]} -gt 0 ]]; then
    echo ">> Missing dependencies: ${missing_deps[*]}"
    echo ">> Install with: sudo apt install -y ${missing_deps[*]}"
    echo ">> Attempting to continue anyway..."
    echo ""
fi

# Configure
mkdir -p "$BUILD_DIR"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    "$@"

# Build
echo ""
echo ">> Building..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

# Install
mkdir -p "$INSTALL_DIR"
if [[ -f "$BUILD_DIR/bin/libtoolscreen.so" ]]; then
    cp "$BUILD_DIR/bin/libtoolscreen.so" "$INSTALL_DIR/"
    echo ""
    echo "===== Build successful ====="
    echo "Output: $INSTALL_DIR/libtoolscreen.so"
    echo ""
    echo "To use with Minecraft:"
    echo "  1. Add to your launcher's JVM arguments or wrapper script:"
    echo "     export LD_PRELOAD=\"$INSTALL_DIR/libtoolscreen.so\""
    echo ""
    echo "  2. Or in Prism Launcher:"
    echo "     Settings > Java > JVM Arguments, add:"
    echo "     -Djava.library.path=$INSTALL_DIR"
    echo "     And in the instance's pre-launch script:"
    echo "     export LD_PRELOAD=\"$INSTALL_DIR/libtoolscreen.so\""
else
    echo ""
    echo "===== Build FAILED ====="
    echo "Check the errors above."
    exit 1
fi

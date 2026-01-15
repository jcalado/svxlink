#!/bin/bash
# Build script for qtel-gtk4

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Parse arguments
CLEAN=0
RECONFIGURE=0
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean|-c)
            CLEAN=1
            shift
            ;;
        --reconfigure|-r)
            RECONFIGURE=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  -c, --clean        Remove build directory and rebuild"
            echo "  -r, --reconfigure  Reconfigure meson before building"
            echo "  -h, --help         Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

cd "${SCRIPT_DIR}"

# Clean if requested
if [[ $CLEAN -eq 1 ]]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

# Configure if needed
if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Configuring meson..."
    meson setup "${BUILD_DIR}"
elif [[ $RECONFIGURE -eq 1 ]]; then
    echo "Reconfiguring meson..."
    meson setup --reconfigure "${BUILD_DIR}"
fi

# Build
echo "Building..."
ninja -C "${BUILD_DIR}"

echo ""
echo "Build complete. Run with:"
echo "  ./qtel.sh"

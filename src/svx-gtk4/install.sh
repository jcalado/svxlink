#!/bin/bash
# Install script for Echo (qtel-gtk4)
# Installs to ~/.local for per-user installation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/builddir"
SVXLINK_BUILD_DIR="$SCRIPT_DIR/../build"
PREFIX="$HOME/.local"

echo "=== Installing Echo (qtel-gtk4) ==="
echo "Prefix: $PREFIX"
echo ""

# Check for svxlink build directory
if [ ! -d "$SVXLINK_BUILD_DIR/lib" ]; then
    echo "ERROR: svxlink build directory not found at $SVXLINK_BUILD_DIR"
    echo "Please build svxlink first with:"
    echo "  cd $SCRIPT_DIR/.."
    echo "  mkdir -p build && cd build"
    echo "  cmake .."
    echo "  make"
    exit 1
fi

# Configure
echo "Configuring..."
if [ -d "$BUILD_DIR" ]; then
    meson setup --reconfigure --prefix="$PREFIX" "$BUILD_DIR" "$SCRIPT_DIR"
else
    meson setup --prefix="$PREFIX" "$BUILD_DIR" "$SCRIPT_DIR"
fi

# Build
echo ""
echo "Building..."
meson compile -C "$BUILD_DIR"

# Install
echo ""
echo "Installing..."
meson install -C "$BUILD_DIR"

# Post-install tasks
echo ""
echo "Running post-install tasks..."

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    echo "  Updating icon cache..."
    gtk-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true
fi

# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    echo "  Updating desktop database..."
    update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
fi

# Compile GSettings schemas
if command -v glib-compile-schemas &> /dev/null; then
    echo "  Compiling GSettings schemas..."
    glib-compile-schemas "$PREFIX/share/glib-2.0/schemas" 2>/dev/null || true
fi

# Install svxlink shared libraries
echo "  Installing svxlink libraries..."
mkdir -p "$PREFIX/lib"

# List of required libraries from svxlink build
REQUIRED_LIBS=(
    "libasyncglib"
    "libasynccore"
    "libasyncaudio"
    "libasynccpp"
    "libecholib"
)

for lib in "${REQUIRED_LIBS[@]}"; do
    # Find all versions of this library (including symlinks)
    for f in "$SVXLINK_BUILD_DIR/lib/${lib}.so"*; do
        if [ -e "$f" ]; then
            if [ -L "$f" ]; then
                # Copy symlink
                cp -P "$f" "$PREFIX/lib/"
            else
                # Copy actual file
                cp "$f" "$PREFIX/lib/"
            fi
        fi
    done
done

echo ""
echo "=== Installation complete ==="
echo ""
echo "Add the following to your shell profile (~/.bashrc or ~/.zshrc):"
echo '  export PATH="$HOME/.local/bin:$PATH"'
echo '  export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"'
echo ""
echo "Then reload your shell or run:"
echo '  source ~/.bashrc  # or ~/.zshrc'
echo ""
echo "You can now launch Echo from your application menu or run:"
echo "  qtel-gtk4"

#!/bin/bash
# Uninstall script for Echo (qtel-gtk4)
# Removes files installed to ~/.local

set -e

PREFIX="$HOME/.local"

echo "=== Uninstalling Echo (qtel-gtk4) ==="
echo "Prefix: $PREFIX"
echo ""

# Files to remove
FILES=(
    "$PREFIX/bin/qtel-gtk4"
    "$PREFIX/share/applications/org.svxlink.Qtel.desktop"
    "$PREFIX/share/metainfo/org.svxlink.Qtel.metainfo.xml"
    "$PREFIX/share/glib-2.0/schemas/org.svxlink.Qtel.gschema.xml"
    "$PREFIX/share/icons/hicolor/scalable/apps/org.svxlink.Qtel.svg"
    "$PREFIX/share/icons/hicolor/symbolic/apps/org.svxlink.Qtel-symbolic.svg"
)

echo "Removing files..."
for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  Removing $file"
        rm -f "$file"
    fi
done

# Post-uninstall tasks
echo ""
echo "Running post-uninstall tasks..."

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

# Recompile GSettings schemas
if command -v glib-compile-schemas &> /dev/null; then
    echo "  Recompiling GSettings schemas..."
    glib-compile-schemas "$PREFIX/share/glib-2.0/schemas" 2>/dev/null || true
fi

echo ""
echo "=== Uninstall complete ==="

#!/bin/bash
# Build and run qtel-gtk4

cd "$(dirname "$0")/builddir" || exit 1

echo "Building qtel-gtk4..."
if ! ninja; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "Starting qtel-gtk4..."
GSETTINGS_SCHEMA_DIR=. \
LD_LIBRARY_PATH=/home/joel/code/svxlink/src/build/lib:$LD_LIBRARY_PATH \
./qtel-gtk4 "$@"

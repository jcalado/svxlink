#!/bin/bash
# Launch qtel-gtk4 with correct library and schema paths

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export LD_LIBRARY_PATH="${SCRIPT_DIR}/../build/lib:${LD_LIBRARY_PATH}"
export GSETTINGS_SCHEMA_DIR="${SCRIPT_DIR}/data"

exec "${SCRIPT_DIR}/build/qtel-gtk4" "$@"

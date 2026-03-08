#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build-ci"

echo "=== Configure ==="
cmake --preset ci

echo "=== Build ==="
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo "=== Test ==="
xvfb-run -a ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure \
    --parallel "$(nproc)"

echo "=== CI passed ==="

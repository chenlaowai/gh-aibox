#!/bin/bash
# End-to-end step-1/step-2 verification:
#   1. compile the official demo against the locally built C++ SDK
#   2. run it on the reflective-jacket test images
#   3. assert that detections were produced
#
# Run ON THE BOX:
#   bash tests/yolov11_official_demo/verify.sh
set -euo pipefail

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== [1/3] build official demo ==="
bash "$TEST_DIR/build.sh"

echo "=== [2/3] run official demo ==="
bash "$TEST_DIR/run.sh"

echo "=== [3/3] check results ==="
python3 "$TEST_DIR/check_results.py"

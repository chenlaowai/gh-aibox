#!/bin/bash
# Run the official YOLOv11 C++ demo on the reflective-jacket bmodel.
#
# Run ON THE BOX:
#   bash tests/yolov11_official_demo/run.sh [image_dir_or_video]
set -euo pipefail

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$TEST_DIR/../.." && pwd)"
# shellcheck source=../../env/env.sh
source "$ROOT/env/env.sh"

BIN="$TEST_DIR/bin/yolov11s_reflectivejacket_demo"
# The deployed INT8 bmodel currently returns all-zero class scores on this box
# (see docs/KNOWN-ISSUES.md KI-001); use its FP16 counterpart for the SDK smoke
# test.  Prefer the test-local copy, then the original text_1 project model.
MODEL="${MODEL:-}"
if [ -z "$MODEL" ]; then
    for candidate in \
        "$TEST_DIR/models/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel" \
        "/data/CT/project/ai/models/BM1684X/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel" \
        "$TEST_DIR/models/yolov11s_fp16_1b_back.bmodel" \
        "/data/CT/project/ai/text_1/projects/text_1/yolo11/models/yolov11s_fp16_1b_back.bmodel" \
        "/data/CT/project/ai/models/BM1684X/yolov11s_fp16_1b_back.bmodel"; do
        if [ -f "$candidate" ]; then
            MODEL="$candidate"
            break
        fi
    done
fi
NAMES="$TEST_DIR/data/reflectivejacket_en.names"
INPUT="${1:-$TEST_DIR/data/images}"

for f in "$BIN" "$MODEL" "$NAMES" "$INPUT"; do
    if [ ! -e "$f" ]; then
        echo "ERROR: missing $f" >&2
        exit 1
    fi
done

cd "$TEST_DIR"
rm -rf results
mkdir -p results

echo "=== running demo ==="
"$BIN" \
    --bmodel="$MODEL" \
    --classnames="$NAMES" \
    --input="$INPUT" \
    --conf_thresh=0.25 \
    --nms_thresh=0.7 \
    --dev_id=0

echo
echo "=== output ==="
find results -type f -printf '%p %s bytes\n' | sort

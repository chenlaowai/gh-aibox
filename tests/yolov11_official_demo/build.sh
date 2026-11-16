#!/bin/bash
# Compile the official Sophon YOLOv8/YOLOv11 C++ demo against the project SDK.
#
# Run ON THE BOX:
#   bash tests/yolov11_official_demo/build.sh
set -euo pipefail

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$TEST_DIR/../.." && pwd)"
# shellcheck source=../../env/env.sh
source "$ROOT/env/env.sh"

SRC="$TEST_DIR/cpp/yolov8_bmcv"
OUT="$TEST_DIR/bin"
mkdir -p "$OUT"

if [ ! -f "$SAIL_HOME/lib/libsail.so" ]; then
    echo "ERROR: libsail.so missing; run env/setup_sdk.sh first" >&2
    exit 1
fi

echo "=== compiling official YOLOv8_plus_det demo ==="
g++ -std=c++11 -O2 \
    -I"$LIBSOPHON_HOME/include" \
    -I"$FFMPEG_HOME/include" \
    -I"$OPENCV_HOME/include/opencv4" \
    "$SRC/main.cpp" "$SRC/yolov8_det.cpp" \
    -L"$LIBSOPHON_HOME/lib" -L"$OPENCV_HOME/lib" -L"$FFMPEG_HOME/lib" -L"$SAIL_HOME/lib" \
    -Wl,-rpath,"$LIBSOPHON_HOME/lib:$OPENCV_HOME/lib:$FFMPEG_HOME/lib:$SAIL_HOME/lib" \
    -lbmcv -lbmrt -lbmlib -lyuv -lbmjpuapi -lbmjpulite \
    -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs -lopencv_videoio \
    -lavcodec -lavformat -lavutil -lswscale \
    -lpthread -ldl \
    -o "$OUT/yolov11s_reflectivejacket_demo"

echo "OK: $OUT/yolov11s_reflectivejacket_demo"

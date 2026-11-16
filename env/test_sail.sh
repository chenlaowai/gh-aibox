#!/bin/bash
# Build and run the C++ sophon-sail smoke test.
# Run ON THE BOX:
#   bash env/test_sail.sh /path/to/model.bmodel
set -euo pipefail

ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$ENV_DIR/env.sh"

if [ $# -gt 0 ]; then
    MODEL="$1"
else
    MODEL=""
    for candidate in \
        "/data/CT/project/ai/models/BM1684X/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel" \
        "/data/CT/project/ai/gh-aibox-dev/tests/yolov11_official_demo/models/yolov11s_fp16_1b_back.bmodel" \
        "/data/CT/project/ai/text_1/projects/text_1/yolo11/models/yolov11s_fp16_1b_back.bmodel" \
        "/data/CT/project/ai/models/BM1684X/yolov11s_int8_1b_back_reflectivejacket_1.bmodel"; do
        if [ -f "$candidate" ]; then
            MODEL="$candidate"
            break
        fi
    done
fi
BIN="$ENV_DIR/bin/test_sail"
mkdir -p "$ENV_DIR/bin"

g++ -std=c++11 -O2 \
    -I"$SAIL_HOME/include" -I"$SAIL_HOME/include/sail" \
    -I"$LIBSOPHON_HOME/include" -I"$FFMPEG_HOME/include" -I"$OPENCV_HOME/include/opencv4" \
    "$ENV_DIR/test_sail.cpp" \
    -L"$SAIL_HOME/lib" -L"$LIBSOPHON_HOME/lib" -L"$FFMPEG_HOME/lib" -L"$OPENCV_HOME/lib" \
    -Wl,-rpath,"$SAIL_HOME/lib:$LIBSOPHON_HOME/lib:$FFMPEG_HOME/lib:$OPENCV_HOME/lib" \
    -lsail -lbmcv -lbmrt -lbmlib -lyuv -lbmjpuapi -lbmjpulite \
    -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs -lopencv_videoio \
    -lavcodec -lavformat -lavutil -lswscale \
    -lpthread -ldl \
    -o "$BIN"

echo "=== running sail smoke test ==="
"$BIN" "$MODEL"

#!/bin/bash
# Build the C++ Sophon SDK (libsophon + sophon-mw + sophon-sail) on the AI box.
#
# Run ON THE BOX:
#   cd /data/CT/project/ai/gh-aibox-dev
#   bash env/setup_sdk.sh
#
# Prerequisite: the vendor packages are in env/packages/:
#   libsophon_soc_0.5.3_aarch64.tar.gz
#   sophon-mw-soc_0.15.0_aarch64.tar.gz
#   sophon-sail_3.11.0.tar.gz
#
# The install is self-contained under env/sdk; /opt/sophon is left untouched.
set -euo pipefail

ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="$ENV_DIR/sdk"
PKG_DIR="$ENV_DIR/packages"
SRC_DIR="$ENV_DIR/src"
BUILD_DIR="$ENV_DIR/build/sail"

LIBSOPHON="$SDK_DIR/libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3"
FFMPEG="$SDK_DIR/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-ffmpeg_0.15.0"
OPENCV="$SDK_DIR/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-opencv_0.15.0"
SAIL_PREFIX="$SDK_DIR"

for p in libsophon_soc_0.5.3_aarch64.tar.gz sophon-mw-soc_0.15.0_aarch64.tar.gz sophon-sail_3.11.0.tar.gz; do
    if [ ! -f "$PKG_DIR/$p" ]; then
        echo "ERROR: missing $PKG_DIR/$p (copy it from reference/release)" >&2
        exit 1
    fi
done

mkdir -p "$SDK_DIR" "$SRC_DIR" "$ENV_DIR/build"

if [ ! -d "$LIBSOPHON" ]; then
    echo "=== extracting libsophon 0.5.3 ==="
    tar -xzf "$PKG_DIR/libsophon_soc_0.5.3_aarch64.tar.gz" -C "$SDK_DIR"
else
    echo "=== libsophon already extracted ==="
fi

if [ ! -d "$FFMPEG" ] || [ ! -d "$OPENCV" ]; then
    echo "=== extracting sophon-mw 0.15.0 ==="
    tar -xzf "$PKG_DIR/sophon-mw-soc_0.15.0_aarch64.tar.gz" -C "$SDK_DIR"
else
    echo "=== sophon-mw already extracted ==="
fi

if [ ! -d "$SRC_DIR/sophon-sail" ]; then
    echo "=== extracting sophon-sail 3.11.0 source ==="
    tar -xzf "$PKG_DIR/sophon-sail_3.11.0.tar.gz" -C "$SRC_DIR"
fi

# Native aarch64 build.  BUILD_TYPE=pcie is the upstream name for "native
# host build" (no cross toolchain); it still produces a SoC-compatible .so.
echo "=== configuring sophon-sail ==="
cmake -S "$SRC_DIR/sophon-sail" -B "$BUILD_DIR" \
    -DBUILD_TYPE=pcie \
    -DBUILD_PYSAIL=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DINSTALL_PREFIX="$SAIL_PREFIX" \
    -DLIBSOPHON_BASIC_PATH="$LIBSOPHON" \
    -DFFMPEG_BASIC_PATH="$FFMPEG" \
    -DOPENCV_BASIC_PATH="$OPENCV" \
    -Dthe_libbmcv.so="$LIBSOPHON/lib/libbmcv.so"

echo "=== building sophon-sail (this takes several minutes) ==="
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "=== installing sophon-sail to $SAIL_PREFIX/sophon-sail ==="
cmake --install "$BUILD_DIR"

# Upstream bug: SAILConfig.cmake.in contains ${SAIL_LIB_DIRS}, but the
# configure_file() call expands it at configure time (empty) because @ONLY is
# not used.  Restore the variable so downstream find_package(SAIL) works.
SAIL_CONFIG="$SAIL_PREFIX/sophon-sail/lib/cmake/SAILConfig.cmake"
if [ -f "$SAIL_CONFIG" ]; then
    sed -i 's#PATHS  NO_DEFAULT_PATH#PATHS ${SAIL_LIB_DIRS} NO_DEFAULT_PATH#' "$SAIL_CONFIG"
    echo "=== patched SAILConfig.cmake find_library path ==="
fi

echo
echo "DONE."
echo "  libsail: $SDK_DIR/sophon-sail/lib/libsail.so"
echo "  headers: $SDK_DIR/sophon-sail/include/sail"
echo "Run 'source $ENV_DIR/env.sh' before compiling/running."

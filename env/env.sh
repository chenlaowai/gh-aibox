#!/bin/bash
# Sophon SDK environment for the AI box (BM1684X, aarch64).
# Usage: source env.sh
#
# The SDK is self-contained under env/sdk and does NOT modify /opt/sophon.
# This keeps the vendor image untouched and makes the same layout reusable
# inside the future Docker image.

_ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export SOPHON_SDK_ROOT="$_ENV_DIR/sdk"

export LIBSOPHON_HOME="$SOPHON_SDK_ROOT/libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3"
export FFMPEG_HOME="$SOPHON_SDK_ROOT/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-ffmpeg_0.15.0"
export OPENCV_HOME="$SOPHON_SDK_ROOT/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-opencv_0.15.0"
export SAIL_HOME="$SOPHON_SDK_ROOT/sophon-sail"

# SDK libraries must precede the older vendor runtime in /opt/sophon.
export LD_LIBRARY_PATH="$SAIL_HOME/lib:$LIBSOPHON_HOME/lib:$FFMPEG_HOME/lib:$OPENCV_HOME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$LIBSOPHON_HOME/bin:$PATH"
export SAIL_DIR="$SAIL_HOME/lib/cmake"

if [ ! -f "$SAIL_HOME/lib/libsail.so" ]; then
    echo "[env.sh] WARN: libsail.so not found, run env/setup_sdk.sh first" >&2
fi

#!/bin/bash
# Sophon SDK environment for the AI box (BM1684X, aarch64).
# Usage: source env.sh
#
# The SDK is self-contained under env/sdk and does NOT modify /opt/sophon.
# This keeps the vendor image untouched and makes the same layout reusable
# inside the Docker image.
#
# 2026-11-18: pinned to the production box's stack
#   libsophon 0.5.1-LTS (SoC build) + sophon-mw 0.14.0.
# Use the SoC tarball (libsophon_soc_0.5.1-LTS_aarch64.tar.gz), NOT the generic
# libsophon_0.5.1-LTS_aarch64.tar.gz: the generic build talks to /dev/bm-sophon
# and fails with "jdi_init: Can't open jpu driver" on BM1684X (which exposes
# /dev/jpu). That mismatch is why the 0.5.1 mixing attempt aborted before.

_ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export SOPHON_SDK_ROOT="$_ENV_DIR/sdk"

export LIBSOPHON_HOME="$SOPHON_SDK_ROOT/libsophon_soc_0.5.1_aarch64/opt/sophon/libsophon-0.5.1"
export FFMPEG_HOME="$SOPHON_SDK_ROOT/sophon-mw-soc_0.14.0_aarch64/opt/sophon/sophon-ffmpeg_0.14.0"
export OPENCV_HOME="$SOPHON_SDK_ROOT/sophon-mw-soc_0.14.0_aarch64/opt/sophon/sophon-opencv_0.14.0"
export SAIL_HOME="$SOPHON_SDK_ROOT/sophon-sail"

# SDK libraries must precede any vendor runtime in /opt/sophon.
export LD_LIBRARY_PATH="$LIBSOPHON_HOME/lib:$FFMPEG_HOME/lib:$OPENCV_HOME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$LIBSOPHON_HOME/bin:$PATH"
export SAIL_DIR="$SAIL_HOME/lib/cmake"

if [ ! -f "$SAIL_HOME/lib/libsail.so" ]; then
    echo "[env.sh] note: libsail.so not found; route B does not link sail" >&2
fi

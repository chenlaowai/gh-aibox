# Offline aarch64 image for the AI detection service.
#
# The box has no internet, so this image is based on the locally available
# `aibox-api:latest` (Debian 12 arm64) instead of pulling ubuntu:20.04.
# Runtime SDK libs come from env/sdk; OpenSSL 1.1 comes from docker/lib
# because Debian 12 ships OpenSSL 3 only.
FROM aibox-api:latest

WORKDIR /app

# Sophon SDK runtime (libsophon + mw + sail). Built/extracted by env/setup_sdk.sh.
COPY env/sdk /opt/sophon-sdk

# Some libsophon/bmlib paths are compiled as /opt/sophon/... even when the
# libraries come from env/sdk. Keep those legacy symlinks inside the image.
RUN mkdir -p /opt/sophon && \
    ln -sfn /opt/sophon-sdk/libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3 /opt/sophon/libsophon-current && \
    ln -sfn /opt/sophon-sdk/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-ffmpeg_0.15.0 /opt/sophon/sophon-ffmpeg-latest && \
    ln -sfn /opt/sophon-sdk/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-opencv_0.15.0 /opt/sophon/sophon-opencv-latest

# libsail / OpenCV need libcrypto.so.1.1 (Ubuntu 20.04); Debian 12 has only .so.3.
COPY docker/lib/libcrypto.so.1.1 /usr/lib/aarch64-linux-gnu/libcrypto.so.1.1
COPY docker/lib/libssl.so.1.1 /usr/lib/aarch64-linux-gnu/libssl.so.1.1

COPY build/aidetect/aidetect /app/aidetect
COPY docker/assets/no_task.jpg /app/no_task.jpg
COPY docker/labels /app/labels

# SDK ffmpeg is used only for `-c copy` when finalizing alarm videos.
RUN ln -sf /opt/sophon-sdk/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-ffmpeg_0.15.0/bin/ffmpeg /usr/local/bin/ffmpeg

# Full WenQuanYi font so every Chinese label/state glyph is available.
RUN mkdir -p /usr/share/fonts/truetype/wqy
COPY docker/fonts/wqy-zenhei.ttc /usr/share/fonts/truetype/wqy/wqy-zenhei.ttc

ENV LD_LIBRARY_PATH=/opt/sophon-sdk/sophon-sail/lib:/opt/sophon-sdk/libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3/lib:/opt/sophon-sdk/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-ffmpeg_0.15.0/lib:/opt/sophon-sdk/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-opencv_0.15.0/lib
ENV AIDETECT_PORT=9000
ENV AIDETECT_MODEL_ROOT=/models/BM1684X
ENV AIDETECT_DATA_ROOT=/data/CT/project/ai/inference/save
ENV AIDETECT_TMP_ROOT=/data/CT/project/ai/inference/video_transtation
ENV AIDETECT_CONFIG_BASE=http://10.10.206.68
ENV AIDETECT_REPORT_BASE=http://127.0.0.1

EXPOSE 9000

ENTRYPOINT ["/app/aidetect"]

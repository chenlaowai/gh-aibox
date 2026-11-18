# 路线 B PoC：sophon-ffmpeg 硬解 + bmcv + bmrt（无 sail）

> 目标：在改主程序前，用最小程序证明「生产同栈」可行，并量出与现有 sail 栈的差距。
> 运行环境：盒子 `10.10.206.25`（BM1684X，aarch64），SDK 为仓库自带
> `env/sdk`（libsophon 0.5.3 + sophon-ffmpeg 0.15.0），与容器内 `/opt/sophon-sdk` 同版本。

## 1. 两个工具

| 文件 | 作用 |
|---|---|
| `decode_probe.cpp` | 只解码：对比 `h264_bm`（VPU）与 `h264`（软件）的帧布局、CPU、fd、dmabuf |
| `poc.cpp` | 全链路：VPU 硬解 → bmcv 预处理(fpv 归一化) → bmrt 推理 → bmcv 画框 → bmcv JPEG |

关键点：**没有任何 `sail::` 调用，也不链接 `libsail.so`**；只链
`avformat/avcodec/avutil/bmcv/bmrt/bmlib`。

## 2. 编译与运行（在盒子上）

```bash
cd /data/CT/project/ai/gh-aibox-dev-src/gh-aibox-dev
source env/env.sh
CF="-std=c++17 -O2 -I$FFMPEG_HOME/include -I$LIBSOPHON_HOME/include \
    -L$FFMPEG_HOME/lib -L$LIBSOPHON_HOME/lib \
    -Wl,-rpath,$FFMPEG_HOME/lib -Wl,-rpath,$LIBSOPHON_HOME/lib"
g++ $CF tools/routeb_poc/decode_probe.cpp -o /tmp/decode_probe -lavformat -lavcodec -lavutil
g++ $CF tools/routeb_poc/poc.cpp -o /tmp/routeb_poc \
    -lavformat -lavcodec -lavutil -lbmcv -lbmrt -lbmlib

U='rtsp://admin:****@10.10.207.241/h264/ch1/main/av_stream'   # 数智技术中心_第二排 2560x1440
M=/data/CT/models/BM1684X/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel
/tmp/decode_probe "$U" h264_bm 200
/tmp/decode_probe "$U" h264    200
/tmp/routeb_poc  "$U" "$M" 120 /tmp/poc_out.jpg 0.25
```

## 3. 实测数据（2026-11-18 09:55，单路 2560×1440 H.264，盒子空闲时段）

| 路径 | 帧率 | CPU/帧 | 说明 |
|---|---|---|---|
| 软件解码 `h264` | 27.9 fps | **35.59 ms** | 现有 sail 走的正是这条路（`avcodec_find_decoder(codec_id)` → yuvj420p 在系统内存） |
| VPU 解码 `h264_bm` | 27.5 fps | **3.25 ms** | 同一路流，帧在设备内存（`data[4]/[5]` 为设备地址） |
| 全链路 PoC | 14.7 fps（源 15fps） | 13.24 ms | 含每帧 2560×1440→640×640 预处理 + fp32 归一化 + 推理 + 画框 + JPEG |

- **解码 CPU 降低 10.9 倍**：36 路 1440p 时，软件解码约 1.28 核/路（现网 6 核的元凶），
  VPU 解码约 0.12 核/路。
- 推理：`bmrt_launch_tensor_ex` **0.13~0.14 ms/帧**（yolov11s fp16 640×640，单实例）。
- 资源：120 帧全程 `fd 6 → 33`、`dmabuf maps 0`（无 mmap 增长），RSS 8.7MB → 36.3MB。
- 输出：`/tmp/poc_out.jpg` 1280×720 / 322416 B，画面与源一致（OSD 时间与墙钟一致）。

## 4. 已确认的接口事实（写代码时照抄）

1. **硬解帧布局**：`h264_bm`/`hevc_bm` 输出 `AV_PIX_FMT_NV12`，
   `data[4]`/`data[5]` 是 Y/UV 的**设备地址**，`linesize[4]`/`linesize[5]` 是 stride，
   `data[0]/[1]` 是同一块内存的 CPU 映射（不要用，避免 mmap 路径）。
2. **零拷贝接进 bmcv**：
   ```cpp
   bm_image_create(handle, h, w, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &img, stride);
   bm_device_mem_t p[2] = { bm_mem_from_device((u64)frame->data[4], h*stride[0]),
                            bm_mem_from_device((u64)frame->data[5], h/2*stride[1]) };
   bm_image_attach(img, p);
   ```
3. **送进 bmrt**：`bmcv_image_vpp_convert` 出 640×640 `FORMAT_BGR_PLANAR`；
   模型输入是 fp32 时，再 `bmcv_image_convert_to(alpha=1/255)` 出 `DATA_TYPE_EXT_FLOAT32`；
   `bm_image_get_device_mem` + `bmrt_tensor_with_device` 直接把图像设备内存当输入张量
   （**不拷贝**），输出张量用 `bmrt_tensor_ex` 预分配、每帧复用。
4. **JPEG 编码只吃 NV12**：`bmcv_image_jpeg_enc` 对 `FORMAT_BGR_PACKED`/`BGR_PLANAR`
   都报 `not support this image format`；必须先把画好框的图 `vpp_convert` 成 NV12 再编码。
   （现有 sail 的 `imencode` 走的是 `bm_image_to_mat` + `cv::imencode` 的 CPU 路径，
   这也是对比文档里“带框预览有 CPU 路径”的来源。）
5. `bmrt_launch_tensor_ex` 本身阻塞到推理完成，**不需要**额外的 `bmrt_thread_sync`
   （本版本无该符号）。

## 5. 尚未覆盖

- ISC 通道：ISC 流经 `IngestBridge` 转封装成本地 MPEG-TS 后进入同一个解码器，
  与 RTSP 只是 URL/容器不同；PoC 用 RTSP 验证了同一解码路径，ISC 留到集成时复验。
- 36 路并发下的 bmcv 设备侧排队（`vpp_convert` 单帧 22ms wall）需要在集成后用
  抽帧节奏（`frameInterval`）压住；这是下一阶段的主要观察点。
### 迭代 98 佐证图

`docs/assets/routeb_poc_frame.jpg`：`tools/routeb_poc/poc.cpp` 在 10.10.206.25 上
直连相机 `数智技术中心_第二排`（2560×1440 H.264），VPU 硬解 → bmcv 缩放/归一化 →
bmrt 推理 → bmcv JPEG 的实拍输出（1280×720，322416 B，OSD 时间与墙钟一致）。

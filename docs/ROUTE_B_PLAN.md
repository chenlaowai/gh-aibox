# 路线 B 实施计划（去 sail：bmrt + bmcv + sophon-ffmpeg 硬解）

> 状态：**PoC 已完成并验证**（迭代 98，`tools/routeb_poc/`）。本文是主程序改造的分步计划。
> 依据：PoC 实测 + 对 `env/src/sophon-sail/src/cvwrapper.cpp`（sail 源码就在盒子上）的核对。

## 1. 事实基线（不要重复调研）

| 事项 | 结论 |
|---|---|
| 现有解码 | `sail::Decoder` 内部是 `avcodec_find_decoder(codecpar->codec_id)`，本 SDK 上 = **软件 h264**（`yuvj420p`，系统内存），再 `copy_host_to_device` + `vpp_convert` |
| 换硬解 | `avcodec_find_decoder_by_name("h264_bm"/"hevc_bm")`，输出 `AV_PIX_FMT_NV12`，设备地址在 `frame->data[4]/[5]`、stride 在 `linesize[4]/[5]` |
| 实测收益 | 2560×1440：软件 35.59 ms CPU/帧 → VPU 3.25 ms（10.9×）；两路并发互不降速 |
| 推理 | `bmrt_load_bmodel` + `bmrt_launch_tensor_ex`，输入可直接用 bm_image 的设备内存（`bm_image_get_device_mem`），输出 `bmrt_tensor_ex` 预分配复用；0.13 ms/帧 |
| JPEG | `bmcv_image_jpeg_enc` 只吃 NV12；画框用 `bmcv_image_draw_rectangle`（BGR_PLANAR 可以） |
| 代码面 | `sail::` 共 121 处：`BMImage` 62、`Bmcv` 24、`Decoder` 16、`Handle` 13、`PaddingAtrr` 5、`Tensor` 5、`Engine` 3、`MultiDecoder` 3、`sail::tools` 6 |

## 2. 两条可选路线

### 路线 B1（推荐）：兼容层 `src/routeb/compat.hpp`

在 `namespace sail` 里用 `bmcv_*`/`bmrt_*`/sophon-ffmpeg 实现现有代码用到的
`Handle`/`BMImage`/`Bmcv`/`PaddingAtrr`/`Tensor`/`Engine`/`Decoder`/`MultiDecoder`，
把 6 个源文件的 `#include <sail/...>` 换成 `#include "routeb/compat.hpp"`，**调用点不动**。

- 优点：改动集中在 1 个新文件；121 个调用点的语义不变；可逐类型替换（先 Engine，再 Decoder，最后 Bmcv）。
- 缺点：`BMImage` 的移动语义/设备内存生命周期要自己保证；要用 `bm_image_destroy` 准确配对。
- 判定标准：`ldd aidetect | grep sail` 为空，且 36 路回归点全过。

### 路线 B2：直接改调用点

把 `bmcv.convert_format(x, FMT)` 逐个改成 `bmcv_image_vpp_convert(...)`。121 处、易漏，
且 `sail::BMImage` 的 RAII 还得替换，工作量大且回归面广。除非 B1 出问题，否则不选。

## 3. 分步实施（每步都可编译、可回滚）

| 步 | 内容 | 验收 |
|---|---|---|
| 0 | ✅ PoC（已完成） | 迭代 98 数据 |
| 1 | `compat.hpp` 的 `Engine`/`Tensor`（bmrt） | `model_probe` 改用 compat 后与 sail 版输出一致（同图同模型 top-k 相同） |
| 2 | `Handle`/`BMImage`/`PaddingAtrr`/`Bmcv`（bmcv） | 单路预览/告警图与改造前逐像素接近（允许 JPEG 量化差），`ldd` 里仍有 sail 也没关系（未调用） |
| 3 | `Decoder`/`MultiDecoder`（h264_bm/hevc_bm） | 1 路 RTSP + 1 路 ISC 出帧、重连、`frame_shape` 正确；`dump()` 先返回不支持（现网 `AIDETECT_NO_DUMP=1`） |
| 4 | 去掉 sail 头/库、CMake 清理 | `ldd` 无 libsail；`cmake --build` 通过 |
| 5 | 部署 1~2 路 | 告警图片/视频可出、预览正常、RSS/dmabuf/CPU 有对比数据 |
| 6 | 36 路全量 | 32 ok / 4 connecting（ISC 侧问题保持）、合流墙 8fps、告警视频可播 |
| 7 | 文档 + 本地提交 | PROGRESS/README/KNOWN-ISSUES/PRODUCTION_COMPARISON 更新；不推送 |

## 4. 关键实现细节（照抄 PoC）

```cpp
// 1) 解码帧 -> bm_image（零拷贝）
bm_image_create(handle, h, w, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &img, stride);
bm_device_mem_t p[2] = { bm_mem_from_device((u64)f->data[4], h*stride[0]),
                         bm_mem_from_device((u64)f->data[5], h/2*stride[1]) };
bm_image_attach(img, p);

// 2) 预处理（BGR_PLANAR 640x640）+ fp32 归一化
bmcv_image_vpp_convert(handle, 1, src, &dst, &crop);
bmcv_convert_to_attr a{}; a.alpha_0=a.alpha_1=a.alpha_2=1.f/255; a.beta_0=a.beta_1=a.beta_2=0;
bmcv_image_convert_to(handle, 1, a, &dst, &dst_f32);

// 3) 推理（输入零拷贝，输出复用）
bm_image_get_device_mem(dst_f32, &mem);
bmrt_tensor_with_device(&in, mem, BM_FLOAT32, net->stages[0].input_shapes[0]);
bmrt_launch_tensor_ex(rt, net_name, &in, 1, outs, net->output_num, true, false);
bm_memcpy_d2s(handle, host, outs[i].device_mem);
```

## 5. 风险与对策

| 风险 | 对策 |
|---|---|
| `BMImage` 移动/拷贝语义变化导致 double free 或悬垂 | 只实现移动语义（现状代码已是 move/emplace）；拷贝处逐个确认；ASAN 跑一遍单路 |
| `Decoder::read` 的返回码语义（`SAIL_ERR_*`）被调用方判断 | 用与 sail 相同的数值/枚举（源码在 `env/src/sophon-sail/src/internal.h`） |
| `dump()` 告警视频能力 | 现网走 `TsSegmentRecorder`（`AIDETECT_NO_DUMP=1`），先返回不支持并保留回退 |
| 部署 `kill -9` 偶发 PID1 僵尸 | 按既有铁律，接受偶发重启盒子；部署前先 `cp` 备份旧二进制 |
| 36 路 bmcv 排队（单帧 vpp 22ms wall） | 检测按 `frameInterval` 抽帧；预览/合流共用已缩放帧，避免重复 convert |

## 6. 回归清单（每一步都要过）

1. `ldd /opt/bin/aidetect | grep sail` 为空（步 4 之后）。
2. 单通道预览出图，OSD/时间正确。
3. 合流墙 `all_3`/`all_4` 九宫格有实时画面。
4. 触发一次告警：`/data/.../save` 下有 JPEG，告警视频 `ffmpeg -i` 可解析（5.2s/H.264/1280×720）。
5. `/proc/<pid>/status` RSS、`/proc/<pid>/fd` 数量、`/proc/<pid>/maps` 里 dmabuf 条目、`ps %cpu` 连续采样 10 分钟。
6. 重连次数：10 分钟内 0（测试源循环除外）。

# 实时预览重构：真 10fps、1080p 上限、按需渲染

> 关联需求：`重构AI盒子ai检测服务端.md`「实时预览页面」与 2026-11-05 的新要求：
> 在告警视频保持源帧率/分辨率的前提下，单通道与 3×3 合流的**真实内容**为 10fps；
> 源分辨率高于 1080p 缩到 1080p，低于 1080p 保持不变；源帧率高于 10fps 抽帧到 10fps，
> 低于 10fps 不补帧、不抽帧。

## 1. 结论

生产盒子（`10.10.207.46`）的实时预览不是"每路固定 1080p、固定 10fps 地重新编码"，
而是：

1. **每路只解一次码**；
2. 预览帧是解码器/VPU 的**按需缩放输出**（`DecodeInstance::requireScaled()` /
   `revokeScaled()`，请求方给出目标尺寸列表）；
3. 每个任务有一个**独立的 JPEG 编码实例**（`JpegEncoderInstance`），只在有观看者时
   编码，编码走硬件 JPEG（board SDK 的 `bm_jpeg_encoder` / `create_jpeg_encode`）；
4. 3×3 合流是独立的 `MergeInstance`，它同样有 JPEG 输出（`markJpegVideoOutput`），
   瓦片在硬件上完成缩放/拷贝后整图编码；
5. 网页端播放的是 **JPEG 流**（`MJpegInstance`、`Jpeg Stream Session`、配置项
   "Jpeg Stream Quality" 的说明就是 "web page plays videos via JPEG stream"）。

我们原来的实现与这个模型相反：每个任务无论有没有观看者都按固定 1920×1080 重编码，
合流线程再用 CPU 解码 9 张 1080p JPEG、拼图、再编码；30 路时 CPU/JPU 被这些"无人看"
的编码占满，真正被观看的通道反而掉到 2~5fps。本次按生产方案重构为**按需 + 硬件缩放
+ 真内容 10fps 门控**。

## 2. 生产盒子逆向（只读）

### 2.1 约束

- 全程只执行只读命令：`ps/ss/ls/cat/find/strings/nm/objdump/readelf/md5sum`；
- 分析用的二进制是**拷贝到开发盒**的副本，不在生产盒上执行任何写入、安装、调试；
- 复核：生产 `/data/app/edge_global.yaml` 的 mtime 仍是 `2025-01-01`，
  `app`/`libEdgeSystem.so` 的 mtime 仍是 `2025-12-15`，`edge.db` 等只有厂商进程自身
  的运行时写入（见 `docs/assets/prod_nochange.txt`）。

### 2.2 现场事实

| 项 | 观测值 |
|---|---|
| 主进程 | `./app`（`/data/app/app`，1,269,576 B，带 DWARF） |
| 线程/内存 | 1043 线程、RSS ≈ 1.4 GB、CPU 43.8%~70% |
| 监听 | 80、554（RTSP）、1935（RTMP）、58000/58002、10002/10010/10011/10099、9090 |
| 输入 | 多条 `10.10.69.250:1935` 的 RTMP 长连接（厂商流媒体/ISC 汇聚） |
| 预览相关库 | `libEdgeSystem.so` 35 MB（stripped，保留 RTTI/字符串）、`libboard_sdk_bm.so` 25 MB（带 DWARF） |

### 2.3 二进制证据

`libEdgeSystem.so` 的字符串/RTTI：

```
JpegEncoderInstance        MJpegInstance             MergeInstance
TaskInstance::markJpegVideoOutput
MergeInstance::markJpegVideoOutput
DecodeInstance::requireScaled(name, vector<cv::Size>)
DecodeInstance::notifyScaled()
DecodeInstance::revokeScaled(name)
realtime_scale.width:%f    realtime_scale.height:%f
Jpeg Stream Session:%s     Jpeg stream encode failed
QualityOfPreview           Jpeg Stream Quality
```

`libboard_sdk_bm.so` 的导出符号/DWARF：

```
board::bm_decoder::set_scaled_size(std::vector<cv::Size>&)   // 解码即缩放
board::base_frame_resize(frame, cv::Size)
board::create_jpeg_encode(int,int,int,int)                   // 硬件 JPEG
board::bm_jpeg_encoder::process(...) / config_quality(int)
board::frameToJpegByCpu(...)
board::get_jpeg_encoder(callback, ...)
```

配置帮助文本（生产 Web 配置项）也直接说明：

> "Quality of Preview Picture(0 means low,1 means medium,2 means high,default 1)"
> "Set the picture quality when the web page plays videos via JPEG stream ..."

### 2.4 还原出的生产预览链路

```
摄像头/RTMP ──> DecodeInstance（硬件解码，每路一次）
                    │
                    ├─ requireScaled(consumer, [目标尺寸...])
                    │      └─ notifyScaled(): 只给"有消费者"的预览输出缩放帧
                    │
                    ├─ AI 推理（独立线程/实例）
                    │
                    └─ JpegEncoderInstance（每任务一个，硬件 JPEG，按需）
                           └─ MJpegInstance / Jpeg Stream Session ──> 网页 JPEG 流

9 路预览帧 ──> MergeInstance（瓦片缩放/拷贝 + 整图 JPEG 输出）──> 合流 JPEG 流
```

要点：**"按需"是生产方案的核心**。没有观看者的通道不做缩放、不做 JPEG 编码；
有观看者时才 `requireScaled`，关闭页面后 `revokeScaled`。这解释了为什么同样的
BM1684X 硬件，生产能同时跑几十路检测而不掉预览。

## 3. 重构前的实现与问题

| 问题 | 原实现 | 后果 |
|---|---|---|
| 无人看也编码 | `Task` 每 100ms 固定渲染 1920×1080 JPEG | 30 路 ×10fps 的 JPEG 编码把 JPU/CPU 占满 |
| 固定 1920×1080 | `preview_w_/preview_h_` 默认 1920×1080 | 704×480 被放大到 1080p；2560×1440 也只能整帧 CPU 拷贝 |
| CPU 往返 | `to_mat()` + `cv::resize` + `cv::rectangle` + `mat_to_bm_image()` | 每帧 2 次 6 MB 级拷贝，1080p 单帧耗时约 100ms |
| 合流重复帧 | `Aggregator` 每 100ms 发一次"最新图"，与内容更新无关 | WS 10fps，但内容是 2fps 重复帧 |
| 合流 CPU 拼图 | 9×`cv::imdecode` + `cv::resize` + `cv::imencode` | 1080p 拼图编码慢，掉到 6~7fps |
| 双解码 | 部分版本为预览再起一个 ffmpeg 解码进程/文件 | 每路两倍解码开销 |

## 4. 重构方案（已落地）

### 4.1 按需渲染

- `StreamHub` 记录每个 `taskId` / `all_1..all_4` 的订阅计数；
- `WsSession` 在订阅切换/断开时 `note_subscribe/note_unsubscribe`；
- `Task::maybe_preview()`：
  - 有单通道观看者：生成"单通道预览"（1080p 上限）；
  - 只有合流观看者：生成 640×360 合流瓦片（硬件 VPP + 硬件 JPEG）；
  - 两者都没有：仅 1fps 生成一张给 `/task/get_area_frame` 用的最新图；
  - `AIDETECT_NO_PREVIEW=1` 时完全关闭。
- `Aggregator` 只在 `all_N` 有订阅者时工作；没有订阅者时 0 编码、0 发送。

### 4.2 分辨率规则

`Task::preview_target(w,h)`：

```text
w <= 1920 && h <= 1080  ->  (w, h)                       // 原样，不放大
否则                    -> 等比缩放到能放进 1920x1080       // 只缩小
```

例：2560×1440 → 1920×1080；1080×1920 → 608×1080；1280×720 → 1280×720；
704×576 → 704×576。

### 4.3 10fps 真内容门控

`Task::preview_due()` 使用**令牌桶**（容量 2，速率 1000/previewIntervalMs）：

- 源 ≥10fps：按 10/s 消耗令牌，25fps 时输出 10 个**不同的**解码帧，不重复、不补帧；
- 源 <10fps：帧间隔大于 100ms，令牌每次都够，**每个解码帧都输出**，输出帧率等于源帧率；
- 不使用"每 100ms 取最新帧再重复发送"的旧逻辑。

### 4.4 单通道硬件路径

`Task::render_preview_fast()`：

```
解码帧 (NV12/YUV)
  └─ bmcv.vpp_resize(目标尺寸)          // VPP 硬件缩放
      └─ bmcv.vpp_convert_format(BGR_PACKED)
          ├─ bmcv.rectangle(...)        // 设备上画检测框
          ├─ bmcv.image_copy_to(plate)  // 设备上贴中文告警标签
          └─ bmcv.imencode(".jpg")      // JPU 硬件 JPEG
```

告警图片仍走原来的 CPU 路径（`render_jpeg`，带 Pillow 中文标签），因为告警是低频事件，
需要最高可读性。

### 4.5 合流硬件路径

`Aggregator::loop()`（每个合流组一个线程）：

```
all_N 有订阅者?
  ├─ 否: sleep 100ms（不拼图、不编码、不发送）
  └─ 是: 等待 dirty（瓦片更新） -> 10fps 槽位 -> 硬件拼图:
           tile JPEG --bmcv.imdecode--> 640x360 --image_copy_to--> 1920x1080 画布
           --bmcv.imencode--> JPEG --WS 发送
```

- 只有"有内容更新"才发送，发送帧率 ≤10fps，且每帧都是新内容；
- 槽位空/任务未配置时使用 `no_task.jpg`/黑底；
- 硬件路径失败时自动回退 OpenCV CPU 拼图（保证可用性）。

### 4.6 告警视频保持源帧率/分辨率

- 每任务一个常驻 `ffmpeg -c copy -f segment` 环形录制（2s 分片，10 片）；
- 告警时 `clip_recorder()` 直接 `-c copy` 拼接最近 5 个分片（10s），**不转码**；
- 修改后单/多解码模式都优先走环形录制，失败才回退到 `decoder.dump`/worker；
- 因此预览重构不会改变告警视频的帧率和分辨率。

## 5. 代码落点

| 文件 | 变更 |
|---|---|
| `src/server.hpp` | `StreamHub` 订阅计数、`WsSession` 订阅生命周期、`Aggregator` 按需+硬件+内容驱动发送、`TaskManager` 注入预览路由 |
| `src/task.hpp` | 删除 ffmpeg 预览管线；新增 `preview_target/preview_due/maybe_preview/render_preview_fast/render_tile_fast`；告警视频优先环形录制 |
| `docs/PREVIEW_PIPELINE.md` | 本文（逆向 + 方案 + 验证） |

## 6. 与生产方案的差异

| 生产 | 我们 | 原因 |
|---|---|---|
| 解码器级 `set_scaled_size`（解码即缩放） | SAIL `vpp_resize`（解码后 VPP 缩放） | 我们使用 SAIL 3.11 的 `Decoder`，没有暴露解码器缩放接口；VPP 缩放同样是硬件，成本很低 |
| 每任务独立 `JpegEncoderInstance` | 每任务共享全局硬件 JPEG（`jpeg_hw_mutex` 串行） | SAIL `Bmcv::imencode` 的 JPU 调用需要串行；按需后并发编码量已很小 |
| WebRTC/RTMP/JPEG 多种播放 | 保持现有前端协议：`WS /ws/frame` + JPEG | 前端已定型，不能改协议 |
| 合流瓦片缩放/拷贝全部在设备上 | JPEG 瓦片由 JPU 解码后 `image_copy_to` 拼图 | 瓦片是跨线程按需产生的 JPEG；已完成设备内拼图+编码，无 CPU 拼图 |

## 7. 验证

见 `docs/PROGRESS.md` 迭代 6 与 `docs/assets/preview_verify.txt`。核心结果：

| 场景 | 源 | 输出分辨率 | 内容帧率 | 重复帧 |
|---|---|---|---|---|
| 单通道 2.5K | 2560×1440@25 | 1920×1080 | 9.9~10.0 fps | 0 |
| 单通道 1080p | 1920×1080@25 | 1920×1080 | 10.0 fps | 0 |
| 单通道 11fps | 704×480@11（实时 UDP） | 704×480 | 10.0 fps | 0 |
| 单通道 9fps | 704×576@9（实时 UDP） | 704×576 | 9.0 fps | 0 |
| 单通道 1280×720 | 1280×720（容器真实通道） | 1280×720 | 9.88 fps | 0 |
| 3×3 合流（3 路活动） | 混合 | 1920×1080 | 9.8 fps | 0 |
| 单通道（容器 30 任务，6488） | 2560×1440@25 | 1920×1080 | **9.88 fps** | 0 |
| 3×3 合流（容器 30 任务，all_3） | 混合 | 1920×1080 | **9.75 fps** | 0 |
| 告警片段 | 2560×1440@25 源 | 2560×1440 | 24.9 fps（-c copy） | - |

> "内容帧率"按 WS 每帧 JPEG 的 SHA1 去重统计；"重复帧"指与历史帧字节完全相同的帧。
> 低帧率 9fps 源输出 9 个不同帧/秒，证明"低于 10fps 保持不变"；11fps 源输出 10 个
> 不同帧/秒，证明"高于 10fps 抽帧到 10fps"；2560×1440 输出 1920×1080 证明"只缩不放"。

视觉证据：

- `docs/assets/preview_single_2560x1440_to_1080p.jpg`
- `docs/assets/preview_single_704x480_label.jpg`
- `docs/assets/preview_single_1080p_boxes.jpg`
- `docs/assets/preview_merge_1920x1080.jpg`
- `docs/assets/ui_preview_20261105.png`（Web 端实时预览页实测）

## 8. 2026-11-10 合流实时性/重连 OSD 修复

### 8.1 慢动作根因

上一版把 `maybe_preview()` 直接放在解码线程里：9 个 task 都要
`bmcv.imencode` 640×360 JPEG，而 JPU 调用被全局 `jpeg_hw_mutex()` 串行化。
解码线程等待编码时，SAIL `Decoder::read()` 仍在交付越来越旧的包，合流
画面时间流速就落后于墙钟，且越积越多；单通道只有 1 路编码，所以看起来
是同步的。

### 8.2 实现

- 每任务 `preview_thread_` + `preview_mu_/preview_cv_`；解码线程在 worker
  空闲时把当前帧 `image_copy_to` 到 `preview_frame_`，worker 完成后
  `preview_busy_=false`。busy 时直接丢帧，**不排队**，因此合流永远拿最新帧。
- 合流仍是 Aggregator 10fps 内容驱动；tile 仍是 640×360，单通道仍按源分辨率
  只缩不放；状态/告警/ROI 逻辑不变。
- 实测（`.25`）：12:46:32→12:47:36 共 64s，all_1 左上 tile OSD
  12:47:04→12:48:08（64s），时间差不再累积；all_1 600 帧/64.1s≈9.36fps。

### 8.3 重连状态心跳

`Task::status_thread_` 每 500ms 检查 `last_frame_ms_`；超过 3s 无帧就
1Hz 重发 `publish_status`，画面时钟和“已断开 Ns”持续更新；恢复后发一次
“监控视频流已恢复”。单通道有观看者时状态图 1280×720，否则 640×360。
前端仍保留 10s 兜底文案和“重新连接”按钮。

### 8.4 任务编号 OSD

生产 `libEdgeSystem.so` 的证据链：

```text
MergeInstance::setTaskInstance(int, shared_ptr<TaskInstance>)
MergeInstance::getGroupSnapshot / onChannelImageArrived / onMergeFinished
OsdConfig / EnableOsdRender / board::render::draw_text_t / osd_on_frame
字符串: AlgTaskSession
```

对应行为：每个 task 的画面右上角是黑底白字任务名；重连 status tile 也有；
`no_task` 是 MergeInstance 生成的占位图，没有任务条。我们按 `taskName`
（`AlgTaskSession`）在 tile 上绘制同样的 plate，未配置格不绘。

### 8.5 通道 4 / 槽位

- AI 端 4 组×9 槽位；`VideoRoute=36` 时后端 `GetTaskDropdown` 返回
  `all_1..all_4`，前端再兜底补 `all_4`。
- `TaskManager::assign_slot` 首空顺序分配：先填满 group0 的 9 格，再
  group1…；停止任务只 `Aggregator::clear(group,slot)`，不移动其它格；
  新启动填回第一个空槽。
- 实测 35 task：group0/1/2 各 9 格、group3 0..7，第 36 格“任务未配置”。
  停 `7c3af4cc`(group0 slot8) 后只有右下变未配置，重启后回 slot8。

### 8.6 验证图

- `docs/assets/preview_merge_task_osd_20261110.jpg`（9 格任务条）
- `docs/assets/preview_single_reconnect_20261110.png`（单通道重连状态）
- `docs/assets/preview_no_task_slot_20261110.jpg`（停任务留空且无任务条）
- `docs/assets/preview_channel4_20261110.jpg`（通道4 + 未配置格）
- `docs/assets/preview_channel4_web_20261110.png`（Web 菜单通道4 实测）

## 9. 2026-11-10 框同步/配置原图/ROI 区域线

- **框滞后根因**：`dets_ms_` 记的是推理完成时间，而框属于更早的推理帧；
  `infer_q_` 还保留 2 帧 FIFO。改为 `InferJob{img, frame_ts}`、推理队列
  latest-only、`dets_ms_=frame_ts`，预览按帧龄外推（上限 1.5s）。
  生产证据：`FramePackage::setBoxes/getBoxes` 按帧号存框，
  `ImageTaskSpace::onFrameDetectFinished`、`board::track::get_iou_tracker`。
- **配置原图**：`/task/get_area_frame` 改用 `area_jpeg_`（空 dets、无任务
  条、无 ROI 的原图），不再返回带框的 `latest_jpeg_`。
- **区域线**：`taskArea` 解析为归一化 `roi_polygons_`，用绿色闭合折线画在
  单通道、合流 tile、告警图上；配置原图不画。生产样式见
  `docs/assets/prod_prev_0.jpg` 的绿色多边形。ROI 点串纳入
  `config_signature`，保存后 2s 轮询热更新，14K 保存后单通道立即
  出现绿色多边形（`preview_14k_roi_20261110.jpg`、合流
  `preview_merge_roi14k_20261110.jpg`）。
- **验证图**：`preview_merge_box_sync_20261110.jpg`、
  `preview_area_raw_20261110.jpg`、`preview_single_roi_20261110.jpg`、
  `preview_merge_roi_20261110.jpg`、`preview_alarm_roi_20261110.jpg`、
  `preview_start37_guard_20261110.png`。

## 10. 运行中任务同步 ROI/算法参数

- `main.cpp` 原来只在启动 2s 后调用一次 `reconcile_from_backend()`，所谓
  10s 配置检查再没执行；签名也不含 ROI 和 `threshold_for_people`。
- 生产 `handleRemoteMessage` 解析保存消息后调用 `onTaskArrived`，再走
  `TaskChannel::archiveUpdated` → `TaskInstance::taskArchiveApply`；不重建
  TaskChannel。
- 适配：前端任务/ROI 保存成功后直连 `POST 9000 /task/reload_config`
  （JWT+CORS），Task 在推理线程帧间重建 `models_`/ROI，不释放槽位；
  2s 轮询保留为兜底，仅源地址/任务名变化才 stop/start。
- 实测 Playwright 在 14K ROI 页保存后，16:27:49 仅出现
  `direct hot update: 6488` + `hot config applied models=3 rois=1`，无
  `task started`；models 从 3 个用户算法变 2 个，预览旧标签消失。
  14K ROI 保存后单/合流绿线出现。

## 11. ROI 过滤框/标签/告警

- 生产 `SystemAlarmFilter` 的 `BoxInRoiRegion` / `isBoxCenterInRegions`
  按框中心判断 TaskRegion。
- 我们统一 `filter_roi_polys`：算法自己的 `taskArea` 优先，为空回退任务级
  `roi_polygons_`；`update_alarm` 入口过滤，`detect()` 写 `dets_` 前再兜底
  过滤，区域外目标不画框、不打标签、不告警。
- 验证：7c 低阈值下 ROI 外目标全部消失
  （`preview_roi_filter_outside_20261110.jpg`）；6488 近全屏 ROI 内目标
  仍保留框/标签（`preview_roi_filter_inside_20261110.jpg`）。

## 12. 无任务通道状态/分辨率

- 生产把 MediaChannel 与 TaskChannel 分开，所以没任务也能显示通道状态。
- 本盒子不常驻 39 路 MediaChannel：通道页对无运行任务的在线通道调用
  `POST /channel/probe`，ISC resolve 后 `ffmpeg -c copy` 只读流头 6s，
  解析分辨率，60s 缓存、2 并发、不进 BPU；有任务通道用 Task 的
  `health/resolution`。
- 实测无任务直连 `数智技术中心_茶水间 正常 2560x1440`，无任务 ISC
  `五公司-德阳项目-安家村-冲洗区1 Input/output error`。

## 13. 2026-11-12 框按帧绑定（迭代 39）

生产盒子把检测框存进 `FramePackage::setBoxes(frameId, boxes)`，预览消费
`getBoxes(frameId)`；`board::track::IoUTracker` 只负责目标 id，不负责把旧框
外推到新帧。我们此前用“最新检测 + 速度外推 + LK”仍会在检测延迟时把框按在
旧位置，并在人出 ROI 后靠 `rt.active` 的 1s TTL 留边缘框。

现在的实现（`src/task.hpp`）：

1. **轨迹历史**：`PreviewTrack` 保存 `(ts, box)` 最近 3s 历史、`Detection proto`、
   LK 点、速度；`dets_history_` 保存最近 20 个 `(frame_ts, dets, persons)`。
2. **按检测帧关联**：新检测到达时，用 `dets_ts` 找轨迹在检测帧的姿态，
   只应用 `det_box - hist_box` 的增量；框尺寸按面积比平滑修正。绝不会把框
   重置回检测帧的旧位置。
3. **帧间推进**：960 宽灰度（VPU `vpp_resize` + `vpp_convert_format`，不再
   在 CPU 转换 2560×1440 全帧），25 点 LK + 前后向校验；失败时用内框
   `matchTemplate` 兜底；再失败用短时速度外推。
4. **人员锚点**：`dets_persons_` 保存全帧人员框（base people，不画）。属性
   算法漏检时，把属性框直接吸附到人员框在检测帧的位置，避免属性框留在
   空过道；无人员锚点的属性轨迹删除。
5. **ROI 当帧删除**：每预览帧用框中心对 ROI 多边形判断；人出线当帧不再画，
   不再保留 1s。
6. **CPU 保护**：跟踪只在有订阅者时运行；`preview_prev_ts_` 闲置 >1.5s
   自动清轨迹；960 宽灰度 + 模板匹配只在 LK 失败时运行。

验证：`docs/PROGRESS.md` 迭代 39；生产逆向
`docs/assets/prod_box_frame_binding_20261112.txt`；回放图
`preview_track_follow_20261112.jpg`、`preview_track_two_people_20261112.jpg`、
`preview_track_edge_gone_20261112.jpg`；最终现场边缘前后帧
`preview_track_edge_last_box_20261112.jpg` →
`preview_track_edge_cleared_20261112.jpg`。

## 14. 2026-11-12 推理调度公平化与标签合一（迭代 40）

- **背景**：15 任务并行时全局 `infer_mu_` 让测试_2（4K、5 个算法）一轮检测
  1.7s，检测快照超过预览端 1.5s 新鲜度上限，画面完全没有框。根因与生产对照见
  `docs/assets/prod_infer_scheduler_syms_20261112.txt`，实测见
  `docs/assets/infer_scheduler_measure_20261112.txt`。
- **推理侧**：FIFO 票据闸门 + `AIDETECT_INFER_CONCURRENCY`（默认 2）；每任务检测
  节奏 `AIDETECT_DETECT_INTERVAL_MS`（默认 300ms，4K 源 ×1.5）；裁剪算法只取
  `AIDETECT_CROP_PERSONS`（默认 4）个最大人形框。
- **预览侧**：
  - 同一目标被多个算法命中时按 IoU≥0.45 聚类，公共框取最大框，每个算法各出一个
    标签 → 渲染成“一个框 + 竖排多标签”，不再出现嵌套/偏移的重复框。
  - 属性漏检时，只要有全帧人形框锚点就保留轨迹 600ms（`last_person_ms`），
    不再因一次漏检闪烁；整帧既无属性框也无人员框时不判定目标消失。
  - 合流瓦片单独令牌桶 `tile_interval_ms_`（默认 120ms），避免 9 格瓦片和单通道
    抢同一个 10fps 预算。
- **验证**：`preview_test2_boxes_15tasks_20261112.jpg`、
  `preview_test2_follow_20261112.jpg`、`preview_test2_follow2_20261112.jpg`；
  改造前同条件 `preview_test2_starved_15tasks_20261112.jpg`（无框）。
## 15. 2026-11-12 标签文字与目标状态（迭代 41）

- **标签底板**：`Task::ensure_label_plate` 在任务启动/热更新时，为本任务所有算法的
  `report_content` 用 FreeType 现渲染红底白字底板；`/app/labels/label_*.png` 只覆盖
  6 个历史名字，`抽烟/打电话`、`人员聚集`、`人数监控` 过去没有底板，导致快速预览
  路径不画标签、jpeg 路径画纯红条（FreeType badge 原点用错）。
- **状态合一**：同一目标多算法命中时按 IoU≥0.45 聚类，公共框取最大框；有 confirmed
  只画红色标签，否则只画一个蓝色倒计时（不再出现蓝框 + 红标签 + 多个数字重叠）。
- **轨迹关联**：只比 `label` 不比 `class_id`（行为模型每轮跳类），否则抽烟轨迹
  每轮重建、活不过 600ms。
- **验证**：`preview_test2_smoking_label_20261112.jpg`（抽烟/打电话 + 未穿救生衣 +
  未穿反光衣三行有字、贴同一框）、`preview_test2_pending_countdown_20261112.jpg`
  （单个 5s 倒计时）。
## 16. 2026-11-13 标签底板按实际字形裁切（迭代 42）

- `ensure_label_plate` 不再用 `getTextSize()` 估算：在 96x640 红底画布上 `putText`，
  用 `inRange(白)` + `boundingRect` 取真实字形框，四周 8/4 px 裁切后缓存。
  `抽烟/打电话` 底板 197x34 → 182x39，末字完整。
- 33 任务满载时预览约 2fps 属容量限制（推理闸门 `wait_avg=0`，并发 2/4 无差别），
  见 KNOWN-ISSUES KI-031，不要误判为调度问题。
## 17. 2026-11-13 标签排布/文案与预览对齐生产（迭代 43）

- **空行根因**：行为模型用 1.10 倍扩边裁剪框上报，比其他算法的人形框高 6px，
  两段标签栈起点不同 → 视觉上“空一行”。已改回人形框上报，实测 6px → 2~3px。
- **文案**：`display_label` 按任务开启的行为开关拼接（抽烟 / 打电话 / 玩手机，
  多个用 `/`），同一目标只出一组标签。
- **底板**：`tools/gen_label_plates.cpp` 构建期生成 `docker/labels/plate_*.png`
  与 `labels.json`（16 个标签），运行时只读清单；缺标签才回退现画并 WARN。
- **预览分辨率**：单通道对齐生产 1280×720（实测生产网页 `<img>` 为 1280×720，
  合流 1920×1080）；`AIDETECT_PREVIEW_WIDTH/HEIGHT` 可强制覆盖。
## 18. 2026-11-13 每帧只缩放一次 + 每算法节奏（迭代 44）

- `maybe_preview` 先把源帧 `vpp_resize` 到 `preview_target()`（默认 1280×720），
  跟踪、单通道渲染、合流瓦片都用这张缩放帧；检测框在跟踪前换算到预览坐标，
  渲染时 `sx=1`。对齐生产 `DecodeInstance::requireScaled()`。
- 效果（33 任务）：`render_ms` 30~268 → 20~67ms；合流 2.8 → 5.6~7.0fps；
  2.5K 单通道 6.9~7.9fps；4K 源单通道 3.2~3.7fps。
- 裁剪算法按 `AIDETECT_ALG_INTERVAL_MS`（默认 400ms）独立刷新，未到点直接跳过；
  标签靠跟踪的人形锚点维持（守护窗口 = max(600ms, 最大间隔×1.5)）。
## 19. 2026-11-13 检测裁剪也用缩放帧（迭代 45）

- 有人可裁时，`detect()` 每轮把源帧缩到 1280×720 一次（`det_src`），
  人形框按 `det_scale` 换算后从缩放帧裁剪；人形检测仍用原图（框坐标不变）。
- 测量口径：容器启动后 ≥3 分钟、33 任务全部 running、连测 3 次。
- 33 任务稳定后：合流 **6.5~7.1fps**（生产 22 任务 8.7~9.2）、all_2 5.2fps、
  2.5K 单通道 6.3~8.1fps、4K 测试源单通道 2.5~3.2fps。
## 20. 没人看时不编码：检测与预览是两条独立链路（2026-11-13 验证）

- 生产（反编译）：`DecodeInstance::onYuvDataArrived` 把解码帧组成 `FramePackage` 推入
  `ThreadSafeLazyQueue<FramePackage>`，推理线程 `ImageTaskInstance::workLoop` 从这里取帧做检测；
  JPEG 预览是另一条 sink —— `JpegEncoderInstance::onImagePoped` 先调 `MJpegInstance::anyWatcher()`，
  没人看就直接返回，不建编码器也不编码。告警图片只在告警时刻用 `frameToJpegByCpu` 编一张。
- 我方等价实现：`infer_loop()` 从 `infer_q_` 取帧跑 `detect()`，与预览无关；
  `maybe_preview()` 在 `!want_single && !want_tile` 时直接 return；
  `save_alarm()` 只在告警发生时 `render_jpeg()` 一张（尺寸 = 源尺寸）。
- 实测（本机 32 任务、无任何 WS 订阅、开启 profile 14s）：`detect round` 194 条，
  `preview render` 0 条、`preview path` 0 条 —— 没人看不编码，但检测照常。
- 推论：合流墙有人看时，九个任务会按需出 640×360 瓦片（等价生产的 MergeInstance），
  这是“有人在看”的场景，不影响检测链路。

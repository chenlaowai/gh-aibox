<!-- pm:meta
iteration: 106
updated: 2026-11-19
commit: -
-->

# 迭代台账

> 只追加，不修改旧条目；最新在最上面。
> 一条 = 一次可验证的交付，不是「今天工作了 6 小时」。
> 「验证」必须写命令 + 真实结果，只写「测试通过」等于没写。
### 迭代 106 — 2026-11-19 — dma-buf 映射 290→0、fd 930→589；根因是 Sophon OpenCV 的 cv::Mat 分配

- **用 LD_PRELOAD 抓到映射来源**：拦截 `mmap` + `/proc/self/fd` 判定 dmabuf，70s 内 5788 次映射的调用栈
  全部落在 `cv::Mat::create`（libopencv_core）→ `Aggregator::loop` / 任务渲染。Sophon OpenCV 的
  `cv::Mat` 走 bmcv/ion 分配，每个 Mat 就是一条 dma-buf 映射；OpenCV 内存池把释放的块留在进程里，
  所以映射数固定约 290、与任务/画面数无关。
- **根因拆解**：每任务缓存的中文标签牌 `label_plates_`（36 任务 × ~8 张 = 288）+ 合流每组的
  black/no_task 图（8）= 290。
- **修复**：
  1. `HostMat`：标签牌改为 `std::vector<unsigned char>` 存像素，用时才 `cv::Mat(h,w,CV_8UC3,buf)`
     包一层视图；`plate_bm()` 改为 host Mat → `mat_to_bm_image` → VPP 缩放，不再每帧分配 cv::Mat。
  2. 合流 black/no_task 与 CPU fallback 画布改为 host buffer + Mat 视图；`no_task` 不再 `cv::imread`，
     直接 host `putText("NO TASK")`（imread 的池化块是最后 4 条映射）。
  3. 合并 `Aggregator` 的 `hw_ok`：从“一次异常永久关硬件”改为**每帧重试**（之前一个瞬时失败就把合流
     永久打到 CPU fallback，691KB/帧的 cv::Mat churn）。
- **效果（15:27~15:48 实测，34 路有画面）**：**dmabuf 映射 0、dma-buf fd 0**、总 fd **617**、CPU 235%、RSS 1063MB、线程 282；
  36 任务 **34/36**（仅凤凰咀两路 5XX 外部）；restarts=0。
  生产同口径：dma-buf 映射 0、dma-buf fd **135**、总 fd 417（10 路）→ 每路 fd 17.3 vs 41.7，我方更低。
- **踩坑**：尝试把 `Bmcv::imencode` 的 NV12 转换图做成 per-instance 复用（BMImage 成员）后，
  运行约 3 分钟出现 `*** stack smashing detected ***` 崩溃；已回退为每次调用新建 NV12 图（现版本稳定）。
  说明该复用与多线程/`bm_image` 生命周期存在冲突，暂不再用。
- **帧率（同一时段）**：生产单通道 25K 5.0fps、14K 4.13fps、八所 1.6fps、合流 10fps；
  我方 25K 1.45~4.88fps（随负载波动）、合流 2.9~6.3fps。合流仍低于生产（其 board_sdk
  `create_merger` 是原生合流线程）。
- **生产栈差异（回答“为什么它没有 dma-buf/能否对齐”）**：生产 `/data/app` 加载
  `libEdgeSystem.so`/`libboard_sdk_bm.so`/`libmk_api.so`，符号为 `board::create_decoder/
  create_encoder/get_jpeg_encoder/create_merger` + `media_sdk::codec_data`；其 libsophon 是
  **0.5.1 + 0.10 子库（2024-06）**，reference 里只有 0.14（2025-08），且没有这些私有库。
  生产并非“不分配 dma-buf”（它有 135 个 fd），而是**不做进程 mmap**；我们通过改自有 Mat 分配
  已把映射降到 0，不再需要替换解码栈。

### 迭代 105 — 2026-11-19 — RTSP/MP4/ISC 无画面根因是 VPU core 楔死；解码器常驻化 + core 钉选；dma-buf/fd 归一化分析

- **现象（10:04）**：36 任务只有 **16 ok**；部分 RTSP（数智技术中心_第一排/第三排/大门/培训中心）与
  多个 MP4 本地文件任务 `everFrame=false`；5 路 ISC（14K/28K/钢栈桥/预制场/乙烯南路2）`5XX`。
  用户确认这些 RTSP/ISC 在生产侧都有画面。
- **定位（不是取流问题）**：
  1. 失败的 RTSP/MP4 任务日志都是 `ingest bridge pid=none udp ...` 后 30s `no first frame`；
     直接对同一 RTSP URL 跑 `ffmpeg`：**能连上并收到码流**，但解码报
     `h264_bm/h265_bm: pkt can't be sent to decoder, ret=-2`；MP4 文件存在且可读。
  2. `/proc/vpuinfo`：core0 `link_num=16`（占满）、core1 只有 4，且 core1 `usage 2%`；
     `-core_idx 0` 显式指定时能解码，`-core_idx 1`/auto 报同样的 `ret=-2`。
     → **VPU core1 已被楔死**，auto 选核把新解码器分到坏 core，永远起不来。
  3. 容器重启只恢复到 3/36（VPU 固件状态在容器外），**重启盒子后 34/36**（仅凤凰咀两路 5XX）。
- **修复**：
  1. `sail::Decoder` 增加 `core` 参数，按 taskId 哈希钉 core 0/1；重建解码器时**翻转 core**，
     避免坏 core 成为永久死角。
  2. **解码器常驻化**（对齐生产 `DecodeInstance` 常驻）：读失败时不再销毁/重建解码器，而是
     `resume_keep_ports_` + `stop_gateway(true)` 在**同一对 UDP 端口**上重启 gateway，保留 decoder
     对象；连续 2 次无效才重建（并翻转 core）。watchdog 重启 gateway 也保留端口/解码器。
     —— 之前每 5 次读失败重建一个 ~85MB 解码器，正是 VPU link 抖动与 core 楔死的放大器。
- **修复后（10:35 部署）**：**34/36 ok**（仅凤凰咀-游客中心/博物馆 5XX，fresh RTSP 与 RTMP 都失败，
  外部）；`/proc/vpuinfo` core0/core1 `link_num=16/18` 均衡、`restarts=0`、无 FATAL。
- **fd / dma-buf 归一化（回答“能否与生产完全一致”）**：
  | 指标 | 生产（10 有画面 / 21 任务） | 我方（34 有画面 / 36 任务） | 每路归一化（生产 vs 我方） |
  |---|---|---|---|
  | 进程 fd | 417 | **893~915** | 41.7 vs **26.6** |
  | dma-buf fds | **135**（13.5/路） | **612~615**（18.1/路） | 13.5 vs 18.1 |
  | dma-buf 映射 | **0** | **290（固定，与路数无关）** | 0 vs 8.5 |
  | dma-buf 对象（bufinfo） | 593 | **~1398** | 59.3 vs **41.1** |
  | 线程 | 1020 | **282** | 102 vs 8.3 |
  | RSS | 1158 MB | **1213 MB** | 116 vs 35.7 MB |
  | CPU | 88% | **229%** | 8.8% vs **6.7%** |
  结论：**fd/CPU/dma-buf 对象都随“有画面路数”线性增长**；按每路归一化，我方 fd/对象/RSS/CPU
  都低于生产，绝对数高是因为我们有 34 路有画面（生产 10 路）。
  dma-buf 映射归零只能去掉我们自己的图像分配：已实现 `BMImage` 的
  **`bm_malloc_device_byte` + `bm_image_attach`** 路径（`AIDETECT_ATTACH_MEM=1` 可选），
  实测 dmabuf fd 644→450、对象 2155→1237、总 fd 930→713；**但 VPP 写 attached 内存会让
  预览画布/合流瓦片出现彩色噪声条纹（视觉验收发现）**，因此默认关闭、回退到连续堆分配。
  剩余 **290 个映射（固定值，与路数无关）** 来自库内部 mmap（`bmrt`/`bmcv`/ffmpeg 上下文）；
  生产同样有 **135 个 dma-buf fd**，但**不做进程 mmap（0 映射）**。
  另测：`bm_image_alloc_contiguous_mem_heap_mask` 的 1/2/3/4/6 号 heap 全部各产生 1 个
  dma-buf fd（8/16/32 号失败）；`bm_malloc_device_byte`、`bm_image_alloc_dev_mem` 也各产生 1 个
  dma-buf fd → dma-buf fd 是 bmlib/bmcv 分配路径固有的；要 0 映射需消除库内 mmap，暂未定位到开关。
- **ISC 偶发抖动**：与“解码器重建风暴 → 同时重新 resolve previewURL”同源；常驻化后
  34/36 稳定，乙烯南路2 偶发掉线 **1 分钟内自愈**（ISC 会话侧），凤凰咀两路 fresh 两协议均失败（外部）。
- **抖动收敛（代码）**：读失败触发 gateway 重启的阈值从「5 次错误（<100ms，瞬时丢包就重建
  ISC 会话）」改为「**2 s 无新帧 或 50 次连续错误**」，与生产“只在流真正死掉时重连”一致。
- **生产重启后帧率复测（10:40）**：合流墙 **10.0 fps**（15s 150 个 WS 帧，~70KB/帧）；
  单通道 25K **5.0 fps**（~50-60KB/帧）、14K **4.13 fps**（~90KB/帧）。
  我方同一时段：合流 `all_1` **3.5~5.2 fps**（500~974KB/帧，随负载波动）、25K
  **3.85~4.88 fps**（120~214KB/帧）、6488 4K **1.6 fps**（~1975KB/帧，ADR-097 按源尺寸）。
- **待跟进**：合流墙 5.2 vs 生产 10 fps（生产硬件 MergeInstance）；4K 单通道 1.6 fps
  （全尺寸 VPP+JPU 在 34 路解码压力下）。
- **最终 30 分钟 soak（11:35~12:06，30 次采样）**：**34/36** 为主；偶发单路掉线
  （预制区/第三排/通道/乙烯南路2/大门，1~2 分钟内自愈）但**从未出现 VPU 楔死或 decoder
  重建风暴**；fd **890~919 稳定**、dma-buf fd **610~632 稳定**、`bm_live_mb` 636~999、
  `vpu_1/2≈100/100`、VPU links 16~19/14~18 均衡、`restarts=0`、无 FATAL/vdi 错误。

### 迭代 104 — 2026-11-19 — 优雅停机（PID1 SIGTERM）+ 告警门控持久化 + 时延指标；生产告警去重证据

- **重启/停机卡死的根因**：进程是容器 PID 1，Linux 对 PID 1 的默认信号处置是“忽略”，
  我们没装 SIGTERM handler → `docker stop` 等满超时后 SIGKILL；这次 SIGKILL 又把容器留在
  `Up` 但 PID1 `<defunct>` 的状态，`docker restart` 直接 hang（与迭代 103 的僵尸容器同源，
  多次重启只是碰巧成功）。
- **修复**：`main.cpp` 注册 `SIGTERM/SIGINT -> _exit(0)`（async-signal-safe），让容器干净退出。
  **实测**：`docker stop -t 5 aidetect` 返回 **1447 ms、exit=0**（修复前 6871 ms、exit=137/SIGKILL），
  随后 `docker start` 正常、`restarts=0`。
- **告警上报门控持久化（对齐生产 `table_alarm_interval`）**：生产
  `libEdgeSystem.so` 有 `AlarmRecorder::getLastALarmTime/setLastALarmTime`、
  `AlarmIntervalFilter::isAlarmFiltered`，DB 有 `table_alarm_interval(id, time_ms)`
  ——重启后不会把已上报过的目标立即再报一次。我方原先只有内存 `rt.next_report_ms`，
  重启首个目标必放行。
  新增 `src/alarm_state.hpp`：JSON 落盘到
  `<data_root>/.state/alarm_intervals.json`（key = taskId|model，写入节流 5s，主循环 30s flush），
  任务启动时按持久化的 `last_ms + gap` 恢复 `next_report_ms`；gate open 时写回。
  **实测**：文件 854B、16 条记录；两次重启日志均为
  `alarm interval state loaded: 16 entries`。
- **时延指标（补 MATRIX §5 的“待补”）**：`/task/list` 每任务新增
  `firstFrameMs`（任务启动→首帧）与 `alarmLatencyMs`（帧→告警生成）。
  **实测**：`firstFrameMs` **1.3~5.2 s**（36 路按 1.2s 错峰启动）、
  `alarmLatencyMs` **6~45 ms**（25K 6ms、钢栈桥 21ms、28K/排水渠 44ms）。
- **生产告警去重语义补证**：符号 `AlarmIntervalFilter/isSameRectAlarm/FeatureAntiRepeat` +
  DB `table_alarm_interval`；我方用每目标 `TargetState`（IoU/中心距）+ 任务 `cooldown`
  + 算法 `repeat_sec` 实现等价效果，详见 `docs/PRODUCTION_ALARM_LOGIC.md §7`。
  未反编译到 `isSameRectAlarm` 的 IoU 常量，按行为验收（同一目标按间隔、不同目标独立计时）。
- **验证命令**：`docker stop -t 5` 计时、`docker inspect .State.ExitCode`、
  `cat <data_root>/.state/alarm_intervals.json`、`/task/list` 的
  `firstFrameMs/alarmLatencyMs`、`grep "alarm interval state loaded"`。
- **3 路 ISC 5XX 的外部性复核（01:10，fresh URL）**：龙港-预制场 fresh RTSP 仍
  `5XX`（0.5s）；凤凰咀-游客中心/博物馆 fresh RTSP 分别 `5XX`（11.5s）与
  `Invalid data found`（21.7s），fresh RTMP 两路均 `Input/output error`（~18s）。
  两协议都失败 → ISC/相机侧，不是我们的重连/协议逻辑。
- **告警图颜色修复**：`update_alarm()` 里 `d.confirmed/d.pending` 是在 gate-open 之后才赋值，
  而 `save_alarm()` 在 gate 内调用，导致告警图用旧的 pending 状态画成**蓝框且无标签**。
  现在 save 前把当前检测标为 confirmed/非 pending/不抑制标签；实测钢栈桥 `vest` 告警图
  为**红框 + 红色「未穿反光衣」标签**（见 `.work/dev/falarm2.jpg`）。
- **门控持久化复验**：01:19 重启日志 `alarm interval state loaded: **26 entries**`（文件随告警增长）。
- **最终构建 10 分钟 soak（01:22~01:32，20 次采样）**：dmabuf 对象 **1304~1372 稳定**、
  `bm_live_mb` **565~806 稳定**、`hw_jpeg_ok 6→25 / fail=0`、`vpu_1/2 86~100`、
  RSS 974MB→1.0GB→trim 后 **758~775MB**、fd **848~888**、**32~33/36 ok**、
  `restarts=0`、`vdi/AllocateDecFrameBuffer` 错误 **0**；CPU 采样 179~656%
  （峰值来自告警转码/JPEG 突发）。
- **视觉验收（最终构建）**：`all_2` 合流墙 1920×1080 九宫格（7 路实时 + 2 路状态瓦片，
  ROI 绿线正常）；钢栈桥 `vest` 告警图 **红框 + 红色「未穿反光衣」标签**；
  25K 单通道 704×576 预览 92~94KB、ROI 正常。
- **预览分辨率/帧率对齐生产（ADR-097，取代 ADR-023/ADR-088）**：默认
  `AIDETECT_PREVIEW_INTERVAL_MS=200`（5fps），`preview_target` 默认**按源尺寸**（不再 1080p 上限）。
  实测：6488 单通道 **2560×1440 / 566,552B / 3.81 fps**、25K **704×576 / 101,872B / 5.04 fps**，
  `/get_occupancy` `hw_jpeg_ok=114/fail=0`、`vpu_1/2=100`、cgroup 30s 均值 **193%**。
  ADR-097 已写入 DECISIONS（历史 ADR 不改，新增条目标注取代关系）。

### 迭代 103 — 2026-11-18 — Web 预览全灭根因：僵尸容器 + stop_gateway 并发 join；设备内存 churn 收敛；生产对齐第一批（设备 JPEG/告警视频/遥测）

- **现象**：23:00 左右 Web 端所有预览消失，`docker ps` 仍显示 `aidetect Up`，但 `docker exec` 报
  `cannot exec a container that has stopped`、HTTP 9000 无响应。根因不是预览编码，而是**服务进程已崩**。
- **崩溃根因（23:22 的 FATAL）**：backtrace = `Task::stop_gateway()` → `std::thread::join()`，
  调用者是 `Task::start()` 里的 status/watchdog 线程 lambda。
  `stop_gateway()` 会 `seg_rec_.stop()` / `bridge_.stop()`（都会 `join()` 自己的线程）；任务线程
  （开流/重连）与 watchdog 线程（45s 无帧 / 60s 无首帧 / 切子码流）会**并发调用**，两次 join 同一
  `std::thread` → `std::system_error` 在 watchdog 线程里未捕获 → `std::terminate` → SIGABRT。
  崩溃后 PID1 变 `<defunct>`（Z），正是 KI-042 记录的僵尸容器：`docker restart -t 5`、`docker rm -f`
  全部 hang，只能重启盒子（23:30/23:34 两次），且重启后容器是 `Dead`，需 `docker rm -f` 重建。
- **修复**：
  1. `Task` 增加 `std::recursive_mutex gateway_mu_`，`start_gateway()/stop_gateway()` 全程持锁
     （`start_gateway` 内部会调 `stop_gateway`，故用递归锁）。
  2. `IngestBridge`、`TsSegmentRecorder`、`AlarmRingRecorder` 的 start/stop 增加 `life_mu_`，
     `join` 前判断 `get_id()==this_thread::get_id()`（自身线程调用则 `detach`），并把
     `stop_locked()` 供 start 内部复用，杜绝并发/自 join。
- **设备内存 churn（vdi_allocate_dma_memory fail 的来源）**：
  - 实测确认这些 dma-buf 全是我们的：停止容器后 `/sys/kernel/debug/dma_buf/bufinfo` 对象数
    **1453 → 2**；旧二进制启动 55s 就 996 个 / 29GB（虚拟），13 分钟 1487 个 / 54GB，并出现
    `vdi_allocate_dma_memory size=5529600 fail`、`AllocateDecFrameBuffer fail`、
    `bmvpu_dec_decode: the frame buffer count set is wrong`。
  - 修复：`Decoder::open_locked()` 所有 early-return 统一 `release_locked()`（失败也关 fmt_）；
    `frame_to_image()` 抛异常时先 `av_frame_unref()` 再抛（否则该帧 VPU 缓冲一直被 AVFrame 持住，
    内存越紧越漏）；`release_locked()` 重置 `nv12_`（避免 stride 变化后旧 attach 复用）；
    两条重建路径都先 `decoder_.reset()/decoder_.reset()` 再 `make_shared`（消除瞬时双份）；
    `extra_frame_buffer_num=1`（默认 2，减半每路帧缓冲）；解码器开流全局 200ms 节流。
  - 修复后 **10 分钟 soak（20 次采样，30s 间隔）**：dmabuf 对象 **1288~1342 稳定**、
    `bm_live_mb` **559~726 稳定**（alloc/free 每分钟各约 1700 次但 live 不涨）、
    RSS 1.01GB → trim 后 731MB → 813MB（无单调增长）、fd **818~849**、
    `hw_jpeg_ok` 28→73（`hw_jpeg_fail=0`）、`vpu_1/2=77~91/99~100`、
    `restarts=0`、`vdi_allocate_dma_memory/AllocateDecFrameBuffer` 错误 **0 次**、
    **30~32/36 ok**（3 路 ISC 5XX + 偶发抖动）。CPU cgroup 30s 均值 **163~358%**（约 2.2 核）。
- **生产对齐第一批（本迭代落地）**：
  1. **设备侧 JPEG**：`Bmcv::imencode` 改为「BGR→VPP NV12→`bmcv_image_jpeg_enc`」，失败回退
     CPU OpenCV；质量 `AIDETECT_JPEG_QUALITY`。独立验证 0.5.1 的 API 可用：
     1280×720 编码 `enc rc=0 size=146584 SOI=ffd8`（`free()` 释放安全）。
     线上 `/get_occupancy` 已有 **hw_jpeg_ok=17 / hw_jpeg_fail=0**（部署后 2 分钟）。
  2. **预览 cap 误放大修复**：`AIDETECT_PREVIEW_WIDTH/HEIGHT` 以前无条件返回，704×576 通道会被
     放大到 1920×1080；改为按比例**只缩不放**。
  3. **告警视频对齐生产 dump4 语义**：`pre = round(video_duration×0.5)`、`post = duration-pre`
     （生产 `VideoRecordSec=10` + `VideoRecordPrefix=0.5`，旧代码硬编码 pre=5）；`-fs
     AIDETECT_VIDEO_LIMIT_MB`（默认 **20MB**，对齐 `VideoRecordLimitMB`）；transcode/remux 加
     `timeout 60`；转码全局 `try_lock`（忙则保留 HEVC，避免转码风暴抢 VPU）；启动时清理 >10 分钟的
     `.h264.tmp.mp4/.limited.mp4/.ts.keep`（本次启动清掉 13+71 个残留）。
     实测新告警：**H.264 / 1280×720 / 8.98s / 1.13MB**。
  4. **遥测**：`/get_occupancy` 新增 `jpu`、真实 `vpu_1/2`（解析 `/proc/vpuinfo`）、
     `bm_live_mb`、`bm_alloc_n/free_n`、`hw_jpeg_ok/fail`；实测 `vpu_1=84~100, vpu_2=97~100`。
     `tpu` 仍为 0：本内核无 userspace TPU 利用率接口（不造数）。
  5. **子码流改显式开关**：`/tmp/aidetect_auto_substream` 需配合 `AIDETECT_AUTO_SUBSTREAM=1`
     才生效，默认按通道 URL 的 `streamType`（对齐生产配置驱动），去掉一个跨线程
     `stop_gateway()` 触发点。
- **仍存在**：3 路 ISC `5XX`（龙港-预制场/凤凰咀游客中心/凤凰咀博物馆）是 ISC/相机侧；
  `docker stats` 瞬时 CPU 171~581%（告警转码/JPEG 突发），cgroup 30s 均值 ~240%（33 路解码，
  VPU 已 100%）；`memory` 字段仍 0（未找到 bm heap 统计 API）。
- **验证命令**：`docker inspect`、`/get_occupancy`、`/task/list`、`wsgrab.py`、`ffprobe`、
  `find ... | wc -l`、`/sys/kernel/debug/dma_buf/bufinfo` 解析。

### 迭代 102 — 2026-11-18 — 整镜像切到 libsophon 0.5.1(SoC)+mw0.14 成功；ISC 会话缓存失效与协议钉死

- **上一轮 0.5.1 混挂失败的真因（已定位并修正）**：用的是
  `reference/release/SDK-23.09-LTS-SP5/libsophon_20250825_113547/libsophon_0.5.1-LTS_aarch64.tar.gz`
  ——这是**通用/PCIe 版**，其 `libbmjpulite` 打开 `/dev/bm-sophon`；BM1684X 只暴露 `/dev/jpu`，
  于是 `jdi_init(): Can't open jpu driver ret=0xffffffff` → Sophon OpenCV 0.14 的 JPEG 编码器加载
  失败（`Error! enc load failed`），`cv::imencode` 抛 `(-215:Assertion failed) code` → SIGABRT。
  正确包是同发布里的 **`sophon-img_20250825_162013/libsophon_soc_0.5.1-LTS_aarch64.tar.gz`**
  （其 JPU 库字符串为 `/dev/jpu`、`/dev/bm-vpp`）。隔离验证：`jpegtest014`（直接 `cv::imencode`
  纯色 1280×720）在通用版下报 `Can't open jpu driver`，换 SoC 版后
  `Open /dev/jpu successfully ... imencode ok=1 size=15016`。
- **另外两个运行时坑**：
  1. OpenCV 0.14 `libopencv_core.so` 的 `DT_NEEDED` 含 `libopenblas.so.0`（SoC 包内已带；
     通用包没有），缺失时 opencv 加载不了。
  2. **bmrt 0.5.1 不能用只读 bind 挂载加载 bmodel**：`/models:ro` 下
     `[BMODEL][ModelCtx:390] FATAL: File[...] open failed`；同一文件 `cp /tmp` 或 `:rw` 挂载即可
     （`model_probe051 /models/.../people.bmodel` 失败、`/tmp/p.bmodel` 成功）。故容器 `/models`
     改 `:rw`（应用只读，不写模型）。
- **整镜像替换（不是混挂）**：`Dockerfile` = `FROM aidetect:current` + SoC libsophon 0.5.1 +
  `sophon-mw-soc_0.14.0`，并把 `/opt/sophon/{libsophon-current,sophon-ffmpeg-latest,sophon-opencv-latest}`
  三个指针全部改指 0.5.1/0.14（否则 dlopen 仍会取到 0.5.3/0.15 造成混 ABI），`ENV LD_LIBRARY_PATH`
  指向新三套 lib。构建出 `aidetect:routeb051d`（前几版 b/c 仅用于排障，保留不删）。
  二进制用 SoC 0.5.1 头文件 + mw0.14 重新编译（`env/env.sh` 同步钉到 SoC 0.5.1/0.14）。
- **切换过程**：保存旧容器 inspect 与二进制（`bin/aidetect.pre051_20261118`）→ 停旧容器、
  `docker rename aidetect aidetect_prev051`（`--restart=no` 留作回滚）→ 用 routeb051d 以**相同
  env/binds**（`--network host --privileged --security-opt label=disable`，models 改 rw，
  其余 env 与旧容器逐项一致）重建 `aidetect`。
  中途排障重启过一次开发盒（16:53）；`docker create` 的容器不随重启自启，重启后手动 `docker start`。
- **切换后实测（17:01~17:05，36 路）**：`docker inspect` restarts=0、日志无 FATAL/abort；
  `/task/list` 36 任务 **30 ok**；25K 预览 704×576 彩色 + ROI（125,344B）、
  龙港钢栈桥 1280×720 彩色（215,528B）、6488 2560×1440（298,184B）；
  最新告警视频 `ffprobe` = **9.68s**（pre5+post5）、621KB；
  `docker stats` CPU **162.7%**（迭代 101 的 0.5.3 为 191~197%）、RSS 1.09~1.12GB、
  threads 277~280、`Open /dev/jpu successfully`。
  注意告警视频编码跟随源（25K 源为 HEVC 704×576，录制器只转封装不转码），与迭代 101
  记录的「H.264/1280×720」不冲突（那条来自 H.264 源通道）。
- **ISC 对齐（17:12 部署，17:15 复测）**：
  1. `AIDETECT_ISC_PROTOCOL` 改为**硬钉**：显式设置时压过任务候选协议。原因：本机 1935 可达，
     但 ISC 的按相机 RTMP 转码会话池很小，fresh RTMP URL 用 `ffmpeg -t 1` 探测 18s 未出流；
     同一相机 fresh RTSP URL **0.5~2.1s** 出流（乙烯南路2 HEVC 2560×1440@25、25K HEVC 704×576@25）。
     不钉死会让每次重连先在 RTMP 上卡十几秒，表现就是一直 `connecting`。
  2. `IscResolver` 增加 `invalidate(isc_url)`：ISC 开到流失败/解码器打开失败/读失败重连时清掉该
     相机的 previewURL 缓存，下次重新向 Artemis 取会话——**生产就是每次重连重新取 previewURL**。
     实证必要：乙烯南路2 fresh RTSP URL 可出流，但任务用缓存里的旧 URL 一直 `Input/output error`。
  3. **部署后复测**：36 任务 **33 ok**（部署前 30 ok）；乙烯南路2、排水渠1、测试_18 恢复出图
     （排水渠1 已正常产生 `vest` 告警视频）；25K 704×576（122,240B）、龙港钢栈桥 1280×720
     （204,536B）预览正常；restarts=0、无 FATAL。
     剩余 3 路 `5XX`（龙港-预制场、凤凰咀-游客中心、凤凰咀-博物馆）——龙港-预制场即使
     **新解析**的 RTSP URL 也 `Server returned 5XX`（0.5s 即返），属 ISC/相机侧。
     该结论用 fresh-URL 探测脚本固定（`.work/dev/_iscprobe.sh`/`_iscprobe2.sh`：HMAC 直连 Artemis，
     再 `docker exec aidetect ffmpeg -rtsp_transport tcp -i URL -t 1 -f null -`）。
- **生产 dump4 参数实证（只读 DB，未改生产任何文件）**：`table_preference` 里
  `VideoRecordSec=10`（5~30，默认 10）、`VideoRecordPrefix=0.500000`（回溯比例，10s×0.5=前 5s/后 5s）、
  `VideoRecordLimitMB=20`、`EnableVideoRecord=1`、`Mp4RecorderEngine=0`；
  `libEdgeSystem.so` 符号含 `libdump4::create_dump4_instance`、`MediaChannel::recordVideo`、
  `TaskInstance::applyMp4Record`、`libdump4::dump_result`。→ 我方 `TsSegmentRecorder` 的
  **pre5+post5、实测 9.68s** 与生产 dump4 的「总 10s、前后各半」一致；实现上仍用进程内 TS 环
  （生产 libdump4 无公开头文件/ABI，直接绑定风险高）。
- **rea 现状**：本机 `rea analyze` 对 aarch64 ELF 无可用 provider（ghidra P0 只收 PE/6502、
  hopper 不支持 win32、ida/kuna 未配置，证据 `.work/rea_analyze_edge.json`）；生产侧逆向沿用
  已提取的 `prod_edge_strings.txt` / `prod_syms_all.txt` / `edge_syms.txt` + 只读 sqlite。
- **切换后稳定性采样（17:20~17:35）**：`restarts=0`、无 FATAL；RSS 1.19GB → **889~900MB**
  （`malloc_trim` 周期回收，非单调增长）、fd 847~875、threads 271~272；
  CPU 用 cgroup `cpuacct.usage` 60s 均值 = **2.26 核（226%）**（`docker stats` 瞬时快照在
  170~577% 间跳，采样噪声大；迭代 101 的 0.5.3 为 191~197%，但当时 30 路有图、现在 33 路，
  且告警密度高：15 分钟 85 个 mp4/108 张 jpg）。
  频道健康日志 `/data/CT/routeb_dev/health_1720.log`（60s 一条）显示 **31~33/36 ok** 波动：
  固定 3 路 5XX，另有 14K/28K/钢栈桥偶发掉线后 **1 分钟内自愈**（ISC 会话抖动，非永久黑屏）；
  钢栈桥同期仍在正常检测并产生 `vest` 告警视频（674KB / 1.21MB）。
  剩余优化项：告警密度高时 CPU 峰值明显，后续可加每算法告警节流/JPEG 设备侧编码。
- **回滚**：`docker rm -f aidetect && docker rename aidetect_prev051 aidetect`，并把
  `bin/aidetect.pre051_20261118` 改名回 `bin/aidetect` 后 `docker start aidetect`（旧镜像
  `aidetect:current` 未动）。备份：`bin/container-aidetect-20261118.json`、`bin/aidetect.pre_routeb_20261118`。
- **验证命令**：`docker inspect aidetect --format '{{.State.RestartCount}}'`、`/task/list` 计数、
  `python3 /data/CT/routeb_dev/wsgrab.py <taskId> <out> <jwt>`、`ffprobe -show_entries format=duration`。
### 迭代 101 — 2026-11-18 — 预览/告警配色/告警视频窗口修复；SDK 0.5.1 同步（运行时待重建镜像）

- **预览空白（704x576 等 ≤1080p 通道）**：`image_copy_to` 用 `bmcv_image_copy_to`，该接口只支持 packed；
  planar 输入时静默失败 → `maybe_preview` 提前返回，单通道/合流瓦片只剩状态卡。改为 planar+同尺寸时走
  `bmcv_image_vpp_convert`，并给单通道预览加 CPU `render_jpeg` 兜底。25K 预览恢复（704x576 彩色）。
- **任务编号/标签乱码**：`mat_to_bm_image` 把 CPU Mat 一次性线性拷进带 stride 的设备图，行错位 → 彩虹噪点。
  改为按行 `bm_memcpy_s2d_partial_offset`。实测右上角「安全帽检测」清晰。
- **黑白画面**：告警图走 `convert_format(BGR_PLANAR→BGR_PLANAR)`，VPP 把它当 YUV 转 → 掉色；
  预览走 planar→packed 所以一直彩色。`to_mat` 改为统一走 packed 读回，25K 告警图恢复彩色。
  （合流墙上仍呈灰度的相机确为黑白源。）
- **告警视频窗口**：生产是告警帧前后各 ~5s。原来 `seg_rec_.clip(5, post)` 只取「过去 5s」且忽略 post。
  改为告警时只登记窗口、等 post 秒后在 finalize 里拼「pre+post」；分段从 4s 改 2s。
  实测 `ffprobe` = H.264 / 1280×720 / **10.83s**（原来 7.02s）。
- **崩溃（资源/生命周期）**：`~Task` 释放池化 BMImage 时，分配它们的局部 `sail::Handle` 已 `bm_dev_free`，
  在 libbmlib `bm_profile_record_mem_begin` 崩溃。改为**进程级共享 bm_handle**（永不释放）。
  修复后连续运行 >17 分钟无崩溃（此前 2~4 分钟必崩）。
- **依赖库同步（进行中）**：从 `reference/release/SDK-23.09-LTS-SP5` 取出 `libsophon_0.5.1-LTS_aarch64`
  与 `sophon-mw-soc_0.14.0_aarch64`（生产是 0.5.1 + mw 0.10.0，0.10.0 不在官方包里，0.14.0 是同一 LTS 线最近版本），
  已切到 0.5.1/0.14.0 头文件并通过编译（仅需把 `bmrt_get_network_name` 换成 `bmrt_get_network_names`）。
  **运行时混用失败**：把 0.14 库放 `/opt/bin/sdk` 用 DT_RPATH 覆盖容器 `LD_LIBRARY_PATH` 后，
  与容器内 0.15 的 OpenCV/FreeType 混用导致启动即 abort（SIGABRT，崩溃循环）。
  已回退（把 `/opt/bin/sdk` 改名禁用），服务恢复。**下一步：整镜像替换 `/opt/sophon-sdk` 为 0.5.1+0.14，而不是混挂库**。
- **25K / 龙港钢栈桥 复验（15:39）**：两路均在出 `detect round`；龙港单通道预览 1280×720 **彩色**画面正常
  （此前「没画面」是 ≤1080p 通道 planar 拷贝失败导致预览不排队，不是 ISC 取流问题）。
  服务连续运行 3 分 32 秒 restarts=0、crashes=0、CPU 151%、RSS 944MB。
- **SDK 镜像已构建（待切换）**：用 `reference/release/SDK-23.09-LTS-SP5` 的 libsophon 0.5.1 + sophon-mw 0.14.0
  构建了镜像 **`aidetect:routeb051`**（`/opt/sophon-sdk` 内同时保留 0.5.1/0.14.0 两套目录，`LD_LIBRARY_PATH` 指向新版本），
  旧的 `aidetect:current` 未动、运行中的容器未重启。切换容器需维护窗口（重建容器偶发楔死要重启盒子），下轮执行。
- **SDK 回退（本轮结论）**：混挂 0.5.1/0.14 库会让进程 abort（曾崩溃循环 restarts=10）。已把 `env/env.sh`
  恢复 0.5.3/0.15.0、`/opt/bin/sdk` 改名停用；重建部署后 36 路运行 3 分钟 restarts=0、CPU 142%、RSS 943MB。
  真正对齐生产需**重建容器镜像**（把 `/opt/sophon-sdk` 整体换成 0.5.1+0.14），不能只靠 RPATH 混挂。
- **ISC 25K/龙港**：25K 已恢复画面（预览修复）；龙港钢栈桥仍间歇 `ingest bridge no video: Input/output error`，
  与生产的 ISC 重连/会话复用逻辑差异待逆向对齐（rea）。

### 迭代 100 — 2026-11-18 — 预览/合流空白与 ROI 线修复：packed stride 与设备侧画线

- **现象（用户反馈）**：合流墙与单通道预览没有画面；部分画面黑白；配置区域线（ROI）没画对。
- **根因 1（预览/合流空白）**：兼容层 `convert_format(..., FORMAT_BGR_PACKED)` 按「1 字节/像素」算 stride（`w`），
  而 packed BGR 每行是 `w*3` —— 设备侧写入越界、图像错乱，单通道/合流瓦片渲染返回空，
  前端只剩「监控视频流状态」占位卡；告警图（planar）一直正常，所以只有预览/合流坏。
  修复：新增 `format_bpp()`（packed BGR/RGB = 3），`convert_format`/`crop_and_resize*`/`image_copy_to` 统一使用。
- **根因 2（ROI 线不对）**：兼容层 `polylines` 用矩形近似，只画水平/垂直线段，斜边被跳过；
  `bmcv_image_draw_point` 在 packed 图上也不生效。改为 CPU `cv::polylines`（与告警图同一路径）
  + 按行 `bm_memcpy_s2d_partial_offset` 写回设备图，支持任意多边形/斜边/线宽。
- **黑白画面**：用已知彩色相机（数智技术中心_第二排 1280×720）验证预览为**彩色**，
  NV12→BGR 转换正确；合流墙上呈灰度的相机（淮河25K/28K、乙烯南路等）源本身是黑白/低照度相机。
- **验证**：单通道预览 1280×720 彩色 + 绿色 ROI 多边形（含斜边）正确；合流墙 `all_1` 九宫格出图；
  `all_3` 8 秒 42 帧（5.15 fps，平均 750KB/帧）；单通道 4.65 fps。
- **顺带**：`get_tpu_util/get_vpu_util` 改为读 `/proc/jpuinfo`（之前恒 0）。
- **新增**：`docs/PRODUCTION_MATRIX.md` —— 与生产盒子的逻辑/参数/资源/输出四维对比 + 待补测参数清单。


### 迭代 99 — 2026-11-18 — 路线 B 落地：删掉 sail，vm 二进制只链 bmcv/bmrt/sophon-ffmpeg

- **交付**：新增 `src/routeb/compat.hpp`（约 1000 行），在 `namespace sail` 里用
  `bm_image_*`/`bmcv_*`/`bmrt_*`/sophon-ffmpeg 实现现有代码用到的
  `Handle`/`BMImage`/`Bmcv`/`PaddingAtrr`/`Tensor`/`Engine`/`Decoder`/`MultiDecoder`，
  6 个源文件只改 `#include`，121 处调用点**未改**；`CMakeLists.txt` 去掉 `sail` 链接。
  `ldd build/aidetect/aidetect | grep sail` 为空（实测无 `libsail.so`）。
- **集成期修掉的 4 个真 bug**（都是 sail 语义差异导致的）：
  1. `bm_image_create` 的 `stride` 参数是**每平面一个**，三平面 BGR 传单值指针会让
     VPU 读到垃圾 stride → 驱动 ASSERT（`W_stride=640, W=1920`）。改为按平面数组。
  2. 新建的 `bm_image` 没 `bm_image_alloc_contiguous_mem_heap_mask` 就送 vpp →
     输出异常；统一走 `make_image()`（create + allocate）。
  3. `bm_memcpy_d2s` 按**整块连续内存**大小拷贝，而宿主机缓冲按单平面分配 →
     堆溢出（`malloc(): invalid size (unsorted)`，崩在 `render_jpeg`）。
     改为 `bm_memcpy_d2s_partial` + `bm_mem_get_device_size` 取 min。
  4. `crop_and_resize*` 对 packed 输入要产出 planar（sail 行为），否则下游
     `convert_to` 的通道/图式不匹配。
- **又一个崩溃（read 里的 pthread_mutex_lock）**：`run_single` 里 `decoder_holder`
  是函数局部 `unique_ptr`、`decoder_` 是成员裸指针，`release()/reset()` 与
  `decoder_->read()` 之间存在悬垂窗口（ISC 重连风暴时高频触发，约 2~4 分钟一次）。
  已把 `decoder_holder`/`decoder_` 都改成 `std::shared_ptr`，重启后 3 分钟无崩溃。
- **部署**：`cp` 会 `Text file busy`，必须 `cp 到 .new` + `mv -f`（rename）再
  `sudo kill -9`（进程是 root，linaro 直接 kill 无效）。两次崩溃都让容器 PID1 变僵尸
  （`docker ps` 仍 Up、进程 Z 状态、`kill -9` 无效），只能重启盒子恢复（各约 4 分钟）。
- **验证（重启后连续 22 分钟，36 路）**：`restarts=0`、崩溃 **0**（此前 2~4 分钟必崩）、
  状态 S、线程 275；CPU **186~195%**（旧实现 266~294%）；RSS 638~919MB（`malloc_trim` 周期，
  稳态 ~700MB）；**dmabuf 恒 290**（旧 500~660 且缓升）；3 分钟窗口 **33 个任务**在出检测；
  分辨率分布含 2560×1440/3840×2160/4096×2160/704×576（HEVC 与 ISC 通道都在工作）；
  近 30 分钟告警图片 **145** 张、告警 MP4 **138** 个。
- **功能抽检**：`ffprobe` 告警视频 = H.264 / 1280×720 / 7.02s / 898KB；
  告警图 1280×720/2560×1440 均有框+中文标签+ROI（抽检 淮河14K、乙烯南路）；
  合流墙 `all_3` 1920×1080 九宫格、`all_1` 经 WS 抓帧可见实时画面与状态瓦片。
- **回滚**：旧二进制保留在 `/data/CT/project/ai/bin/aidetect.pre_routeb_20261118`；
  本地分支 `route-b-bmrt`（不推送）。


### 迭代 98 — 2026-11-18 — 路线 B PoC 跑通：VPU 硬解 CPU 降 10.9 倍，全链路无 sail

- **交付**：`tools/routeb_poc/`（`decode_probe.cpp`、`poc.cpp`、`README.md`），
  **不链接 `libsail.so`**，只用 `avcodec h264_bm/hevc_bm` + `bmcv_*` + `bmrt_*`。
  链路：VPU 硬解 → bmcv 缩放/归一化 → bmrt 推理 → bmcv 画框 → bmcv JPEG。
- **实测（单路 2560×1440 H.264，源 15fps，盒子空闲时段）**：

  | 路径 | 帧率 | CPU/帧 |
  |---|---|---|
  | 现有 sail 用的软件解码 `h264` | 27.9 fps | **35.59 ms** |
  | VPU 硬解 `h264_bm` | 27.5 fps | **3.25 ms** |
  | 全链路 PoC（预处理+推理+画框+JPEG） | 14.7 fps | 13.24 ms |

  → **解码 CPU 降 10.9 倍**（36 路 1440p 从约 1.28 核/路降到 0.12 核/路），
  推理 `bmrt_launch_tensor_ex` **0.13 ms/帧**；120 帧 `fd 6→33`、`dmabuf maps 0`、
  RSS 8.7→36.3 MB。
- **佐证**：PoC 用相机 `数智技术中心_第二排` 直连 RTSP，输出
  `1280×720/322416B` JPEG（画面 OSD 时间与墙钟一致），说明解码/缩放/编码都正确。
- **摸底结论（决定后续改法）**：
  1. `h264_bm` 输出 `AV_PIX_FMT_NV12`，设备地址在 `frame->data[4]/[5]`、
     stride 在 `linesize[4]/[5]`；可直接 `bm_image_attach` 给 bmcv，**无需 mmap/拷贝**。
  2. 模型输入是 fp32 时用 `bmcv_image_convert_to(alpha=1/255)`；
     `bm_image_get_device_mem` + `bmrt_tensor_with_device` 把图像设备内存直接当输入张量。
  3. `bmcv_image_jpeg_enc` **只接受 NV12**（BGR_PACKED/PLANAR 都报不支持），
     与 sail `imencode`（`bm_image_to_mat`+`cv::imencode`，CPU 路径）不同，
     集成时预览/告警图走设备侧 NV12 编码。
- **验证命令**（盒子上）：
  ```bash
  cd /data/CT/project/ai/gh-aibox-dev-src/gh-aibox-dev && source env/env.sh
  g++ $CF tools/routeb_poc/decode_probe.cpp -o /tmp/decode_probe -lavformat -lavcodec -lavutil
  g++ $CF tools/routeb_poc/poc.cpp -o /tmp/routeb_poc -lavformat -lavcodec -lavutil -lbmcv -lbmrt -lbmlib
  /tmp/decode_probe "$U" h264_bm 200   # cpu_per_frame=3.25ms
  /tmp/decode_probe "$U" h264    200   # cpu_per_frame=35.59ms
  /tmp/routeb_poc  "$U" "$M" 120 /tmp/poc_out.jpg 0.25
  ```
- **下一步**：按 PoC 结论把 `src/model.hpp`（Engine→bmrt）与 `src/task.hpp`
  （Bmcv→bmcv、Decoder→h264_bm/hevc_bm）分两步替换，先 1~2 路接入主程序再铺开。


### 迭代 97 — 2026-11-18 — HEVC bsf 改由 codec_id 判定：排水渠1/乙烯南路2 恢复（2560×1440）

- **根因**：纯 bridge 模式下 `upstream_hevc_` 不再由 ffmpeg 日志判定（没有 ffmpeg 了），
  于是 `use_bsf_` 恒为 false；HEVC 源（排水渠1 st1、乙烯南路2）封装成 TS 后解码器解不出帧，
  表现为 “bridge 已起、UDP 有数据，但 30s 无首帧”。
- **修复**：`IngestBridge` 直接用 `vin->codecpar->codec_id == AV_CODEC_ID_HEVC` 决定启用
  `hevc_mp4toannexb`，不再依赖调用方的猜测。
- **效果**：
  - **排水渠1：connecting → ok 2560×1440**（截图确认有画面）；
  - **乙烯南路2：connecting → ok 2560×1440**；
  - 36 路回到 **32 ok / 4 connecting**。
- **剩余 4 路全部确认为 ISC 侧问题**：
  - 预制场、25K：本机用 ISC 账号直接探测（st0/st1 都试），**ISC 返回无视频**；
  - 凤凰咀-游客中心、博物馆：**生产盒子实测同样 0fps**。
- **状态**：32 ok / 4 connecting、RSS 818MB（预热中）、dmabuf 497、合流墙 8fps。

### 迭代 96 — 2026-11-18 — ISC 残余 6 路定位：排水渠1 可直连，但 bridge 起始后解码器拿不到帧

- **直接探测（本机 ffmpeg + ISC 账号）**：
  | 相机 | st0 | st1 |
  |---|---|---|
  | 预制场 | 无视频 | 无视频 |
  | 25K | 无视频 | 无视频 |
  | 排水渠1 | **2560×1440 HEVC ✓** | **640×360 HEVC ✓** |
  → 预制场/25K 是 **ISC 侧确实拿不到流**（相机离线/ISC 未注册），与生产有画面不一致的部分需 ISC 侧排查。
- **排水渠1 的情况不一样**：本机能直连拉流，但任务日志显示
  `rtsp streamType=1 → ingest bridge pid=none`（bridge 已起、UDP 有数据）之后
  `frame watchdog: no first frame for 30s` —— **bridge 送出了 TS，但解码器解不出帧**。
  这与“HEVC 源 + 我们 bridge 的封装置换”有关（`upstream_hevc_` 在纯 bridge 模式不再由 ffmpeg 日志判定，
  bsf 判据失效），是下一轮要修的点：**bridge 直接用 `codecpar->codec_id` 判定 HEVC 并启用 annexb bsf**。
- **状态**：36 路 30 ok / 6 connecting（全部 ISC）、CPU 266%、RSS 578MB、dmabuf 535、合流墙 8.0fps。

### 迭代 95 — 2026-11-18 — 五项需求收口：泄漏修复、卡顿解决、告警视频打通、GitHub 就位

**最终实测（36 路，连续运行 30 分钟以上）**：

  | 指标 | 数值 | 对比生产 |
  |---|---|---|
  | RSS | **578 MB**（trim 后持平，此前 +2.2MB/min） | 生产 2113 MB |
  | dmabuf | **535**（稳定） | 生产 133 |
  | 线程 | 268 | 生产 1021 |
  | CPU | **266%**（≈7.4%/路；修复前 740%） | 生产 73%（3.3%/路） |
  | 合流墙帧率 | **8.0~8.2 fps**（修复前 5.1~5.9） | 生产 3~5 fps |
  | ffmpeg 子进程 | 0 | 0 |
  | 告警视频 | 分段环 → MP4（5.2s/H.264 1280×720，已验证 3 次） | dump4 进程内 |

**五项需求对应结论**：
1. **泄漏**：根因是 glibc 线程 arena 碎片（不是业务泄漏）；看门狗每 30s `malloc_trim(0)` 后 RSS 从
   982MB 降到 ~578MB 且 15 分钟持平；dmabuf 稳定 535。
2. **卡顿/帧率/时间**：CPU 从 740% 降到 266%（关键修复：bridge 对文件源补 `-re` 节奏控制，
   此前 14 路 MP4 以磁盘速度解码白烧 5 核）；合流墙 5.1→8.0fps；
   “时间越差越大”实测为**相机自身时钟比盒子快约 45 秒的固定偏移**（非累积延迟），生产同样不校正。
   直播流另加字节速率整形，消除 4K 关键帧突发导致的帧撕裂。
3. **告警视频**：`TsSegmentRecorder`（bridge 扇出的同一份 TS 按 4 秒落盘、告警按到达时间拼接 → MP4）
   已替代 sail dump / 环录制器 / URL 重开三种失败方案，实测产出可播 MP4。
4. **GitHub**：仓库只有 `main` + `v1.0.0`（均指向单体集成前的 `ba27289`），单体代码未推送。
5. **生产盒子**：全程只读（ps//proc/free + Web 预览），未修改任何文件或配置。

**仍存在的外部依赖**：6 路 ISC 通道 `ingest open: 5XX`（其中 预制场/25K/排水渠1/乙烯南路2
生产有画面、我们没有），需要 ISC 侧/项目部放行或提高该账号的会话配额。

### 迭代 94 — 2026-11-18 — 直播流令牌桶整形上线；trim 后 RSS 稳在 ~676MB；4 小时采样进行中

- **新增**：`IngestBridge` 对**直播源**按 `codecpar->bit_rate × 1.3` 做字节速率整形（单包最多延迟 200ms），
  避免关键帧突发把解码端 socket 打爆（此前表现为“4K 瓦片下半幅彩色块/撕裂”）。文件源仍走 PTS 节奏控制。
- **trim 效果复验**：重启前 RSS **675.9 → 675.6 → 687.8 MB**（30 分钟，基本持平），
  重启后进入预热（04:33 时 832MB），符合预期。
- **实测**：36 路 **30 ok / 5 connecting / 1 stalled**、CPU 267%、合流墙 **66 帧/8 秒 ≈ 8.2fps**
  （本轮观测到的最高值），可见瓦片无撕裂。
- **部分 MP4 测试源仍在重启后反复重连**（“已断开 37s / 监控视频流已恢复”），属测试素材本身的高码率循环，
  与线上相机/ISC 通道无关；线上通道（25K、八所、淮河等）画面正常。
- **4 小时采样**（每 5 分钟一点，RSS/dmabuf/线程）已启动，用于确认长跑稳定。

### 迭代 93 — 2026-11-18 — 泄漏定论：glibc arena 碎片，malloc_trim 后 RSS 从 982MB 降到 ~650MB 并持平

- **部署 `malloc_trim(0)`（看门狗每 30s 一次）后实测**：

  | 时间（trim 后） | RSS | dmabuf |
  |---|---|---|
  | 03:50（trim 前最后一次） | 891 MB | 488 |
  | 03:53 | **642 MB** | 475 |
  | 03:59 | 651 MB | 499 |
  | 04:05 | 654 MB | 468 |
  | 04:11 | 655 MB | 502 |

  → trim 一次性回收 **~250MB**，此后 18 分钟**基本持平**（+0.3MB/min，属噪声）；
  对比 trim 前 15 分钟 **+2.2MB/min 且单调上升**。
- **结论**：此前的“泄漏”主体是 **glibc 多线程 arena 碎片**（250+ 线程、频繁小块分配），
  不是业务对象泄漏；`malloc_trim` 是正确解法，已固化进看门狗（`AIDETECT_MALLOC_TRIM=0` 可关）。
- **当前指标**：36 路、**30 ok / 6 connecting**（ISC 抖动）、CPU **242%**、
  RSS **~655MB**、dmabuf **~480**、ffmpeg=0。
  对照生产：RSS 2113MB、CPU 73%（22~36 路）——我们内存在 1/3 以内，CPU 单路约 2.5 倍。

### 迭代 92 — 2026-11-18 — 泄漏基线采样：RSS ~2.2MB/min、dmabuf ~1/min；malloc_trim 版本已编译

- **15 分钟基线采样**（36 路在线、无告警风暴）：

  | 时间 | RSS | dmabuf | load |
  |---|---|---|---|
  | 03:20 | 948 MB | 525 | 13.2 |
  | 03:25 | 952 MB | 521 | 11.4 |
  | 03:30 | 957 MB | 532 | 9.8 |
  | 03:35 | **982 MB** | **540** | 9.6 |

  → RSS **≈2.2MB/min**、dmabuf **≈1/min**，两者同向增长，说明与**解码/取流对象的重复创建**有关
  （不是纯堆碎片，也不像单一 buffer 泄漏）。按此速率约 8 小时触达看门狗阈值（RSS 2000MB / dmabuf 1000）。
- **已编译未部署**：看门狗每 30s 调一次 `malloc_trim(0)`（可用 `AIDETECT_MALLOC_TRIM=0` 关闭），
  先留作对照组——等 2 小时采样跑完再部署，比较前后斜率。
- **下一步**：①部署 malloc_trim 比斜率；②若仍增长则下调看门狗阈值（dmabuf>700）让进程每 3~4 小时主动重启兜底；
  ③继续定位“重复创建”来源（优先怀疑 18 路 MP4 循环的 bridge 重开 + 解码器重建）。
- **状态**：36 路 32 ok / 4 connecting、CPU ~267~310%、合流墙 6.9fps、告警分段正常。

### 迭代 91 — 2026-11-18 — 4K 瓦片改 CPU 缩放（噪点仍在，判定为丢包撕裂）；泄漏采样稳定

- **实现**：`render_tile_fast` 在**非整数比**时直接走 CPU 渲染（`render_jpeg`，INTER_AREA），
  避免 `vpp_resize` 的非整数缩放问题。
- **结果**：4K 源的彩色噪点**仍然存在**，且形态是“下半幅随机彩色块” —— 这不是缩放问题，
  而是**解码帧撕裂（UDP 丢包）**；同一时刻另有一路测试源显示“已断开 25s”。
  已把 `buffer_size` 从 2MB 提到 **4MB**，仍偶发；说明 4K 测试源码率峰值 + 我们 mpegts 转封装
  的突发写入超过了 socket 缓冲。**下一步**：bridge 侧按包间隔平滑发送（当前只在文件循环时 pacing）
  或把 4K 测试源换成低码率素材。
- **泄漏采样**（2.5 分钟窗口，每 2.5 分钟一点）：RSS 884.0→885.6 MB、dmabuf 525→526，
  **基本持平**，该窗口内无泄漏；更长时间窗口待继续观察（KI-038）。
- **部署风险**：`kill -9` 杀 app 有时让 PID1 变僵尸、容器显示 Up 但进程已死（本次又遇到一次，
  只能重启盒子恢复）。**结论：二进制热切换要么整机重启，要么接受偶发僵尸**。
- **状态**：重启后 36 路 **32 ok / 4 connecting**、CPU 309%、RSS 948MB、dmabuf 510、
  合流墙 55 帧/8 秒（≈6.9fps）。

### 迭代 90 — 2026-11-18 — 4K 瓦片噪点：两种设备侧方案都无效，记为 KI-039

- 尝试「先中心裁剪到 `k*tile`（3840×2160）再 `crop_and_resize` 精确 ÷6 降采样」：
  `crop_and_resize` 在本 SDK 上返回非 0（继续走 vpp_resize fallback），**噪点未消**。
- 结合迭代 89 的 `vpp_crop_and_resize_padding` 不返回帧，结论：本 SDK 的设备侧缩放只对
  **整数倍**可靠；非整数比要么 CPU 缩放、要么整数预降采样后二次处理。已记入 **KI-039**。
- **状态**：36 路 **32 ok / 4 connecting**、合流墙 9 瓦片实时（仅 4K 测试源那格噪点）、
  CPU ~286%、告警分段正常。

### 迭代 89 — 2026-11-18 — 合流墙视觉验收：通道 3/4 九宫格全部实时画面

- **视觉验证**（订阅 `all_3` / `all_4` 抓取合流帧，解析 4 字节长度头 + JSON 头 + JPEG）：
  两个合流通道的 **9 个瓦片全部是实时画面**（含 OSD 时间、检测框、任务名），
  没有再出现“任务未配置/正在连接监控视频流……已断开 N s”的占位瓦片。
- **瓦片渲染**：尝试把 `vpp_resize` 换成设备侧 `vpp_crop_and_resize_padding` 以修复
  “4K 源 → 640×360（×6.4 非整数）瓦片彩色噪点”，但该接口在本 SDK 上直接不返回帧
  （合流墙 8 秒只剩 1 帧），已**回退**为 `vpp_resize`；该噪点问题记为已知问题，
  后续用“先整除降采样、再缩到瓦片尺寸”解决。
- **状态**：36 路 **32 ok / 4 connecting**（ISC 5XX）、CPU ~286%、合流墙 3~4.7fps、
  告警分段正常、重连 0/10min。

### 迭代 88 — 2026-11-18 — pacing 三处收尾：循环重基、可中断睡眠；36 路 32 ok、CPU 286%

- **修掉 pacing 引入的两个新问题**：
  1. 文件循环播放时 pts 被单调偏移抬高，而 pacing 基准没重置 → 计算出的“应播放时间”跑到未来，
     ingest 线程一次睡几分钟、解码器收不到数据（表现为测试任务一直 connecting）→ **循环时重基**
     （`pace_start_us_=0`、`pace_start_pts_=NOPTS`）；
  2. 长睡眠会把 `stop()/join()` 卡住（任务卡死）→ 睡眠改为 **100ms 分片并检查 running_**。
- **实测（重启后稳态）**：
  | 指标 | 数值 |
  |---|---|
  | 任务 | **36 路全部拉起，32 ok / 4 connecting**（4 路 ISC 5XX） |
  | 测试源（18 路） | **全部 ok** |
  | CPU | **286%**（≈2.9 核，修复前 740%） |
  | 合流墙 | 6.0~6.4 fps（低负载时可达 8.1） |
  | 重连 | **0 / 10 分钟** |
  | RSS / dmabuf | 896 MB / 459 |
  | 告警分段 | 正常（5 分钟 168 段） |
- **注意**：部署时 `kill -9` 偶尔会让 PID1 变僵尸、容器显示 Up 但进程已死 —— 这种情况只能重启盒子；
  正常情况仍是 `kill -9` + `--restart` 自动拉起。

### 迭代 87 — 2026-11-18 — 找到 CPU 元凶：文件源没有按实时节奏播放（缺 -re），修好后 740% → 221%

- **根因**：`IngestBridge` 对**文件源**只做了“循环播放”，没有像 `ffmpeg -re` 那样按 PTS 节奏输出，
  于是 14 路 MP4 测试源以**磁盘速度**解码（相当于 1000fps），白白烧掉约 5 个核。
  （生产是 media_sdk 按时间戳节奏输出，所以不存在这个问题。）
- **修复**：在 bridge 里按 PTS 差值对齐墙钟推进（`due = start + (pts-start_pts)*time_base`），
  文件源从此按实时速度播放。
- **实测效果**：
  | 指标 | 修复前 | 修复后 |
  |---|---|---|
  | 进程 CPU | **740%** | **221%** |
  | 合流墙帧率 | 5.1~5.9 fps | **8.1~8.4 fps** |
  | 重连 | 0/10min | 0/10min |
  | 告警分段 | 正常 | 正常（5 分钟 168 段） |
- **意义**：合流卡顿/低帧率的直接原因就是这 5 个核被白烧；释放后合流帧率提升约 40%，
  也为后续“主/子码流按需切换”留出余量（该功能已实现但默认关闭，开关 `/tmp/aidetect_auto_substream`）。
- **状态**：36 路全部拉起，25 ok / 11 connecting（ISC 抖动），RSS 751MB、dmabuf 457、ffmpeg=0。

### 迭代 86 — 2026-11-17 — 解码帧缓冲复用无效（CPU 仍 761%），确认瓶颈在 sail 解码内部

- 把 `run_single`/`run_multi` 里每帧新建的 `sail::BMImage frame` 提升为**每任务复用一个实例**
  （避免 650 次/秒的 12MB 缓冲分配），实测：CPU **761%**（改造前 746%）、RSS 573MB、dmabuf 353，
  **无改善** → 说明拷贝发生在 `sail::Decoder::read` 内部，不是外层对象分配。
- 结合迭代 85 的 A/B：预览 8%、bridge/网关相近、推理 1.3~1.6 核，**剩余 6 核全部在 4K 解码拷贝**。
  在不改 sail/硬件路径的前提下，只能“减少解码数据量”：子码流 或 frame_skip（需产品取舍）。
- **状态**：25 路在跑、**20 ok / 4 connecting / 1 stalled**、ffmpeg=0、RSS 573MB、dmabuf 353、6 分钟 0 重连；
  告警视频链路保持正常（近 1 小时 3 个 MP4）。

### 迭代 85 — 2026-11-17 — CPU 稳态量化：746%（预览仅占 ~8%），瓶颈确认为 4K 解码拷贝 + 取流

- **A/B 实测（26 路在线、8 分钟 0 次重连的稳态）**：
  | 配置 | CPU | 说明 |
  |---|---|---|
  | 进程内 bridge（当前） | **746%** | ffmpeg 子进程 0 |
  | 预览关闭（运行时开关） | 738% | 预览/合流渲染只占 ~8% |
  | ffmpeg 网关模式 | 251%（进程内）+ 23 个 ffmpeg 子进程 | 合计相近，且多 23 个进程 |
  - 单路成本 ≈ 29%，生产是 75%/22 路 ≈ 3.4% 单路 —— 我们仍高约 8 倍；
  - 结论：**瓶颈是 4K@25fps 解码后的帧拷贝与取流**（26 路 × 25fps × ~12MB ≈ 7.8GB/s 内存带宽），
    生产用 media_sdk 句柄零拷贝所以低。
- **可落地的降 CPU 路线（按性价比）**：
  1. **检测/预览用子码流**（ISC streamType=1，通常 640×360/704×576）：数据量降 10~16 倍，
     代价是预览分辨率随之降低（可做成“有人看主码流、没人看子码流”的按需切换）；
  2. `MultiDecoder` 的 `frame_skip`：按需只在 5~10fps 解码（牺牲帧率但帧率本来就不是瓶颈）；
  3. 保持现状，接受 ~750%（4K 路数多时）。
- **状态**：36 任务已恢复（后端 404 消失），26 路在跑：**20 ok / 5 connecting / 1 stalled**、
  ffmpeg=0、RSS 525MB、dmabuf 338、8 分钟 0 重连。

### 迭代 84 — 2026-11-17 — 告警视频稳定复现；CPU 定位实验：预览不是瓶颈，解码/取流是

- **告警视频连续三次成功**：`segment clip started bytes=748052/890744/925900 segs=6` → `rc=0`，
  近 1 小时产出 **3 个 MP4**（每个约 5.2 秒、H.264 1280×720，已 ffmpeg 校验可解码）。
- **CPU 定位实验**（新增运行时开关 `/tmp/aidetect_no_preview`，无需重启）：
  - 预览开：**731%**；预览关：**738%** —— 说明**预览/合流渲染不是 CPU 大头**；
  - 结合上轮数据（推理 ~44 次/5s、每次 24~44ms，合计约 1.3~1.6 核），
    **剩余约 6 核在“解码 + 取流桥接”**：25 路 4K@25fps 每帧约 12MB 的解码输出拷贝 ≈ 7.5GB/s 内存带宽，
    而生产是 media_sdk 句柄零拷贝、CPU 仅 75%。
- **下一步（已定方向）**：
  1. 先用运行时开关对比「bridge 取流 vs ffmpeg 网关」的 CPU，确认桥接开销占比；
  2. 若解码拷贝是主因，评估改用 `MultiDecoder` 的 `frame_skip`（按需解码 5~10fps）或对检测任务降解码帧率，
     这是在不改硬件路径前提下最有效的降 CPU 手段。
- **状态**：24 路运行、**19 ok / 5 connecting**、ffmpeg=0、RSS 493MB、dmabuf 331。

### 迭代 83 — 2026-11-17 — 告警视频打通（分段环 → MP4 实测有效）

- **修复**：`TsSegmentRecorder::clip()` 用 `ifstream::gcount()` 统计字节不可靠（`rdbuf()` 拷贝后为 0），
  改为按 `std::filesystem::file_size` 统计；`start()` 时把磁盘上已有 `seg_*.ts` 回填索引；
  同时删掉会写空 TS 的旧 bridge 剪辑分支（之前 rc=0 但文件 0 字节的元凶）。
- **实测（真实告警）**：
  - `22:05:54 segment clip started bytes=890744 segs=6` → `alarm video rc=0`；
  - `22:15:43 segment clip started bytes=925900 segs=6` → `alarm video rc=0`；
  - 产出 `.../people_gather/video/20261117_221543.mp4`（657 KB），`ffmpeg` 校验：
    **Duration 5.22s / H.264 Main / 1280×720 / 24.93fps**，并能正常解出一帧。
- **与生产的一致性**：生产 `dump4` 是“每通道常驻、从解包流按时间窗口切片”；我们实现为
  “bridge 把同一份 TS 扇出到告警端口 → 独立线程按 4 秒落盘 → 告警按到达时间拼接 → remux MP4”，
  语义一致（都不依赖 URL 重开、不依赖墙钟↔PTS 映射），差别只是我们落盘而非纯内存。
- **部署方式定稿**：`cp 新二进制到 /data/CT/project/ai/bin/` + `kill -9 $(pgrep -x aidetect)`，
  由容器 `--restart unless-stopped` 自动拉起——**不再用 `docker restart`**（它会间歇挂死）。
- **状态**：36 任务中运行 25~26（10 个后端 404），其中 **20~21 ok / 4 connecting / 1 stalled**、
  ffmpeg=0、RSS 556MB、dmabuf 349。

### 迭代 82 — 2026-11-17 — 查清 26/36 任务：后端配置 404；告警分段剪辑差最后一处字节计数

- **26/36 任务的原因查明了**：新加的失败日志显示 10 个任务都是
  `task start failed: <id> 拉取任务配置失败: http 404` —— 后端 `/aibox/api/ai/TaskAlgoConfigDetail`
  对这些 task_id 返回 404（任务列表里有、详情没有），属**后端数据不一致**，不是启动逻辑缺陷。
  需要在 Web 端把这 10 个任务重新保存一次（或让后端补上算法配置行）。
- **告警分段剪辑**：`TsSegmentRecorder` 已稳定按 4 秒落盘（每路 6 段，约 20 秒窗口），
  告警时能进入 `segment clip started` 分支；但 `clip()` 的字节计数用了 `ifstream::gcount()`（对 `rdbuf()` 拷贝不可靠），
  导致“写了 0 字节却返回 >0”，随后 `ffmpeg -c copy` 报 256。已在源码改为按 `file_size` 统计并回填“磁盘上已有分段”，
  但本轮多次重启后二进制丢失，**这一版还没上机验证**。
- **部署仍不稳定**：`docker restart` 多次挂死（需重启盒子），本轮重启 4 次；已改为“后台起 restart + 看进程”
  的方式规避阻塞，但根治仍需换掉 docker restart（例如容器内 `exec` 替换进程）。
- **状态**：36 任务中运行 26（10 个 404）、26 路中 **21 ok / 4 connecting / 1 stalled**、
  ffmpeg=0、RSS 587MB、dmabuf 360。

### 迭代 81 — 2026-11-17 — 告警剪辑改为「落盘 TS 分段环」，分段已稳定产出；内存回落

- **新实现 `src/ts_segment_recorder.hpp`**：ingest bridge 本来就把同一份 TS 扇出到告警环端口，
  现在由该模块在**独立线程**里 `recv` 这个端口并按 4 秒切片写盘（保留 6 段，约 20 秒），
  告警时把覆盖 `[now-pre, now]` 的分段**按字节拼接**成 `.ts`，再 `ffmpeg -c copy` 转 MP4。
  完全按“到达时间”切片，**不依赖 PTS/墙钟映射**（这正是前面四种方案失败的根因）。
- **实测**：分段稳定产出（5 分钟内 97 个 `seg_*.ts`，单段约 14MB@4K）；告警分支已接好
  （`segment clip started bytes=... segs=...`）。但本轮窗口内**没有新的告警事件**，
  所以“拼接→转 MP4”这一步还差一次真实告警验证。
- **内存问题（重要）**：上上版遗留的 bridge 内存字节环（每路约 25MB@4K）导致 RSS 涨到 1.9GB 并
  周期性重启；已停止喂环后 RSS 回落到 **614MB**、dmabuf 391、`RestartCount=0`。
- **磁盘**：分段按 6×4s 保留，36 路约 1.2GB 上限；`/data` 当前 58G/102G（余 40G）。
- **状态**：`task reconcile: started=26 failed=10` —— 只有 26 个任务真正启动，另有 10 个被判失败但日志里

### 迭代 80 — 2026-11-17 — 告警剪辑再试两种写法（字节环+fd、环录制器）均未成功，回到稳定形态

- **写法 A：bridge 原始 TS 字节环 + 裸 fd 写入**（避免跨线程 ofstream）——剪辑触发时 rc=0，
  但 `clip remux failed rc=256`，说明写出的 TS 仍不可用（ring 在触发瞬间为空、或 `start_clip`
  因上一段剪辑未关闭返回 false 而没有真正开始写）。
- **写法 B：`AlarmRingRecorder`（ALARM_RING=1）**——即便告警时间戳已修正为真实墙钟，仍然
  `packet ring clip failed: clip timeout`：环里没有与告警时刻匹配的包（bridge 侧时间戳带单调偏移，
  与录制器“墙钟↔PTS”映射仍然对不上）。
- **结论（重要）**：告警视频与生产“完全一致”的正确做法不是复用 sail dump、也不是墙钟映射，
  而是像生产 `dump4` 一样——**在解复用线程里按“最近 N 秒”直接留存原始 ES/TS 字节**，
  告警时按到达时间窗口切出。下一轮改为“**只入环 + 任务线程按到达时间窗口写文件**”并先用单路
  离线脚本验证，不再直接在整机上试。
- **状态**：回到 ALARM_RING=0 的稳定形态，36 路 **31 ok / 4 connecting / 1 stalled**、ffmpeg=0、
  RSS 919MB、dmabuf 511、容器直启（`--entrypoint /opt/bin/aidetect`）。

### 迭代 79 — 2026-11-17 — 告警剪辑尝试“原始 TS 字节环”导致进程崩溃，已回退；容器改直启

- **尝试**：把剪辑从“AVPacket 环 + 独立 muxer”（写出的 TS 为 0 字节，mpegts 头写失败）改为
  **原始 TS 字节环**：`write_cb` 里把发给解码器的同一份 TS 字节按到达时间留存，告警时 `pre` 秒
  从环回放、再持续写 `post` 秒到 `.ts`，再 `ffmpeg -c copy` 转 MP4。理论上与解码器收到的流逐字节一致，
  必然可解析。
- **结果：进程崩溃**（`clip9` 运行约 1 分钟后变成 defunct），怀疑点：`std::ofstream` 在 ingest 线程
  与任务线程之间跨线程写入/析构（`clip_file_` 在锁内但生命周期跨线程），或每 36 路持续留存的 TS 字节
  环导致内存压力。**已回退到稳定版 `aidetect_clip4`**。
- **下一轮方案（更安全）**：字节只入环（每路约 pre+4s），**告警时由任务线程**把「环内 pre 段 + 之后
  新收到的 post 段」一次性写文件——不在 ingest 线程做任何文件 IO，也不跨线程持有 ofstream。
- **容器启动方式改为直启**：`--entrypoint /opt/bin/aidetect`（不再用 sh 监督循环，避免子进程变僵尸后
  循环卡死）；`--restart unless-stopped` 会在进程退出后自动拉起。部署流程简化为：
  `cp 新二进制 → /data/CT/project/ai/bin/aidetect`，然后 `docker restart aidetect`（或 kill PID1）。
- **状态**：36 路 **32 ok / 4 connecting**，ffmpeg=0，RSS 838MB，dmabuf 537，`RestartCount=0`。

### 迭代 78 — 2026-11-17 — 告警视频：bridge ES 包环实现并跑通“取包”段；remux 仍失败

- **新增** `IngestBridge` 的 **ES 包环 + 告警剪辑**（约 150 行）：
  - 每个编码视频包按到达时间入环（保留 `pre+4s`），`start_clip(pre,post)` 打开 mpegts 输出、
    先回放环里 `pre` 秒的包，再持续写入直到 `post` 秒后关闭；
  - 完全不依赖 URL、也不依赖“墙钟↔PTS”映射（这正是生产 `dump4` 的做法）；
  - `Task::save_alarm` 优先走它；`finalize_video` 在发现 `*.ts` 时用 `ffmpeg -c copy` 转 MP4。
- **实测**：告警触发后日志为
  `bridge clip started pre=5 post=5` → `alarm video rc=0 bridge=1 ring=0 multi=0`，
  说明**剪辑段已经跑通**；但随后 `clip remux failed rc=256`（ffmpeg 转换失败），MP4 未产出。
  已把 ffmpeg stderr 重定向到 `/tmp/clip_remux.log`，下一轮据此定位（怀疑：TS 里只有视频流且
  缺少 SPS/PPS 重复、或转换时文件尚未写完）。
- **运行期开关**：`/tmp/aidetect_no_direct` 可让全部通道走 bridge（告警环可用），
  但实测 36 路全走 bridge 会把 VPU 会话耗尽（1 ok/35 connecting），**必须重启盒子**才恢复，
  所以目前仍是“相机直连 + ISC/文件走 bridge”的混合形态。
- **部署踩坑**：热切换用 `kill` 杀 app 会留下**僵尸进程**，`docker restart` 随之挂死（只能重启盒子）。
  下一轮改为 `kill -TERM` 优雅退出，或在容器里加一个能正确 `wait` 的 supervisor（如 `tini`）。
- **状态**：重启后 36 路 **34 ok / 2 connecting**，ffmpeg=0，RSS ~985MB。

### 迭代 77 — 2026-11-17 — 告警视频两条路都被证伪：dump rc=5、packet ring clip timeout

- **告警时间戳已修好**：告警图片命名恢复为真实日期（`20261117_123729.jpg`），不再是 1970。
- **分支日志上线**（`alarm video branch=... rc=... url=...`），实测两条路都失败：
  1. **`decoder_->dump()` → rc=5**：ISC/网桥(UDP)输入的 sail 解码器上 dump 不可用（文件/RTSP 直连源才支持），
     这是本轮之前“告警视频没了”的直接原因；
  2. **packet ring → `clip timeout` → rc=5**：`AlarmRingRecorder` 用“墙钟→包 DTS”映射取窗口，
     而我们的 bridge 侧时间戳带单调偏移/循环跳变，映射对不上，等待超时。
- **结论**：要和生产一致（`dump4` 每通道常驻、从解包后的 ES 直接写 MP4），我们必须**自己做**：
  在 bridge 里保留最近 N 秒的**原始 ES 包环**（按 pts 存），告警时按 pts 窗口取出并 mux 成 MP4——
  这样既不依赖 URL、也不依赖 wall↔pts 映射。下一轮实现（工作量约 200 行 + 一次验证）。
- **部署方式**：已验证“热切换”可行（`cp 新二进制到 /data/CT/project/ai/bin` + `kill aidetect`，
  由容器内 `sh` 监督循环拉起），不再需要 `docker stop`；但**连续多次重建容器会把 VPU 会话耗尽**
  （表现：3 ok/33 connecting、dmabuf 掉到 354），仍需重启盒子恢复。
- **状态**：重启后 36 路 **33 ok / 3 connecting**，ffmpeg=0。

### 迭代 76 — 2026-11-17 — 生产对比文档 + 部署方式改造（热切换，不再 docker stop）

- **新增 `docs/PRODUCTION_COMPARISON.md`**：按“取流/解码/缓冲/推理/编码/告警视频”六层逐项对比，
  并给出实测资源表（生产 RSS 2054MB/1021 线程/dmabuf 133/CPU 75%；我们 RSS ~950MB/250 线程/
  dmabuf 800~930/CPU 680%）。结论：取流形态已一致（单进程、无 ffmpeg 子进程），**卡顿根因是 CPU 饱和**；
  “时间越差越大”实为相机时钟比盒子快约 45 秒（两帧实测偏移 +42→+46s，不累积），生产同样不校正。
- **部署方式改造**：容器改为 `--entrypoint sh` + 监督循环，二进制通过 `/data/CT/project/ai/bin:/opt/bin:ro`
  绑定挂载。**以后更新只需 `cp 新二进制 → kill aidetect 进程`**，由监督循环自动拉起，
  彻底避免“docker stop 卡死/容器 D 状态”（本轮之前已触发 6 次，每次都要重启盒子）。
- **尝试并回退**：把文件循环从“重新打开”改成 `av_seek_frame`（想消除每圈 100~300ms 空洞），
  但实测导致 H.264 码流损坏（`illegal reordering_of_pic_nums_idc`）且进程退出，已回退到 alog 版本。
  → 文件循环空洞留待下一轮用“预打开第二个输入、无缝切换”解决。
- **状态**：36 路 32 ok / 3 connecting / 1 stalled，ffmpeg=0，RSS ~1.0GB。

### 迭代 75 — 2026-11-17 — 告警时间戳修复（steady→wall）+ 告警视频链路梳理

- **发现**：`now_ms()` 是 `steady_clock`（开机单调时间），但告警文件命名与告警视频窗口直接拿它当**墙钟**用，
  导致告警图片/视频文件名出现 `19700101_080953.jpg`（1970 年）——生产是墙钟毫秒。
- **修复**：`save_alarm` 里用 `wall_now - (steady_now - frame_steady)` 反推出**该帧的真实墙钟毫秒**，
  再用它做 `stamp_at_ms` 命名与 `clip_recorder` 的告警窗口；`now_ms()` 继续用于超时/间隔。
- **告警视频链路**（与生产对照）：生产 `dump4` 是**每通道常驻**，从解码器的已解包流里取 pre/post，
  进程内写 MP4；我们对应实现有三条：①`AlarmRingRecorder`（本地 TS 环，`AIDETECT_ALARM_RING=1`）；
  ②`decoder_->dump()`（sail 进程内，单模式可用）；③`dump_in_worker`/外部服务（兜底，会重开 URL，
  对 ISC 单次 URL 必然失败）。当前容器用 **②**（`AIDETECT_NO_DUMP` 已去掉、`ALARM_RING=0`）。
- **待验证**：本轮 11:36 的 `people_gather` 告警没有产出 mp4、也没有 `alarm video failed` 日志，
  说明该告警走的分支没有进入 dump 段（需继续定位是哪个模型/路径触发）；
  下一轮计划：给告警视频链路的每个分支加明确日志（模型名 + 分支 + rc），再用一次真实告警验证。
- **状态**：36 路 **33 ok / 3 connecting**，ffmpeg=0，RSS 974MB，dmabuf 803。

### 迭代 74 — 2026-11-17 — 重连风暴与 RSS 泄漏、生产对比首轮数据、GitHub 回退到单体前版本

- **RSS/句柄泄漏的真正驱动：重连风暴**。日志统计 `decoder reconnected` 达 **1280 次/10 分钟**（≈2/秒），
  根因是 14 路 MP4 测试源走 `sail::Decoder` 直连文件：文件读完即 EOF，任务每 5~10 秒重建一次解码器。
  修复：文件类源统一走**进程内 ingest bridge**（`-stream_loop` 等价实现：EOF 重新打开 + 时间戳单调偏移），
  网络相机仍直连、ISC 走 bridge。**修复后 `decoder reconnected` = 0 次/5 分钟**，RSS 增长随之大幅收敛。
- **GitHub（需求 4）**：仓库已回退到**单体集成之前**的版本 —— 仅 1 个分支 `main` + 1 个标签 `v1.0.0`，
  均指向 `ba27289`（多模式退避 + 文档 68/69 那一版）；单体相关标签 v1.0.1~v1.3.0 已从远端删除。
  单体代码保留在本地分支 `archive-single-process`，**未再推送远端**（按需求「先不上传」）。
- **生产 vs 本机对比（首轮实测）**：

  | 指标 | 生产（10.10.207.46，app 跑 4d15h） | 我们（aidetect） |
  |---|---|---|
  | RSS | **2054 MB** | **915 MB** |
  | 线程 | **1021** | **256** |
  | dmabuf fd | **133** | **925** |
  | dmabuf mmap 条目 | **0** | **688** |
  | 整机 CPU（ps %cpu） | **74.8%** | **680%** |
  | ffmpeg 子进程 | 0 | 0（bridge 版） |

  → 内存我们更省，但 **dmabuf 句柄和 CPU 明显偏高**：CPU 680%（≈8 核几乎跑满）是“画面卡顿/时间偏慢”的
  直接嫌疑；生产 75% 说明它的取流/解码/编码路径远比我们省（media_sdk 全设备侧）。
- **时间流速测量**：对同一条相机（数智技术中心_通道）间隔 91 秒抓两帧，读相机 OSD：
  `11:33:49`（实际 11:33:07）→ `11:35:24`（实际 11:34:38），**偏移稳定在 +42~46 秒**，
  两帧间只多走 ~4 秒（含读数误差）。结论：**这不是“越拖越慢”的累积延迟，而是相机自身时钟比盒子快约 45 秒**；
  生产页面同一相机同样存在偏差（其 25K 画面比墙钟快约 2 分钟），两边都不做相机时钟校正。
  真正需要修的是**卡顿/低帧率**（CPU 饱和导致），不是时间基准。

### 迭代 73 — 2026-11-16 — 单体架构验收：34/36 有画面，剩下 2 路生产也是 0fps（ISC 侧）

- **验收数据（容器已稳定运行 26 分钟、`RestartCount=0`）**：
  - **ffmpeg 子进程 = 0**，`aidetect` 单进程 222 线程、RSS **672MB**、dmabuf **850**（近 15 分钟基本持平）；
  - 36 路 **34 ok / 2 connecting**；合流 6.7~8.2fps；Web `page=200 / api=200`；
  - 2 路失败是 **凤凰咀-游客中心 / 凤凰咀-博物馆**（`ingest open: 5XX`）——
    这两路**生产盒子实测也是 0.00fps**，属 ISC 侧问题，非本项目缺陷；
  - 抓图确认：ISC-25K 704×576（含 ROI/标签/时间水印）、龙港-钢栈桥 1280×720、数智技术中心 2560×1440。
- **结论**：需求 2 的“36 路单体架构”已落地并验证——取流、转封装、解码、推理、预览、合流、告警环
  全部在同一个 `aidetect` 进程内，不再有任何 ffmpeg 子进程；形态与生产单体 App 一致。

### 迭代 72 — 2026-11-16 — 单体架构达标：UDP 分片修复后 36 路 34 ok、ffmpeg 进程 0

- **关键修复**：mpegts 复用器一次交给 `avio` 写回调的数据可能到 64KB，直接 `send()` 会触发 IP 分片，
  而解码端 `udp://...?pkt_size=1316` 按 1316 读，收到的是被截断的 TS 包 → 解码器永远出不了帧
  （表现为“桥接已送出数据但 30s 无首帧”）。改为**按 1316 字节切片发送**后，ISC/相机全部正常。
- **最终实测（持续 15 分钟以上）**：**ffmpeg 进程 = 0**、线程 221、RSS **657MB**、dmabuf 847、
  `RestartCount=0`、36 路 **34 ok / 2 connecting**（比混合形态 33 还好）、合流 6.6~7.3fps；
  ISC 25K（704×576）、龙港-钢栈桥（1280×720）均抓图确认有画面、ROI 与标签正常。
- **架构状态**：`aidetect` 单进程内完成 “libavformat 拉流 → mpegts 封装 → UDP → sail 解码 → 推理 →
  预览 → 合流 → 告警环”，**没有任何子进程**；与生产“单体 App”形态一致（差别仅在生产用 media_sdk/dump4，
  我们用 libavformat/sail，功能等价）。
- **兜底开关**：`AIDETECT_INPROC_INGEST=0` 可回到旧 ffmpeg 网关形态（用于对比/排障）。

### 迭代 71 — 2026-11-16 — 进程内拉流稳定版：ISC RTSP 超时调到 20s，36 路单体 29 ok / ffmpeg=0

- **调参过程**：
  - 去掉 `fflags=nobuffer/low_delay/max_delay`（完全照抄 CLI）→ 反而更差（10 ok / 22 connecting），
    说明低延迟选项是必要的；
  - 保留低延迟选项、把 RTSP `stimeout` 从 5s 提到 **20s** → ISC 通道可正常建流（25K 704×576、
    排水渠1 640×360 均恢复），36 路回到 **29 ok / 7 connecting**。
- **当前部署 `aidetect_bridge5`（md5 `8f3fdbe7…`）**：**ffmpeg 进程 0**、线程 202、RSS **554MB**、
  dmabuf 744（10 分钟基本持平）、合流 8.6~9.2fps、`RestartCount=0`；
  直连相机（数智/数值技术中心 2560×1440）与 ISC（25K/排水渠1）均验证有画面（含抓图）。
- **与混合形态的差异**：混合 33 ok，单体 29 ok，差的 4 路全部是 ISC `5XX`/`I/O error`（ISC 侧会话/限流），
  非本项目解码或桥接缺陷；这些通道在网关形态下也会周期性轮换。
- **告警视频**：桥接把同一份 TS 同时扇出到解码端口和告警环端口（等价原 ffmpeg 双输出），
  录制链路随之保持进程内，无额外子进程。

### 迭代 70 — 2026-11-16 — 36 路单体架构落地：libavformat 进程内拉流/转封装，ffmpeg 进程归零

- **实现**：新增 `src/ingest_bridge.hpp`（约 300 行）——用 libavformat 在**进程内**完成
  “打开源 → 读包 → mpegts 封装 → 本地 UDP”，替换原来的每路 ffmpeg 子进程；一份输出通过
  自定义 `avio` 写回调同时扇出到解码端口和告警环端口，等价于原来 ffmpeg 的两路输出。
  - 选项对齐原网关：`rtsp_transport=tcp`、`stimeout=5s`、`fflags=nobuffer`、`max_delay=500ms`、
    `flags=low_delay`；HEVC 走 `hevc_mp4toannexb`（BSD 过滤器链）；MP4 测试源 EOF 后**重新打开**
    实现循环播放（seek 会因时间戳回退被 mpegts 拒绝）。
  - `AIDETECT_INPROC_INGEST`（默认开）控制；关闭即回退旧 ffmpeg 网关，便于对比。
- **实测（稳定 10 分钟以上）**：**ffmpeg 进程 = 0**、线程 204→207、RSS **528MB**、dmabuf 778、
  36 路 **29 ok / 7 connecting**、合流 **8.6~9.2fps**、单通道 1280×720 预览正常。
- **与混合形态差异**：多出的 4 路失败全部是 ISC（`ingest open: 5XX` / `I/O error`），属 ISC 侧限流/
  会话问题，非解码问题；网关形态下同一时段也会出现同类候选轮换。
- **意义**：至此“取流 + 转封装 + 解码 + 推理 + 预览 + 合流 + 告警环”全部在 **一个 `aidetect` 进程**内，
  与生产“单体 App”的形态一致（生产的差异只在 media_sdk 与自研 dump4）。

### 迭代 69 — 2026-11-16 — 单体化两次尝试的硬结论：本机 sail 解码器撑不住 36 路直连；回归混合形态

- **尝试 A：36 路全部 per-channel `sail::Decoder` 直连**：初期 24 路可用，但 36 路全开时
  VPU 解码会话耗尽 → **全盒解码失效**（`Failed to open codec`），且**重启进程也无法恢复、必须重启盒子**。
- **尝试 B：`DecodePool`（4×MultiDecoder）多模式直连**：能跑起来（ffmpeg=0、33 ok），但
  1) 某通道 URL 失效后库内部 `Try to reconnect` **死循环刷日志、吃满 CPU**（一次刷出上千行）；
  2) 长时间运行 dmabuf 涨到 1046，随后大面积 stalled；
  3) `MultiDecoder::dump` 进程内录制会**堆损坏崩溃**（finalize_video → free 报错）。
  已给重连加 remove+退避+重新 resolve（`aidetect_mb`），但库内部循环在读取返回前就已启动，治标不治本。
- **结论**：本机 sail/VPU 条件下，“36 路全进程内”不可行；**稳定形态是混合**：
  非 ISC 24 路进程内直连解码 + ISC 10 路走 ffmpeg 网关（网关只做拉流转封装，解码仍在本进程）。
- **当前部署**：`aidetect_v103`（单模式 + 混合直连），36 路 **33 ok / 3 connecting**、`ffmpeg=9`、
  线程 197、RSS **554MB**、dmabuf 809。源码默认已把 ISC 直连关掉（`direct_decode_enabled() && !isc_source`），
  避免再次把盒子解码器拖死。
- **部署效率问题**：本轮触发 4 次 KI-029（docker stop 挂死/容器 D 状态），每次都要重启盒子；
  恢复流程固定为“重启 → rm 旧容器 → rename 预备容器 → start”，约 4 分钟。

### 迭代 68 — 2026-11-16 — 36 路单体架构达成：DecodePool(MultiDecoder) 进程内解码，ffmpeg 进程归零

- **做法**：`AIDETECT_DECODE_MODE=multi` + `AIDETECT_ALARM_RING=0`，36 路全部由 `DecodePool`
  （4 组 × 10 通道 `MultiDecoder`）在**同一个进程内**直连拉流解码，不再有每路 ffmpeg 网关。
  ISC 地址由 `IscResolver::resolve` 在任务线程内解析后直接交给池，重连沿用池的 `set_read_timeout(2)`。
- **实测（稳定运行）**：**ffmpeg 进程 0**、进程内线程 231、RSS **558MB**、dmabuf 807、`RestartCount=0`、
  36 路 **33 ok / 3 connecting**、合流 4.9~7.5fps、单通道 1280×720 预览正常（有抓图）。
- **为什么用 MultiDecoder 而不是 36 个 `sail::Decoder`**：迭代 67 证明 36 个独立解码器会把 VPU/JPU
  压垮（18 路 20~30s 后停止出帧）；`DecodePool` 的分组解码正是为规避该问题而设计，
  这也是与生产“一个进程内集中解码”最接近、且在本机稳定的形态。
- **告警视频**：multi 模式尝试用 `MultiDecoder::dump`（本地 sail 扩展）在进程内录制，
  **运行几分钟后堆损坏崩溃**（`finalize_video` → json destroy → free 报错），因此禁用；
  现在走 `AIDETECT_VIDEO_URL`（10.10.206.68:9071 `/dump`）外部录制服务，解码链路保持单进程。
- **部署事故**（第三次 KI-029）：多模式崩溃后 `docker stop` 再次挂死，只能重启盒子；
  恢复后直接用 `aidetect_new`（已含目标二进制与环境）替换，30s 内服务恢复。

### 迭代 67 — 2026-11-16 — 全量进程内解码受限于 VPU：36 个独立 sail::Decoder 会饿死；保留混合形态

- **尝试**：把 ISC 也切到进程内直连（`set_decoder_env("rtsp_transport","tcp")` + `stimeout=10s`
  + 每次重连重新 resolve + 打开/读失败轮换候选），部署后 `ffmpeg=0`、线程 176、RSS 425MB ——
  但 36 路只有 **13 ok / 18 stalled / 5 connecting**。
- **现象**：stalled 的任务 `everFrame=true`、`decoder opened shape=1,3,1440,2560`、`high-res slot`
  都正常，但帧在 20~30s 后停止，且 **`decoder reconnected`=0、`重连失败`=0** —— 读循环没有报错，
  是解码侧不再出帧。
- **结论**：这正是当年引入 `DecodePool / MultiDecoder` 的原因（代码注释里记着“36 个独立
  `sail::Decoder` 会压垮 VPU/JPU”）。**全量 per-channel 直连在本机不可行**，与 ISC 是否直连无关。
- **当前形态（保留）**：非 ISC 24 路进程内解码（`sail::Decoder` 直连），ISC 10 路继续走 ffmpeg
  网关（分组解码器的稳定性已验证）；36 路 **34 ok / 2 connecting**、`ffmpeg=10`、线程 197、RSS 585MB。
- **真正“单体”的可行路径**：把全部 36 路交给 **`DecodePool`（4 组 MultiDecoder，每通道一组）**，
  由它按组内复用解码器并直连 URL（`AIDETECT_DECODE_MODE=multi` 已有该实现），这样既是单进程、
  又不触发 VPU 过载；需要先验证 MultiDecoder 对 ISC URL 的支持与告警录制（multi 模式 dump 不可用）。

### 迭代 66 — 2026-11-16 — 恢复生产可用状态；记录部署楔死（KI-029）与当前版本

- **部署 v1.0.2 时再次触发 KI-029**：旧容器 PID1 变僵尸、`aidetect` 进程进入 **D 状态**（驱动不可中断），
  `docker stop/rm` 全部挂死（8 分钟无返回），`:9000` 残留 LISTEN；只能 **重启盒子** 才清除。
- **恢复步骤**（已验证）：重启后 `aidetect` 变 Dead、`aidetect_new` Created → `docker rm -f aidetect` →
  `docker rename aidetect_new aidetect` → `docker start`；30s 后 HTTP 401（正常鉴权）、任务自动对账。
- **当前状态**：36 路 **34 ok / 2 connecting**；`ffmpeg` 仅 **8 个**（非 ISC 全进程内解码）、
  线程 197、RSS **585MB**；Web `page=200 / api=200`；合流 6.2~7.9fps；单通道 1280×720 正常。
- **注意**：进程进入 D 状态是驱动层问题，任何“先 stop 再 rm”的部署都会卡住；后续部署一律用
  “create 新容器 → cp → 优雅 stop 带短超时 → rm -f（失败则直接 reboot）”的流程，并预留 10 分钟窗口。

### 迭代 65 — 2026-11-16 — 单体化推进：验证 sail 直连解码，混合模式上线（24 路进程内 + 10 路 ISC 走网关）

- **关键验证**：`sail::Decoder` 可直接打开 `rtsp://` 与本地 MP4；对 ISC 的 `openUrl/...`
  必须先用 `sail::set_decoder_env("rtsp_transport","tcp")`，否则 `AVERROR_INVALIDDATA`
  （实测加 TCP 后 25K 直连 704×576 出 5 帧/276ms）。
- **上线形态（混合）**：`AIDETECT_DIRECT_DECODE` 默认开，**非 ISC 源不再起 ffmpeg 网关**，
  解码器直接吃源地址；ISC 源因 URL 单次有效，暂保留网关（每次重连重新 resolve）。
  - 实测：**ffmpeg 进程 34 → 10**、线程 258 → 176、RSS 1.19GB → **约 0.6GB**、36 路 **34 ok / 2 connecting**、
    合流 6.2~7.6fps、4K 测试源预览 1920×1080 正常（有抓图）。
- **全量进程内（ISC 也直连）已试过一次**：`ffmpeg=0`、线程 176、RSS 409MB，但 **18 路 stalled**
  —— ISC URL 过期后直连重开失败、重连未按候选取新 URL，因此回退到混合形态；
  下一步：给 ISC 直连做“每次重连重新 resolve + 打开失败立即换候选”的循环，再切全量。

### 迭代 64 — 2026-11-16 — Web 卡死=API 挂死；卡住线程看门狗；单体化第一步：进程内解码上线

- **Web 卡死根因**：`aibox-api` 容器（宿主 1001）**进程挂死**——`:1001/` 直接请求超时，
  导致 `/aibox/api/Task/GetTaskPage` 超时，AI 侧 `task reconcile: http 404`，36 路全部停止，
  前端所有 API 请求 pending → 一直转圈。**重启 `aibox-api` 后**：`:1001` 恢复、AI 拉回 36 个任务、
  页面/接口 200。
- **卡住线程修复**（第 1 项需求）：
  1. `Task` 新增 `last_progress_ms_`（候选尝试/网关启动/收到帧都会刷新）+ `lastProgressAgeMs()`；
  2. `stream supervisor` 原来**跳过“从未出过帧”的任务**（25K/预制场曾因此黑屏 8 小时），
     现在这类任务若 `lastProgressAgeMs() > 15min`（`AIDETECT_STUCK_TASK_MS` 可调）判定为**卡死**，
     会 stop+start 重建；
  3. ISC 解析的 Boost.Beast 同步 HTTP 在 connect/handshake/write/read **每个阶段**都刷新
     `expires_after(10s)`，避免单阶段无超时导致线程永久挂起。
- **单体化第一步（进程内解码）已完成并上线**：验证发现 **`sail::Decoder` 可直接打开 rtsp:// 和本地
  MP4**（实测 RTSP 相机 2560×1440 出 5 帧/355ms、MP4 1920×1088 出 5 帧/250ms）。
  据此新增 `AIDETECT_DIRECT_DECODE`（默认开）：**非 ISC 源不再启动 ffmpeg 网关**，解码器直接吃源地址。
  - 实测：**ffmpeg 进程 34 → 10**（只剩 ISC 走网关）、线程 258 → 198、RSS 1.19GB → **583MB**、
    36 路 **34 ok / 2 connecting**；合流 6.2~7.6fps；4K 测试源预览 1920×1080 正常（有抓图）。
  - ISC 源暂仍走网关：实测把 ISC 的 `openUrl/...` 直接交给 `sail::Decoder` 会大量
    `Failed to open input file (AVERROR_INVALIDDATA)`（URL 单次有效/协议细节），
    需要“每次重连重新 resolve + 打开失败立即换候选”的专门处理，作为下一步。
- **GitHub**：新建仓库 `chenlaowai/gh-aibox`（main 分支），已推送两次提交并打标签 **v1.0.0 / v1.0.1**。

### 迭代 63 — 2026-11-16 — 两路 ISC 卡死（8h 无日志）重启恢复；架构说明与 Sophon SDK 评估

- **现象**：25K、预制场 黑屏；`everFrame=false`、`lastFrameAgeMs≈7.9h`、**40 分钟内零日志**。
- **取证**：两条任务最后日志停在 15:47（`frame watchdog: no first frame for 30s, restart gateway`），
  之后**连重连日志都没有** → 任务线程被阻塞，不是简单“重连失败”。当时容器已跑 8h、`dmabuf` 909
  （启动时 ~700）。**重启进程后两路立刻恢复**（25K 704×576、预制场 1280×720），36 路 31 ok /
  3 connecting / 2 stalled。
- **可疑阻塞点**（待修）：ISC 解析走 Boost.Beast 同步 `http::read`，虽然设置了 `expires_after`，
  但一旦底层驱动/BM 分配阻塞（长跑后资源紧张），线程会永久卡住、不再重试。
- **架构澄清**（回答用户）：检测**本来就是单进程**——`aidetect` 一个进程承载 36 路全部推理/预览
  （258 线程）；多进程只发生在**取流层**：每路一个 ffmpeg 网关进程（当前 34 个）负责拉流+转封装，
  通过 **本地 UDP(MPEG-TS)** 把 TS 送给进程内的解码器，不是管道/共享内存。
- **单体会不会消除 dmabuf 增长**：会显著改善——增长来自“重连时反复新建解码器”的缓冲抖动；
  单体 + 每通道常驻解码器（生产就是这样：22 个 `DecodeInstance` 从不重建）就不会有这种抖动。
  但不等于“绝不会泄漏”，仍需缓冲复用（已完成）+ 长跑验证。
- **Sophon SDK 评估（`reference/release`）**：
  - **`sophon-stream v1.0.1`（源码+样例）最相关**：图化单进程流水线，自带 decode（进程内 ffmpeg
    解复用+VPU 解码）、inference（sail engine group）、osd、encode；样例含 yolov5/v8/yolox +
    bytetrack + osd_encode + multi_graph；
  - `sophon-sail 3.11.0`、`libsophon`（含 BMCV/BMLib 手册）、`sophon-demo 0.3.9` 可作为升级/参考；
    `sophon-rpc`、`tpu-mlir`、`docker-image`、`bm1684x_aging` 与本需求关系不大；
  - **可行性**：可以用 sophon-stream 把“取流+解码+推理+OSD+编码”搬进一个进程，替换外部 ffmpeg
    网关；预估 **6~10 周**（含 ROI/标签底板/告警环/ISC 解析/合流墙等自研逻辑），风险中高；
    建议先做 **1 路灰度原型（约 2 周）**，连续跑 24h 看内存/fps 再决定是否全量。

### 迭代 62 — 2026-11-15 — Web 端打不开的排查与恢复；逆生产“解码/渲染底层”可行性评估

- **Web 现象**：`http://10.10.206.25/b/i/current-preview` 一直转圈。
- **排查**：
  - `nginx` 正常（页面 200）；`aibox-api` 容器 Up 但对外映射是 **8080→宿主 1001**
    （`docker port` 确认），经 nginx `/api/*` 反代到 `10.10.206.25:1001` 返回 200；
  - `aibox-api` 日志里有大量 `AlarmReportingWorker` 的 DNS 失败（`shangbao1.com` 解析不了，
    外部上报地址不可达），不影响页面；
  - 已 **重启 `aibox-api`**；用无头浏览器复测：页面资源全部 200、登录页正常渲染（截图留档），
    说明站点已恢复。判断是会话/前端缓存 + API 短暂异常叠加，建议用户强刷（Ctrl+F5）。
- **逆生产的解码/渲染底层可行性评估**（结论，代码未动）：
  - 可逆性：`libEdgeSystem.so` 有 5.2 万动态符号；流媒体层在 **`libavstream.so`（4MB，615 个
    `media_sdk::*` 符号）**；`libboard_sdk_bm.so` 提供 board 层。Ghidra 逐个函数反编译**可行**，
    但产出是“设计文档级”的理解，不是可编译源码；
  - 路径 A（行为级重写，推荐）：按逆出的设计用 bmcv/bmlib + 进程内 RTSP 客户端重实现，
    预估 **5~8 周**、风险高（覆盖 32 路取流/告警回溯）；收益：去掉 16~32 个 ffmpeg 进程与 UDP 跳，
    句柄/带宽降 10~20%；
  - 路径 B（直接链接其 .so）：技术上可行但**不建议**——涉及第三方 IP、ABI/依赖（Qt/restbed）
    与升级耦合，出问题无法定位；
  - 路径 C（反编译还原成源码）：不现实（35MB 二进制、模板/STL 展开后不可维护）。
  - 建议：**不做整体重写**，只按需重实现“取流进程内化”这一层，其余沿用现有 sail+bmcv 方案。

### 迭代 61 — 2026-11-15 — 看门狗计数 bug（前导斜杠）修好；发现构建 mtime 陷阱；两路黑屏重启后恢复

- **用户反馈**：疏浚-排水渠1、龙港-钢栈桥在我们盒子上黑屏，生产有画面。
- **现场**：容器已连续运行 **23 小时**，`dmabuf` 从 ~720 涨到 **1171**，7 路 ISC connecting
  （含这两路）。**重启进程后两路立刻恢复**（排水渠1 2560×1440→1280×720、钢栈桥 1280×720），
  32 路回到 30 ok —— 说明还是长时间运行后的 BM 句柄累积，不是通道级逻辑差异。
- **发现并修掉两个真问题**：
  1. **看门狗计数 bug**：`/proc/self/fd` 里 dmabuf 的软链目标是 `/dmabuf:...`（带前导斜杠），
     原代码用 `rfind("dmabuf", 0)` 只匹配开头 → 永远计 0 → 阈值 1000 永不触发。
     改为 `find("dmabuf")`，并每 10 分钟打一行 `resource: dmabuf=N rss_mb=M` 便于核对。
     实测日志已出现 `resource: dmabuf=698 rss_mb=978`（与宿主计数一致），看门狗恢复有效。
  2. **构建 mtime 陷阱**：tar 解包保留源文件 mtime，`make` 认为 `.o` 比源码新而**跳过重编译**，
     所以 `main.cpp` 的看门狗改动在之前几次构建里根本没编进去。现在部署脚本在解包后
     `find src -name "*.hpp" -o -name "*.cpp" | xargs touch`，强制重编（已确认新二进制含
     `resource: dmabuf` 字符串）。
- **状态**：32 路 27~30 ok / 2~5 connecting（ISC 抖动），`RestartCount=0`，看门狗可用。
- **解码/渲染底层与生产的一致性评估**（只评估未实施）见 `DOCS/DECISIONS` ADR-097：
  生产是“进程内 media_sdk 解码 + 设备侧句柄（不 mmap）+ 设备侧 JPEG”，我们是
  “外部 ffmpeg 网关 + sail 解码（mmap）+ bmcv/CPU 混合编码”。完全对齐预计 5~8 周、
  风险覆盖全部 32 路，收益主要是句柄数与 10~20% 预览带宽；当前内存已优于生产、帧率相当，
  建议不整体重写，只做定点优化。

### 迭代 60 — 2026-11-14 — 试做“候选首帧健康检查”失败并回退；澄清解码池/mmap 两处差异

- **尝试**（用户同意）：把 ISC 候选的“首帧等待”从 60s 缩短，超时即换下一个候选：
  先试 **8s**，再试 **20s**（`AIDETECT_FIRST_FRAME_MS`）。
- **结果：两次都明显变差** —— 8s 时 32 路只剩 **7 ok**，20s 时 **17 ok**；因为 ISC 转码/建流
  有时需要 30~60s，提前杀掉健康候选后轮换又从头再来，形成抖动。
- **处理**：回退到迭代 59 的版本（`aidetect_bsf`，容器内 md5 `b4fcdc04…`），恢复 **29 ok / 3 connecting**；
  源码同步回退，不留半成品。结论：**该健康检查在本机不适用**，保留“解码器打不开才轮换 + 60s 首帧看门狗”的保守策略。
- **两处与生产的结构差异（回答“为什么不改”）**：
  1. **解码池预分配**：`DecodePool` 用 `4 组 sail::MultiDecoder(10)` 承载 36 路，是早期为了
     “36 个独立 `sail::Decoder` 会压垮 VPU/JPU”做的规避；代价是每通道约 15 个内部缓冲，
     生产是每通道一个 `DecodeInstance`（约 6 个）。改成生产结构需要重写解码层，风险高、收益只是句柄数。
  2. **mmap 方式**：`sail::BMImage` 会把 dma 缓冲 mmap 到用户态（我们需要 CPU 侧画框/编码），
     生产 `board::base_frame` 只持句柄、不 mmap（maps 里 dmabuf=0），纯设备侧操作。
     要一致必须绕开 sail 直接用 bmcv/bmlib C 接口，属于另一个量级的改造。
- **是否更优**：两者都**不带来优势**，只是不同实现路径的副产物；实测内存 21.5MB dma、
  RSS 1.03GB（低于生产 1.65GB）、帧率相当，因此不动。

### 迭代 59 — 2026-11-14 — 修掉“HEVC bsf 粘连”导致 ISC 候选连续失败（预制场/排水渠1 恢复）

- **现象**：ISC 通道（预制场、排水渠1、乙烯南路2等）反复 connecting，但用 ISC 账号在本机直接
  `previewURLs` + ffmpeg 拉流都正常（预制场 st0=H.264 1280×720、排水渠1 st1=HEVC 640×360）。
- **根因**：迭代 47 加的 HEVC 直通标志 `gateway_bsf_hevc_` 是**每任务粘性**的——
  某个候选被判为 HEVC 后置 true，后续候选（哪怕是 H.264）也会带上 `-bsf:v hevc_mp4toannexb`，
  ffmpeg 立刻报错退出（`gateway exited before video`），于是整条候选链全废。
- **修复**：
  1. 每个候选开始前重置 `gateway_bsf_hevc_ = false`；只有该候选失败且日志确认是 HEVC 时，
     才对**同一条** URL 重试一次并置 true，重试后立刻复位；
  2. bsf 只在 `gateway_bsf_hevc_ && upstream_hevc_ == 1` 时启用；
  3. gateway 加 `-hide_banner`，错误信息不再被 ffmpeg 的编译 banner 淹没。
- **效果**：部署后 ISC 从 5 路 connecting 降到 **3 路**（预制场 1280×720、排水渠1 640×360 恢复），
  32 路 29 ok / 3 connecting。
- **剩余**：凤凰咀-游客中心/博物馆（生产侧实测同样 0fps）、乙烯南路2（ISC 的 st0=2560×1440 HEVC
  时好时坏，st1=640×360 可用，但我们的解码器偶发 `Failed to open input file`）。

### 迭代 58 — 2026-11-14 — 澄清 dmabuf 句柄数差异（868 vs 141）：任务数只是小头

- **实测**：我们 `dmabuf` fd **868**、mmap **622**；生产 fd **141**、mmap **0**。
- **拆解我们的 622 个映射**：586 个是 8~36KB 的小缓冲（4 组 `sail::MultiDecoder(10)` × 每通道
  约 15 个内部缓冲 ≈ 600），36 个是 676KB 的池化大缓冲（模型输入/预览/跟踪/瓦片，共约 21.5MB）。
- **为什么比生产多**：
  1. 任务数 32 vs 22（1.45×）——只解释一小部分；按同比例应是 ~190，实际 868，即每路 4.3×；
  2. 我们的 `MultiDecoder` 每组按 10 通道预分配（4×10=40 槽），每槽内部缓冲更多；
  3. `sail::BMImage` 会把 dma 缓冲 **mmap 到用户态**，生产只用 fd 句柄、`/proc/PID/maps` 里
     dmabuf 条目为 0 —— 这一条让“句柄/映射”数量看起来差得更多。
- **结论**：差异是**句柄/映射计数**，不是内存。dma 总占用仅 21.5MB，RSS 1.03GB 反而低于生产 1.65GB。
  若要做到数量接近，可把 `MultiDecoder` 的 max_channels 从 10 调到组内实际通道数（32 任务 → 4×8），
  约省 80 个句柄，但内存/性能无收益，暂不做。

### 迭代 57 — 2026-11-14 — 复测：所谓“4MB/min 泄漏”是预热期，稳态 RSS 仅 0.1MB/min

- **复测方法**：容器启动并完成预热（约 10 分钟）后，连续 4.5 分钟每 30s 采样 `/proc/<pid>/status` 的 VmRSS：

  | 时间 | RSS (kB) |
  |---|---|
  | 13:43:33 | 1,025,984 |
  | 13:46:03 | 1,026,396 |
  | 13:48:03 | 1,026,464 |

  4.5 分钟只涨 **0.5MB（≈0.1MB/min）**，属噪声；同时 `dmabuf` 713~744 持平、dmabuf 总大小恒 21.5MB。
- **结论**：迭代 55/56 记的“约 4MB/min 堆增长”是**启动预热**（池化缓冲首次分配、页缓存、
  分配器 arena 建立）造成的；池化改造后**稳态不再增长**，与生产“39.5h 不重启”的行为一致。
  对比：改造前同一测法（12:21→12:25）是 +22MB/4.5min（≈5MB/min），现在 0.1MB/min。
- **保留**：资源看门狗（dmabuf>1000 / RSS>2000MB 自重启）继续作为兜底，当前不会触发；
  `/proc/<pid>/smaps` 显示 `[heap]` 仅 684KB，增长点不在用户堆。
- **状态**：32 路 28 ok / 4 connecting（4 路 ISC 死链），`RestartCount=0`，部署 `aidetect_pool3`。

### 迭代 56 — 2026-11-14 — 跟踪中间图与合流瓦片也池化；缓冲模型与生产对齐完成

- **改动**（承接迭代 55）：
  1. `track_preview_boxes` 增加 `sail::BMImage& scratch` 参数，改由调用方用 `vpp_resize(in,out)`
     写入每任务复用的 `track_img_`（尺寸按“减半到 ≤1280”在调用方先算好，保证一致）；
  2. `render_tile_fast` 增加 `sail::Handle&` 参数，合流瓦片改用每任务 `tile_img_`（640×360 固定）；
  3. 至此热路径上**不再有每帧新建的 BMImage**：推理输入（u8+float32）、预览帧、跟踪中间图、合流瓦片全部复用。
- **验证（32 路在线）**：
  - `dmabuf` 句柄 **713~744 持平**；`dmabuf` 总大小 **21.5MB 恒定**（说明 BM 侧不再增长）；
  - RSS **855MB**（迭代 55 为 1.0GB，迭代 54 前为 1.9GB）；
  - 24 分钟内 `dmabuf` 无上升趋势；单通道 数智技术中心_通道 1280×720 @8.4fps、
    测试_2 3.0fps、合流 all_1 5.5~6.3fps；
  - 视觉验证：单通道（1280×720，ROI 正确）、合流九格（瓦片正常）、测试_2（1920×1080，
    框+多行标签贴人）均正常。
- **仍未解决**：RSS 仍有约 **4MB/min** 的匿名内存增长（`smaps_rollup` 的 Private_Dirty 为主，
  dmabuf 总大小不变 → 是用户态堆增长，不是 BM 缓冲）。按此速率约 5~8 小时到看门狗的 2GB 阈值，
  会被自动重启兜住；根因（疑似每帧某个 std::string/vector 未释放）留待下一轮用 heaptrack/二分定位。
- **部署**：`aidetect_pool3`，容器内 md5 `c02d68c2…`，`RestartCount=0`，28 ok / 4 connecting。

- **与生产的最终对比（同口径只读取证）**：

  | 指标 | 生产（32h+ 连续运行） | 我们（本轮池化后） |
  |---|---|---|
  | dmabuf 句柄 | 133 | 713~744（池化持有，持平） |
  | dmabuf 总大小 | — | **21.5MB 恒定** |
  | RSS / Pss | 1.65GB / 1.64GB | **0.89GB / 0.88GB** |
  | Private_Dirty | 1.54GB | 0.86GB |
  | 重启次数 | 0（39.5h） | 0（本轮） |

  按任务摊：生产约 75MB/路，我们约 28MB/路 —— 池化后内存占用已经优于生产；
  剩余差异只有那条约 4MB/min 的用户态堆增长（生产没有），由看门狗兜底。
### 迭代 55 — 2026-11-14 — 对齐生产的“池化缓冲”：模型输入/预览帧改为每任务复用

- **目标**：生产 `app` 连续 39.5h 不重启、dmabuf 恒 133，靠的是每通道池化复用帧；
  我们是“每次 resize 都新建 BMImage”，所以 15h 涨到 1359。本轮把热路径改成池化。
- **改动**：
  1. 新增 `pooled_frame(fmt,dtype,w,h)`：按 (格式,宽,高) 缓存推理输入图（u8 缩放图 +
     float32 RGB 图），人形检测与 4 个裁剪算法（helmet_v3 / boat / attr / behavior）全部复用；
  2. 预览帧改为每任务一个 `preview_img_`（`vpp_resize` 的 dst 版直接写入），
     工作线程改用借用指针 `preview_frame_ptr_`，不再 `make_shared` 一张新图；
  3. CPU 缩放退化为兜底（仅当环境变量强制非整数尺寸时）。
  依赖 sail 的 dst 接口：`vpp_resize(in,out,w,h)`、`vpp_crop_and_resize_padding(in,out,...)`。
- **验证（32 路、预览 1080p 上限）**：
  - `dmabuf` **稳定在 700~730 不增长**（改造前 610→1359/15h）；RSS 1.0 GB（改造前 1.9 GB）；
  - 检测框/标签正确（`p2_t2` 抓图：1920×1080，人形框+三行标签贴人）；
  - 测试_2 3.3fps、合流 all_1 6.2~6.8fps；28 ok / 4 connecting（4 路均为 ISC 侧死链）。
- **事故记录**：本次部署再次触发 KI-029（旧容器 PID1 僵尸、:9000 残留 LISTEN、`docker stop/rm` 挂死），
  杀 shim 后仍无法释放端口，最终 **重启盒子** 才恢复；重启后容器按 `--restart unless-stopped` 自动拉起。
- **仍未完全对齐**：生产 dmabuf 133，我们 ~700（池化按任务持有 + 跟踪/合流瓦片仍是按需分配）；
  RSS 仍有 ~5MB/min 缓慢上涨，未定位。下一步：把跟踪（`track_preview_boxes` 的 960/1280 中间图）
  与合流瓦片也改成池化，并继续查 RSS 增长。

- **跟踪/合流瓦片的池化：本轮尝试后回退**。给 `track_preview_boxes` / `render_tile_fast` /
  `render_preview_fast` 加 handle 参数并复用 scratch 的改动编译不通过（helper 插错位置、
  签名不匹配），为避免把正在跑的版本搞坏，源码已回退到本轮已验证的 pool2 状态
  （构建产物 `aidetect_pool2`、容器内 md5 `6b6fda73…`）。这一步留到下一轮用更小的改动重做。
### 迭代 54 — 2026-11-14 — 逆生产缓冲管理：生产无重启、dmabuf 恒 133；我们靠“重连风暴限速”止住增长

- **生产取证（ssh 只读）**：`ruijing.service` 里 `app` 进程连续运行 **1 天 15:31（39.5h）**、
  `NRestarts=0`（systemd 从未重启）；RSS 1.6GB、fd 409、**dmabuf 133 / ion 50**，长期持平。
  => 生产**没有**重启/清理机制，它只是**根本不累积**：解码帧来自每通道固定的
  `board::base_frame` 缓冲（`DecodeInstance` 池化复用），JPEG 直接从该帧编。
- **我方差异**：稳态 dmabuf 610~660（生产的 4.6 倍），且长时间运行会涨（15h → 1359）。
  拆解发现增长来自**失败重连的抖动**：3 台连不上的 ISC 相机每 10 分钟各走 28 次候选，
  每次都新建 gateway + 解码器尝试，失败后立刻重试（`open_fail_count_` 被重置），
  每次失败泄漏少量 BM 缓冲 → 约 +1.3 个/分钟。
- **修复（已上线 `aidetect_wd3`）**：解码器打不开编码（`Failed to open codec`）时，
  除了轮换 ISC 候选，还 **`++open_fail_count_` 并按 `open_backoff_ms()` 退避**（1.5s→3s→…→120s），
  不再立即重试。
- **效果（部署后实测 2.5 分钟）**：`dmabuf` 稳在 **604~618 不增长**（修复前 +1.3/分钟）；
  候选重试量从“单任务 28 次/10min”降到整体 **18 次/3min**；32 任务 29 ok / 3 connecting，
  ISC 25K 704×576 @8.8fps、合流 5.1~5.3fps。
- **仍未完全对齐**：生产是“池化复用、零分配”，我们仍是“按需分配 + 退避”，只是把增长压住。
  若要完全一致，需要把预览/裁剪的 `vpp_resize` 改成每任务固定缓冲复用（sail 暂无 dst 版 API，
  需用 bmcv 底层接口），这是下一步工作项。

### 迭代 53 — 2026-11-14 — ISC 再次全部无画面：BM 缓冲耗尽重演；看门狗阈值收紧 + 候选轮换

- **现象**：11-14 上午 32 路里 **10 路 connecting，全部是 ISC**（八所/预制场/25K/28K/14K/栈桥/
  排水渠1/乙烯南路2/凤凰咀×2），同一时间生产盒子这些 ISC 链接都有画面。
- **取证**：
  - 容器已连续运行 **15 小时**，`dmabuf` 涨到 **1359**（稳态 610~660），RSS 1.9GB；
  - 错误分两类：凤凰咀 `upstream no video`（ISC 返回 URL 无流，生产侧游客中心也无画面），
    14K `decoder open: Failed to open codec`（BM 池几乎耗尽时解码器打不开编码器），
  - 用 ISC 账号在本机直接 `previewURLs` + ffmpeg 拉流：25K/14K 的 st0=H.264 1280×720、
    st1=HEVC 704×576 都能解析出流 —— **ISC 侧通道本身是好的**；
  - **重启进程后立即恢复**：29 ok，ISC 全部回来（25K 704×576、预制场/28K/14K/栈桥 1280×720、
    八所 352×288、排水渠1 640×360）。
- **结论**：与迭代 52 同源——长时间运行后 BM(ion) 缓冲累积，先把 ISC 解码器拖死；
  上一版看门狗阈值（dmabuf>1500 / RSS>2600MB）设在故障点之上（实际 1359 就出事），所以没拦住。
- **修复（已上线 `aidetect_wd2`）**：
  1. 看门狗阈值收紧为 **dmabuf>1000 或 RSS>2000MB**（`AIDETECT_WATCHDOG_DMABUF`/
     `AIDETECT_WATCHDOG_RSS_MB` 可调），稳态 610~660，留足余量又能在黑屏前触发；
  2. **ISC 候选轮换**：gateway 有数据但解码器打不开编码时，`++isc_rot_` 并丢掉当前 gateway，
     下一轮从下一个候选（另一 streamType / 协议）开始，避免在同一条打不开的流上死循环。
- **验证**：28 ok / 4 connecting（凤凰咀-游客中心/博物馆=ISC 500、乙烯南路2、栈桥在重连），
  `dmabuf=658` 正常；25K 704×576 @7.8fps、预制场 1280×720 @6.4fps、合流 4.0fps；
  `resource watchdog` 未触发（说明当前健康）。
- **遗留**：BM 缓冲累积的根因仍未定位（怀疑 ISC 重连时旧解码器回收不全）；
  下一步：按子系统统计 buffer 增量、给 ISC 重连加最小间隔。

### 迭代 52 — 2026-11-13 — 大面积“没画面”根因：BM 缓冲池耗尽（不是缩放算法）；加资源看门狗

- **现象**：20:19 线上 32 路里 **19 路 connecting**（含数智/数值中心与多路 MP4 测试源），
  与“未缩放”时段对比明显变差；19:22 起各任务陆续超过 270s 无帧，supervisor 在 19:26 批量重启任务，
  但重启后仍拿不到帧，直到 **20:21 整体重启进程** 才恢复到 29 ok / 3 connecting。
- **排查结论（不是缩放公式的问题）**：
  - 故障时 `dmabuf` 句柄 **925**，正常启动后 117，稳态（含合流墙观看）约 **610~660**；
  - 故障期间 gateway ffmpeg 进程都还活着（16 个 Ssl 状态），但解码侧拿不到帧 —— 典型的
    **BM(ion) 缓冲池耗尽**：新解码器分配不到图像缓冲，任务就永远 connecting；
  - 重启*任务*无效（池子没释放），重启*进程*立刻恢复 —— 与观察一致；
  - 缩放后预览画面本身正常（2560×1440→1280×720 干净），合流墙连续看 2.5 分钟
    `dmabuf` 稳定在 610~660，不随缩放增长；RSS 在有人看时约 **22MB/min** 上涨（基线 4MB/min）。
  - 触发条件判断：ISC 相机 500/会话轮换导致的**重连风暴**（每轮都新建解码器）+ 长时间预览，
    把 BM 池顶满；ISC 的 500 是外部问题（生产侧游客中心同样无画面）。
- **临时对策（已上线）**：`main.cpp` 增加资源看门狗——每 30s 采样 `/proc/self/fd` 的 dmabuf
  数量与 VmRSS，超过阈值（默认 dmabuf>1500 或 RSS>2600MB，`AIDETECT_WATCHDOG_DMABUF` /
  `AIDETECT_WATCHDOG_RSS_MB` 可调）就记 WARN 并 `_exit(0)`，由 docker `--restart unless-stopped`
  在数秒内拉起，任务由后端对账自动恢复——比“耗死到 19 路黑屏”体验好得多。
- **待办**：定位 BM 缓冲泄漏的确切分配点（重点怀疑 ISC 重连时旧解码器回收不全），
  并给 ISC 重连加限速；看门狗只是兜底。
- **验证**：重启后 29 ok / 3 connecting（3 路均为 ISC 侧）；预览 1280×720 干净；
  `dmabuf` 稳态 610~660 不增长；部署 `aidetect_wd`。

### 迭代 51 — 2026-11-13 — 预览缩放全走 VPP：按整数倍缩到 1080p 以下（实测比“不缩放”更省）

- 用户要求不用 CPU 缩放：改为**整数倍 VPP**——选最小整数 k 使 w/k ≤1920 且 h/k ≤1080、
  且两维都能整除。实际映射：2560×1440→1280×720（÷2）、3840×2160→1920×1080（÷2）、
  4096×2160→1024×540（÷4）、3200×1800→1600×900（÷2）、≤1080p 不动。
  CPU `cv::resize` 只保留为环境变量强制非整数尺寸时的兜底。
- **三档实测（同一批约 30 路在线）**：

  | 策略 | 4K 单帧 `render_ms` | 合流 all_1 | 2.5K 单通道 | CPU us/sy/idle | 内存 |
  |---|---|---|---|---|---|
  | 不缩放（生产做法） | 160~288 | 3.4fps | 6.7fps | 20.2/24.3/52.9 | 1527MB |
  | 1080p + 非整数倍走 CPU | 39~215 | 4.7~5.1fps | 7.3fps | 21.5/25.9/49.7 | 1555MB |
  | **整数倍 VPP（本次）** | **42~79** | **7.0~7.3fps** | **8.9fps** | 19.1/25.9/52.6 | **1473MB** |

- **结论（回答“VPP 够不够”）**：整数倍缩放**比不缩放更省 VPP/JPU**——因为缩放后
  `convert_format` 与 JPEG 编码都在更小的图上做（2.5K→720p 少 4 倍像素，4K→1080p 少 4 倍），
  多出来的只有一次硬件 VPP 缩放。实测 CPU、内存、TPU 都没变化，合流反而翻倍到 7fps。
  “不缩放”并不是省资源：它仍然要用 VPP 做 YUV→BGR 转换、用 JPU 编码整帧，只是像素更多。
- **验证**：2560×1440 通道输出 1280×720（VPP ×0.5）干净；4K 通道 1920×1080 `render_ms=42~79`；
  32 任务 29 ok / 3 connecting；`RestartCount=0`；部署 `aidetect_intk`。

### 迭代 50 — 2026-11-13 — 预览花屏根因：VPP 非整数倍缩放；改为整数倍走 VPP、其余走 CPU

- **现象**：多路 2560×1440 相机（数智/数值技术中心）的预览出现大面积彩色噪点，
  单通道视图下半幅、合流瓦片几乎整幅花屏；704×576/1280×720 的 ISC 通道正常。
- **定位**：同一帧、同一代码路径，只有“非整数倍缩小”的通道花屏。对照实验：
  - 2560×1440 → 1280×720（×0.5，整数）**正常**；
  - 2560×1440 → 1920×1080（×0.75，非整数）**花屏**（底部一条整幅彩色噪声）；
  - 3840×2160 → 1920×1080（×0.5）**正常**。
  => `bmcv.vpp_resize` 在本 SDK 上只对**整数倍**缩小正确；非整数倍会读到错位的行。
  预览跟踪里的 2560→960（×0.375）同样是非整数倍，也在被污染。
- **修复**：① `maybe_preview` 只在 `w%tw==0 && h%th==0` 时用 VPP 缩放，否则保留源帧，
  由 `render_jpeg`/`render_preview_fast` 里的 `cv::resize`（CPU）完成非整数倍缩放；
  ② `track_preview_boxes` 的缩放改为“不断减半直到 ≤1280”，保证整数倍。
- **效果**（30 路在线）：花屏消失；合流 all_1 **2.9~3.3 → 4.7~5.1fps**、
  数智技术中心_通道 7.3fps、测试_2（4K→1080p）3.8fps。非整数倍那一路改用 CPU 缩放后反而更快
  （VPP 那条错误路径本身也慢）。
- **验证**：`docs/assets/preview_vpp_corrupt_before_20261113.jpg`（修复前）、
  `preview_vpp_fixed_after_20261113.jpg`（修复后 2560×1440→1920×1080）、
  `merge_tiles_clean_20261113.jpg`（合流九格正常）；32 任务 30 ok / 2 connecting，`RestartCount=0`。
- **部署**：`aidetect_vpp`。

### 迭代 49 — 2026-11-13 — 按房规恢复 1080p 预览上限；A/B 量化“省不省资源”

- 用户决定采用房规：源 ≤1920×1080 保持原分辨率；>1920×1080 等比缩到 1080p。
  `preview_target()` 默认回到该规则（`AIDETECT_PREVIEW_WIDTH/HEIGHT` 仍可覆盖）。
- **A/B 实测（同一台机器、同一批 30 个在线任务，原尺寸 vs 1080p 上限）**：

  | 指标 | 原尺寸（生产同款） | 1080p 上限 | 变化 |
  |---|---|---|---|
  | 4K 单帧 JPEG | 2.50 MB | **0.79 MB** | −68% |
  | 4K 预览渲染 `render_ms` | 160~288 | 39~215（中位约 110~190） | 约减半 |
  | CPU us/sy/idle | 20.2 / 24.3 / 52.9 | 21.5 / 25.9 / 49.7 | 噪声内，无收益 |
  | 内存 used | 1527 MB | 1518~1555 MB | 噪声内 |
  | 测试_2（4K）fps | 2.0 | 2.4 | +20% |
  | 数智技术中心_通道（2.5K） | 6.7 | 6.9 | 噪声内 |
  | 合流 all_1 | 3.4 | 2.6~2.8 | 噪声内 |

- **结论**：1080p 上限省的是**预览编码时间、JPEG 带宽、JPU/VPP 占用**（4K 帧小 3 倍、渲染减半），
  **不省 CPU 和内存**——整机瓶颈在解码+推理，不在预览编码。要真正降资源占用，
  杠杆是减 4K 源解码/推理（降检测频率或降源分辨率），不是压预览尺寸。
- **与生产的差异（知情选择）**：生产是原尺寸出图（ADR-087 的实测证据），我们按房规压到 1080p；
  差异只影响“一个人看单通道”时的清晰度与带宽，不影响检测/告警（检测用解码原帧）。
- **验证**：4K 测试源预览输出 1920×1080（787KB）、2.5K 源 1920×1080、25K 704×576；
  30 ok / 2 connecting；`RestartCount=0`；部署 `aidetect_cap`。

- **告警图片与预览解耦**：`save_alarm()` 不再用 `preview_target()`，改用源尺寸
  （生产也是用解码帧本身编告警图）。实测：2560×1440 相机触发告警 → 告警图片 **2560×1440**（895KB），
  同一路预览仍是 1920×1080；告警视频仍 `-c copy` 源分辨率。

### 迭代 48 — 2026-11-13 — 生产实测：>1080p 预览不缩放（2.5K 通道 2560×1440）；我方改为原尺寸

- **用户在生产上新加了一条 2560×1440 测试通道**（生产 `table_media` 里 media 名 `test`，
  URL `rtsp://admin:***@10.10.207.241/h264/ch1/main/av_stream`）。只读测量生产预览页：
  `img.naturalWidth×naturalHeight = 2560×1440`，连测两轮都是 **2560×1440 @ 5.00fps**，
  **没有缩到 1920×1080**。
- **反编译佐证**（`libEdgeSystem.so`）：`JpegEncoderInstance::onImagePoped(bool,
  shared_ptr<base_frame>)` 只做三件事——`MJpegInstance::anyWatcher()` 判断有没有人看、
  `board::create_jpeg_encode(w, h, q)` 用帧自身宽高建编码器、`board::frameToJpegByCpu(frame, q)`
  直接编码；**函数内没有任何 resize/缩放调用**，质量分支是 `cmp #2/#1 → csel 70/40`。
  之前找到的 1920×1080/1920×1088/1280×720 白名单在 `MediaInstance::onCodecDataArrived`，
  属于媒体/GB28181 接入校验，不是预览尺寸上限。
- **结论**：生产预览 = 解码帧原尺寸，**不放大也不缩小**；我上一轮说的“生产从未处理 >1080p”
  是因为当时生产没有 >1080p 的通道，现在有了反例。
- **我方修正**：`preview_target()` 去掉 1920×1080 上限，返回源尺寸（`AIDETECT_PREVIEW_WIDTH/HEIGHT`
  仍可强制指定，供 4K 测试机降档）。其余不变：每帧只缩放一次（本场景下退化为拷贝）、
  检测仍用解码原帧、合流瓦片仍 640×360。
- **实测对比（同一台 10.10.207.241 相机）**：生产 2560×1440 @ 5.00fps；我方 2560×1440 @ 5.0fps，
  截图 `docs/assets/preview_2k5_native_20261113.jpg`。
- **代价**：本地 4K 测试片源（3840×2160/4096×2160，共 12 路）现在也按原尺寸编码，
  33 任务下测试_2 降到 1.6fps、合流 2.0fps。真实相机最大 2560×1440，不受影响；
  需要压测 4K 时可设 `AIDETECT_PREVIEW_WIDTH=1920 AIDETECT_PREVIEW_HEIGHT=1080` 回到 1080p 档。
- **验证**：25K 704×576、数智技术中心_通道 2560×1440、测试_2 3840×2160 三种尺寸都取到且框/标签贴合；
  `RestartCount=0`、无 segfault；部署 `aidetect_native`。
- **生产只读**：只读 ssh 命令 + 只读 web 预览测量，未改任何文件/配置。

### 迭代 47 — 2026-11-13 — 逆生产取流：streamType 按通道配置、不自动切换；补 HEVC 直通

- **生产方式（反编译 + 只读取证）**：
  - `libEdgeSystem.so` 内的 `ISCStreamFactory::GetMediaUrl(RemoteUrl)` → `PostJson` + `makeSignedHeaders`
    → `/artemis/api/video/v1/cameras/previewURLs`，请求体 `"streamType": %d`；URL 模板
    `isc://user:pass@host:port?cameraIndexCode=..&streamType=0&protocol=rtsp&https=1&videotype=h264&transcode=0`。
  - 生产 `table_media.MediaUrl`（只读导出）实测：**streamType 逐路写死**，25K=`1`、预制场=`0`、
    八所_预制区=`1`。生产**不会**在主/子码流之间自适应。
  - 生产预览实测：25K=704×576、八所_预制区=352×288、预制场=1280×720，都是码流原始尺寸；
    二进制里与预览尺寸相关的只有 1920×1080 / 1920×1088 / 1280×720 的白名单比较
    （`MediaInstance::onCodecDataArrived`），没有“>1080p 降 720p”的分支。
  - 检测与预览**同一条流**：每通道一个 `DecodeInstance`，检测用解码原帧，预览用
    `requireScaled/notifyScaled` 的缩放帧经 `JpegEncoderInstance` 编码。
- **我方修正**：① ISC 候选顺序改回**配置 streamType 优先**（另一条仅兜底）；
  ② 补 HEVC 直通——gateway 日志识别到 `Video: hevc` 时在 `-i` 之后加 `-bsf:v hevc_mp4toannexb`，
  让主码流 H.265 能正确进 MPEG-TS；③ 预览上限维持 1920×1080。
- **结果**：25K 走主码流 **704×576（HEVC）**，与生产一致；整机 29 ok / 3 connecting；
  实测 25K 4.0fps（生产 4.83fps）、测试_2（4K→1080p）2.8fps、数智技术中心_通道 6.6fps、合流 all_1 3.5fps。
- **验证**：`docs/assets/preview_25k_704x576_20261113.jpg`；`RestartCount=0`、无 segfault；部署 `aidetect_hevc3`。
- **生产只读**：仅 ssh 只读查看与 scp 只读拷贝，未改生产任何文件/配置/服务。
- **遗留**：凤凰咀-游客中心/博物馆 仍为 ISC `DESCRIBE 500`（生产侧游客中心同样无画面）。

- **补充取证（streamType 语义逐相机不同）**：八所_预制区（02109e2f）在 ISC 上
  `streamType=0` 是 HEVC 3200×1800，`streamType=1` 才是 352×288（CIF）；
  生产该路配置是 1，所以生产预览是 352×288；我们按“配置优先、失败兜底”取到 0（3200×1800）。
  说明 streamType 的含义由相机/ISC 侧决定，不能当成“主=高清、子=标清”的固定假设。

### 迭代 46 — 2026-11-13 — ISC 无画面根因（主码流 HEVC）→ 改走子码流；预览恢复 1080p 上限

- **现象**：用户反馈“ISC 流都没画面”。逐任务核对：8 路直连 RTSP 相机
  （10.10.207.24x）全部 ok，**11 路 ISC（10.10.69.250）全部 connecting**，
  即故障只发生在 ISC 链路，不是取流框架坏了。
- **排查**（只读）：
  - `src/isc.hpp` 与迭代 42/43/45 三个快照逐字节相同（md5 `0A7AB353…`），
    **没有改过 ISC 取流代码**；server.hpp 的改动只涉及合流 JPEG 校验。
  - 在 .25 上直接用 ISC 账号调 `previewURLs`（只读接口）：解析成功，
    返回 `rtsp://10.10.69.250:554/openUrl/...`；用 ffmpeg 拉这条 URL 能出帧，
    但 `-c copy -f mpegts` 之后再读，HEVC 报 `Error parsing NAL unit #1`。
  - 容器里 25K 的 gateway 日志只有 `[hevc] PPS id out of range`，
    AI 侧 `frame watchdog: no first frame for 30s` —— 主码流是 **H.265**，
    这套解码链路解不出来；此前一直用“主码流优先”，于是任务永远起不来。
  - 生产预览 25K 的画面是 **704×576**、预制场是 1280×720，即**子码流**，
    说明生产预览走的就是 streamType=0（H.264）。
- **修复**：ISC 候选顺序改为**子码流优先**（`add(proto,0)` → `add(proto,1)` →
  另一协议 0/1），与生产一致；直连 RTSP 相机不受影响。
- **结果**：ISC 11 路里 7 路恢复出图（25K/28K/14K、预制场、钢栈桥、八所 1280×720，
  八所 3200×1800）；整机 21 ok/11 connecting → **28 ok/3 connecting/1 stalled**。
  仍失败的：凤凰咀-游客中心、凤凰咀-博物馆（gateway `DESCRIBE failed: 500`，
  生产侧游客中心同样 0fps）、乙烯南路2、排水渠1。
- **单通道预览分辨率恢复按规格**：`preview_target` 上限从 1280×720 改回
  **1920×1080**（源 ≤1080p 保持原分辨率，>1080p 等比缩到 1080p，告警视频仍用源分辨率）。
  上一轮降 720p 是误判——生产样例 1280×720 是因为那一路源就是 720p 子码流。
- **实测（29 路有图、预览 1080p 上限）**：合流 2.8~3.0fps、测试_2（4K→1080p）
  2.1~2.3fps、数智技术中心_通道（2.5K→1080p）6.3fps、25K（ISC 720p）8.3fps、
  预制场（720p）6.2fps。对比 720p 上限时合流 6.5~7.1fps——**1080p 预览的编码/缩放
  成本约为 720p 的 2 倍**，这是规格与帧率的直接取舍。
- **验证**：`preview_test2_1080p_20261113.jpg`（4K→1920×1080，框/三行标签贴人）；
  33 任务 running、`RestartCount=0`、无 segfault；`label not pre-rendered` 0 条。
- **生产只读**：只调只读接口与预览页，未改任何配置；本轮新增的 ISC 探查脚本
  也只调 `previewURLs` 并用 ffmpeg 读流（不写任何文件到生产）。
### 迭代 45 — 2026-11-13 — 测量方法纠正；检测裁剪改用缩放帧：33 任务合流 3.0→6.5~7.1fps

- **测量纠正（重要）**：迭代 44 报的“合流 5.6~7.0fps”是**任务刚重启、还在陆续上量**
  时测的（重启后 30~60s 内只有部分任务出图）。等 33 个任务全部 running、稳定 2~3 分钟
  后复测，同一版本（alg3）合流只有 **3.0~3.1fps**、测试_2 2.5~3.5fps。
  之后所有对比都在“启动后 ≥3 分钟”测，并连续取 3 次。
- **本轮改动（alg6）**：检测侧也改用缩放帧——当画面里有人要裁剪时，
  每轮先把源帧缩到 1280×720 一次，人形框换算到该坐标系后从缩放帧裁剪；
  人形检测本身仍用原图（框坐标不用换算）。无人可裁的任务不多付这次缩放。
- **同口径 A/B（33 任务、稳定后各取 3 次）**：
  | 指标 | alg3（仅预览缩放一次） | alg6（+检测裁剪用缩放帧） | 生产（22 任务） |
  |---|---|---|---|
  | 合流 all_1 | 3.0~3.1fps | **6.5~7.1fps** | 8.7~9.2fps |
  | 合流 all_2 | — | 5.2fps | — |
  | 测试_2（4K→720p） | 2.5~3.5fps | 2.5~3.2fps | — |
  | 数智技术中心_通道（2.5K） | 8.2fps | 8.1fps | — |
  | 安全帽检测（2.5K） | 8.5fps | 6.3fps | 4.9fps（720p 源） |
  - 单轮检测：`detect round ms=66`（base 22 / crop 44），比 alg3 又降一截；
    预览 `track_ms`（含 4K→720 缩放）120~225ms 仍是 4K 源的主要成本。
  - 结论：**合流提升一倍多**，4K 单通道持平，2.5K 单通道略降（负载从 33 路竞争里分摊），
    仍是“33 任务 vs 生产 22 任务”。保留 alg6 并部署（`aidetect_alg6`，md5 `8b6f42a8…`）。
- **生产侧事实核对**：用同一只读账号拉 `alg_task_fetch`，生产当前配置 **22 个任务**
  （不是 36），实时预览页也列出 22 个任务通道；生产合流 8.7~9.2fps 是在 22 任务下测的。
- **验证**：33 任务全部 running、`RestartCount=0`、无 segfault/Jpu、
  `label not pre-rendered` 0 条；测试_2 框/三行标签贴人
  （`preview_test2_scaled_crops_20261113.jpg`）；构建 `[100%] Built target aidetect`。
- **生产只读**：只调只读接口/看预览，未改任何配置或文件。
- **遗留**：合流 6.8 vs 生产 9.0。注意现场当前 33 个任务里只有 **21 个 health=ok**
  （6 connecting、6 stalled，是项目部网络/ISC 侧问题），生产是 22 个任务，
  所以这是“21 路有图 vs 22 路有图”的同量级对比，差距约 25%。
  4K 源单通道 2.5~3.2fps 受 4K→720 缩放与 33 路解码限制；要进一步缩小差距，
  需要按任务数自适应降 4K 源的预览/检测分辨率，或把缩放放到解码输出
  （需要改网关，风险更高）。

### 迭代 44 — 2026-11-13 — 按算法节奏 + 每帧只缩放一次（注：当轮 7.0fps 为上量期读数，稳定值见迭代 45）

- **背景**：迭代 43 已把推理排队消掉（cap=4 时 `wait_avg=0`），但 33 任务时
  合流仍只有 2.8~3.0fps、单通道 2.4fps。本轮按生产结构（每算法独立流水线 +
  `DecodeInstance::requireScaled` 缩放帧）继续重构，先量化再改：
  - profile 实测（07:27）：测试_2 预览每帧 `track_ms=109`（4K→960 灰度 + LK）、
    `copy_ms=10`（4K 拷贝 24MB）、`render_ms=30~268`（4K→1280 缩放 + BGR + 编码）。
    同一帧里把 4K **缩放了三次**（跟踪、单通道、瓦片），且都抢 VPP。
- **改动 1：每算法独立节奏（生产 `onAllAlgoSkipped` 的落地）**
  - `ModelRuntime::min_interval_ms/last_run_ms`：裁剪算法按各自间隔跑，
    `AIDETECT_ALG_INTERVAL_MS`（默认 400ms），`boat ×1.25`、`behavior ×1.5`；
    基础人员模型仍是每轮。到点才跑，没到点直接跳过。
  - 效果：测试_2 单轮 `103~617ms` → `56~345ms`；推理次数
    boat 48→30、helmet 49→31、vest 50→32（每 5s），总需求下降约 20%。
  - 预览跟踪的“属性漏检保留”窗口改成 `max(600, 最大算法间隔×1.5)`，
    避免算法跳帧时标签闪烁。
- **改动 2：每帧只缩放一次（生产 `requireScaled` 的落地）**
  - `maybe_preview` 先用 `vpp_resize` 把源帧缩到预览尺寸（1280×720），
    跟踪、单通道渲染、合流瓦片**都用这一张缩放帧**；检测框坐标在进入跟踪前
    统一换算到预览坐标系。删掉 4K 拷贝与渲染里的第二次缩放。
  - 效果：`render_ms` 30~268 → **20~67ms**；跟踪+缩放 5~195ms（VPP 竞争时波动）。
- **实测（33 任务并行，与生产同口径）**：
  | 项 | 迭代 43 | 迭代 44 | 生产（22 任务） |
  |---|---|---|---|
  | 合流 all_1 1920×1080 | 2.8~3.0fps | **5.6~7.0fps** | 8.7~9.2fps |
  | 单通道 测试_2（4K→720p） | 2.4fps | **3.2~3.7fps** | —（生产该路为 720p 源） |
  | 单通道 数智技术中心_通道（2.5K） | ~8fps | **6.9~7.9fps** | 4.9fps（720p 源） |
  | 单通道 安全帽检测（2.5K） | — | **7.0fps** | — |
  - 结论：**2.5K/720p 单通道已经不低于生产（7fps vs 4.9）**；合流 7.0 vs 9.0，
    但我们是 33 任务、生产是 22 任务，按任务数归一后基本对齐。4K 测试源
    （12 路 4096×2160 MP4）是剩下的主要成本，真实相机多为 1080p。
- **验证**：33 任务全部 running，`RestartCount=0`，无 segfault/Jpu；
  测试_2 框/标签贴人（`preview_test2_720p_20261113.jpg`）、
  b062 ROI 线位置正确（`preview_b062_720p_20261113.jpg`）、
  合流 9 格坐标无错位（`merge_all1_tiles_20261113.jpg`）；
  构建 `[100%] Built target aidetect`；部署 `aidetect_alg3`。
- **生产只读**：本轮未再登录生产，沿用迭代 43 的实测数据；未改任何生产配置。
- **遗留**：合流 7.0 vs 生产 9.0（任务数不同）；4K 源单通道 3.7fps 仍受
  “4K→720 VPP 缩放 + 33 路解码”限制。若要继续压缩，下一步是把解码侧
  `requireScaled` 前移到解码输出（网关就出 720p），而不是预览线程缩放。

### 迭代 43 — 2026-11-13 — 标签空行根因；标签按开关动态生成 + 底板预渲染；生产调度实测对照

- **需求 1（标签之间空一行）**：根因是**行为模型上报的框是 1.10 倍扩边后的裁剪框**，
  比 vest/boat/helmet 的人形框大约 6px，于是它自己的标签栈从更高的位置开始，
  两栈之间就露出 6px 空档（不是“给未戴安全帽留位”——我们不会为空标签占位）。
  修复：行为检测框改回 `p`（人形框），扩边只用于模型输入裁剪。实测两栈间隙
  6px → 2~3px（等于板间 1px 抗锯齿），图 `preview_test2_labels_flush_20261113.jpg`。
- **需求 2（标签文案 + 底板）**：
  - `ModelRuntime::display_label`：按任务**实际开启的行为开关**拼标签——只开抽烟
    → `抽烟`；只开打电话 → `打电话`；只开玩手机 → `玩手机`；开两个以上 →
    用 `/` 连接成一个标签。检测到哪一类都沿用同一个拼好的标签（生产同一目标
    只出一组标签）。
  - 底板改为**预渲染资源**：新增 `tools/gen_label_plates.cpp`，用与运行时相同的
    字体/字号把 16 个标签（含 `抽烟`、`打电话`、`玩手机`、`抽烟/打电话`、
    `抽烟/玩手机`、`打电话/玩手机`、`抽烟/打电话/玩手机`、`未戴安全帽`、
    `未穿反光衣`、`未穿救生衣`、`人员聚集`、`人数监控` 等）渲染成 PNG +
    `docker/labels/labels.json`。运行时只读清单 + PNG，**不再在任务启动时渲染**；
    缺清单里的标签才回退现画并打 WARN。日志实测
    `label plates from manifest=16`、`label not pre-rendered` 0 条。
  - 说明：测试_2 的算法配置里 `开启抽烟/打电话/玩手机` 三个开关当前都是开（DB
    `behavior_enable_*` 均为 1），所以画面显示 `抽烟/打电话/玩手机`；在
    “任务管理 → 编辑 → 算法配置”里关掉打电话/玩手机后就会变成 `抽烟`。
- **需求 3（模型调度与生产对照）**：
  - **生产实测**（10.10.207.46，网页只读登录，未改任何配置）：22 个任务；
    合流 1920×1080 **8.7~9.2fps**；单通道 **1280×720，4.9fps**；
    截图 `docs/assets/prod_preview_channels_20261113.png`、
    `prod_preview_single_20261113.png`。说明生产也不是“全 10fps”，
    合流约 9fps、单通道约 5fps。
  - **我们的**（33 任务，比生产多 11 个）：合流 2.5~3.0fps、单通道 2.0~2.5fps。
    `infer stats`：约 104 次推理/s，cap=2 时 `wait_avg=13~19ms`、
    测试_2 单轮 103~617ms；**cap=4 时 wait 降到 0、单轮 72~399ms、
    合流 2.4→3.0fps**，因此并发默认从 2 调成 **4**（`AIDETECT_INFER_CONCURRENCY`）。
  - 并发语义：cap=N 是“同时进行的推理上限”，不是任务数阈值——开 1 个任务
    只会用 1 路；只要**有两个任务的推理请求同时在飞**，第 2 路就会被用上。
    36 任务时 cap=4 的容量约 140 次/s > 当前 33 任务实测需求 104 次/s，推理
    不会排队；剩下的延迟来自 33 路解码 + VPP/JPU 与预览共享硬件，不是调度器。
  - 顺带对齐生产：单通道预览上限从 1920×1080 改为 **1280×720**（生产同款），
    省 2.25 倍 VPP/JPU；并加“管道积压时自动降检测节奏”（`ModelScheduler::load()`，
    生产 `onAllAlgoSkipped` 的等价物，最多 3 倍间隔）。
  - **仍未对齐**：生产合流 9fps vs 我们 33 任务 3fps。差距不在推理闸门，
    在“33 路解码 + 36 路算法 VPP 预处理 + 预览/合流编码”抢同一套 VPP/JPU。
    下一轮按生产 `ImageInference` 每算法独立实例 + 每算法固定节奏重构，
    并把 33 任务的检测需求压到 ~50 次/s（当前 104 次/s）。
- **验证**：
  - 33 任务全部 running；`RestartCount=0`；无 segfault/Jpu。
  - 标签：三行贴框、无空行、文字完整（`preview_test2_labels_flush_20261113.jpg`）。
  - 底板：`label plates from manifest=16`，无 `label not pre-rendered`。
  - 帧率/推理：见上；`peak_pending=8~10`、cap=4 时 `wait_avg=0`。
  - 构建：`cmake --build build/aidetect -j8` → `[100%] Built target aidetect`；
    部署 `aidetect_lab19`。
- **生产只读**：仅用网页账号登录查看预览/任务列表并读接口（`alg_task_fetch` /
  `alg_ability_fetch`），没有改任何配置、没有写生产文件。
- **事故与恢复**：部署时用 `docker restart` 在 33 任务下把容器 PID1 变成 zombie、
  `:9000` 被孤儿 socket 占住（KI-029 复现）。处理：杀卡住的 docker 客户端 + shim →
  `rm -f` → 新建容器 → **重启盒子**释放 socket；之后改成
  `docker create → cp → stop → rm → rename → start`，不再用 restart。
- **遗留**：合流/单通道帧率仍低于生产（见 KNOWN-ISSUES KI-032）。
- **更正**：过程中几次“预览横向撕裂”截图，实际是**裁剪工具把 1920 宽的坐标用在
  1280 宽的新预览上、越界后被边缘像素拉伸**造成的测量假象，预览本身正常
  （按 1280×720 重新裁剪即无撕裂）。KI-033 已删除。

### 迭代 42 — 2026-11-13 — 标签末字被裁修复；33 任务容量实测（并发 2/4 无差别）

- **现象**：迭代 41 的截图里 `抽烟/打电话` 底板末字被裁（“话”只显示一半），
  `未穿救生衣/未穿反光衣` 正常。原因是底板尺寸用 `getTextSize()` 估算：
  `197x34`（宽高比实际绘制结果大/小不一致），实际字形比估算更高，且右边距不足。
- **修复**：`ensure_label_plate` 改为**先画再量**——在 96x640 红底画布上
  `putText`，用 `inRange(白)+boundingRect` 取真实字形框，四周各留 8/4 px，
  裁出底板。实测 `抽烟/打电话` 底板变成 `182x39`（原来是 197x34），
  三行标签都完整可读（图 `preview_test2_label_full_text_20261113.jpg`，
  修复前 `preview_test2_label_clipped_before_20261113.jpg`）。
- **33 任务容量实测**（用户现场把全部任务都开起来了，比需求里的 15 个更极端）：
  - 运行态：33 running（含 15 个测试源 + 18 个项目通道），CPU 64%、TPU 30%、
    温度 53/61℃、`aidetect` 单进程 258% CPU；`RestartCount=0`、无崩溃。
  - 推理：约 400 次/5s（people 157 + people_big 124 + boat/helmet/vest 各 50 +
    gather 25 + smoking 9），`wait_avg=0`、`peak_pending=5~9`，
    **闸门没有排队**——瓶颈不在调度器。
  - 单任务检测轮：测试_2 4K 为 **199~836ms**（15 任务时 22~32ms），
    预览 2.1fps（15 任务时 5.8fps）；合流 2.3~2.6fps。
  - 并发 A/B：`AIDETECT_INFER_CONCURRENCY=4` 与 2 在 33 任务下都是
    `wait_avg=0`、帧率 2.1/2.6fps、检测轮 199~684ms，**没有提升**；
    说明瓶颈是 33 路解码 + VPP/JPU 共享硬件，而不是推理准入。
  - 结论：**默认保持并发 2**；15 任务以内表现良好，30+ 任务是这颗盒子的容量上限，
    要再上量需要减算法/减任务或换更高算力盒子（已记 KNOWN-ISSUES KI-031）。
- **验证**：33 任务下抓帧确认三行标签完整、贴框（
  `preview_test2_label_full_text_20261113.jpg`）；`RestartCount=0`、无 segfault/Jpu；
  构建 `[100%] Built target aidetect`；部署 `aidetect_lab13`（md5 `fbcf2004…`）。
- **生产只读**：无生产访问，未改生产任何文件/配置。

### 迭代 41 — 2026-11-12 — 修复「检测标签无字」；抽烟标签可确认；待确认框只有一个；并发 A/B

- **现象（需求 2 的“标签无字”）**：15 任务并行时测试_2 的框上方会出现**纯红色、
  没有任何文字**的标签条（图中位于框顶，和“未穿救生衣/未穿反光衣”叠在一起）。
  逐帧统计（`node .work/scan_text2.js`）在改造前 15 帧里命中 15 个
  `179x33 white=0.00%` 的纯红条。
- **根因（三处叠加，都已修）**：
  1. 标签底板只有 6 张 PNG（未戴安全帽/未穿反光衣/未穿救生衣/未穿安全帽/抽烟/人员），
     而算法实际上报的 `report_content` 是 **抽烟/打电话**、**人员聚集**、**人数监控**，
     没有底板 → 走 FreeType 现画；
  2. FreeType 现画时用了 `cv::Point(4, bh - 4)` 作原点，而 OpenCV 的 FreeType2
     `putText` 以**左上角**为原点（`task_bar_mat` 早已踩过同一个坑并修好），
     文字被画到底板下方 → 只剩纯红条；
  3. 快速预览路径 `render_preview_fast` 只画底板，没有底板就**完全不画标签**，
     所以“抽烟”确认后连红条都没有。
  4. 另外：`update_alarm` 的目标 1s 没命中就删除，而行为模型每轮在
     吸烟/打电话/玩手机之间跳类，间隙常 >1s → 永远到不了 5s 持续时长，
     抽烟标签从来没进过 confirmed；预览轨迹又要求 `class_id` 相同才匹配，
     类别一跳就新建轨迹 → 抽烟框活不过 600ms。
- **修复**：
  - 新增 `Task::ensure_label_plate(text)`：任务启动（和热更新）时，为本任务所有
    算法的 `report_content` 现渲染红底白字底板（FreeType 左上角原点 `(6,3)`），
    存进 `label_plates_`；快速预览路径直接贴底板，不再依赖 PNG 名单。
  - FreeType 兜底 badge 的原点改为 `(4, 2)`（与任务编号条一致），并加
    `AIDETECT_PROFILE` 下的 `label badge text=...` 日志用于定位。
  - 目标存活 TTL 从 1s 改为 `AIDETECT_TARGET_TTL_MS`（默认 **2000ms**），
    让间歇性行为模型能在 2s 间隙内累积到持续时长；预览幽灵框仍由跟踪器
    600ms 人形锚点宽限控制，不依赖该 TTL。
  - 预览轨迹关联改成**只比 label**（`抽烟/打电话` 的 class 会跳），不再比 class_id。
  - 同一目标被多算法命中时，聚类后**只保留一种状态**：有 confirmed 就只画红色标签，
    否则只画一个蓝色倒计时（之前会同时出现蓝框 + 红标签 + 多个“3s/5s”数字重叠）。
  - 合流瓦片：编码后校验 JPEG 的 SOI/EOI（`jpeg_markers_ok`），不合格直接丢弃，
    合成端解码失败时保留上一帧瓦片而不是贴纯色（KI-030 的防护）。
- **验证**：
  - 15 任务并行、测试_2 4K：框顶标签依次为 **抽烟/打电话 / 未穿救生衣 /
    未穿反光衣**，三行都有字、贴着同一个框（图
    `preview_test2_smoking_label_20261112.jpg`）；待确认目标只有一个蓝色
    “5s”倒计时（图 `preview_test2_pending_countdown_20261112.jpg`）。
  - 日志：`label plate rendered: 抽烟/打电话` / `人员聚集`，`label plates=8`
    （原来 6），`label badge text=` 不再出现（不再走兜底）。
  - 帧率（10s）：测试_2 单通道 5.8fps、all_1 合流 5.1fps、15 任务全部 running；
    `grep -icE "segfault|terminate|Assertion|Jpu_GetError|double free"` = 0。
  - 并发 A/B：`AIDETECT_INFER_CONCURRENCY=2` 与 `=4` 在 15 任务下都是
    `wait_avg=0 / peak_pending=3~4`、帧率 5.2~5.7fps，**没有差别**（需求远低于
    容量），因此保留默认 2（更省 TPU/内存）。
  - 构建：`cmake --build build/aidetect -j8` → `[100%] Built target aidetect`；
    部署 `aidetect_sched11`。
- **生产只读**：本轮只在开发工作区的 `.work/libEdgeSystem_prod.so` 上做符号/反编译
  读取，没有连生产、没有改任何生产文件或配置。
- **遗留**：合流瓦片花屏的**根因**未定位（现在只是丢弃坏瓦片/保留上一帧），
  需要时可对瓦片 JPEG 做逐字节校验或复现压测；KI-030 继续跟踪。

### 迭代 40 — 2026-11-12 — 推理调度改公平并发闸门；15 任务下测试_2 恢复出框、标签合一

- **现象（需求 1/3 复现）**：15 个任务同时 running 时，测试_2（4K、5 个算法 runtime）
  预览里**一个框都没有**；只开 4 个任务时正常。改造前 profile：单次推理 187ms，其中
  **172ms 在等全局锁**，TPU 真正计算只有约 15ms，同时排队 15 个请求；测试_2 一轮
  `detect round` **1703ms**，两轮间隔约 3s。预览端「检测快照超过 1.5s 隐藏」的保护因此
  把框全清掉 → 这就是“15 个任务时检测不出目标”。4 个任务时同一把锁只有 4 个竞争者，
  测试_2 能拿到 1/4 的推理机会，所以看起来正常。
- **生产逆向（只读，未改生产任何文件）**：`ImageTaskInstance::workLoop` +
  `onInferenceFinished(Json::Value&, shared_ptr<void> const&)` 说明每个任务有独立
  workLoop，`sdkImageInference(...)` 以 **frame + regions + 完成回调** 异步投递；
  `ThreadSafeLazyQueue<shared_ptr<FramePackage>>::Push` 是“只留最新帧”的队列；
  `FramePackage::algoSubmitted` + `TaskInstance::onAllAlgoSkipped` 说明**某帧算法跟不上
  就整帧跳过**，不是把请求堆进全局队列。证据与命令：
  `docs/assets/prod_infer_scheduler_syms_20261112.txt`。
- **修复**：
  - `ModelScheduler::infer` 去掉全局 `infer_mu_`，改 **FIFO 票据闸门**（`enter_queue`/
    `leave_queue`，严格按到达顺序准入），并发数 `AIDETECT_INFER_CONCURRENCY` 默认 2；
    实例池 `max_instances = max(1, 并发/2)`，避免同模型的不同任务挤在一个 engine 锁上。
  - **需求上限**（生产 `onAllAlgoSkipped` 的等价物）：每任务检测节奏
    `AIDETECT_DETECT_INTERVAL_MS` 默认 300ms（4K 源再 ×1.5）；裁剪算法只处理
    `AIDETECT_CROP_PERSONS`（默认 4）个最大的人形框；`AIDETECT_ATTR_MODELS_PER_ROUND`
    可把裁剪算法改成轮转（默认全部，保留开关）。
  - **标签合一**（需求 2）：同一目标被多个算法命中时，预览输出按 IoU≥0.45 聚类，
    取最大框作为公共框、每个算法各自出标签，渲染成“一个框 + 竖排多标签”。
    改造前每个算法各画一个略有偏移的框，就是“标签乱飞/嵌套框”。
  - **框不闪**：某个快照没有属性命中时，只要还有全帧人形框锚点就保留轨迹 600ms，
    不再因一次漏检立刻清框；整帧既无属性也无人员时不判定目标消失。
  - 合流瓦片单独令牌桶 `tile_interval_ms_`（默认 120ms），不再和单通道抢同一个
    10fps 预算。
- **验证**：
  - 15 任务并行、测试_2 4K：`detect round ms=22~32`（改造前 1665~1703），
    `infer stats` 等锁 0ms、`peak_pending=3~4`（改造前 15）；
    图 `preview_test2_boxes_15tasks_20261112.jpg`（框 + 竖排“未穿救生衣/未穿反光衣”）、
    `preview_test2_follow_20261112.jpg` / `..._follow2_20261112.jpg`（间隔约 0.5s 的两帧，
    框始终贴着白衬衫行人）。
  - 同条件改造前抓帧 `preview_test2_starved_15tasks_20261112.jpg`：画面无任何框。
  - 合流 `merge_all1_15tasks_20261112.jpg`：9 格正常，测试_2 格内有框。
  - 帧率（`node .work/browser/count_ws.js`，10s）：单通道测试_2 5.2~6.4fps
    （改造前 6.4~6.7），all_1 合流 4.9~5.2（改造前 4.7~4.9），
    数智技术中心_通道 9.9/9.9。数字与命令见
    `docs/assets/infer_scheduler_measure_20261112.txt`。
  - `/debug/scheduler`：6 个模型各 1 实例、`busy=0`；15 任务全部 `health=ok`。
  - 稳定性：连续运行期间 `grep -icE "segfault|terminate|Assertion|Jpu_GetError|double free"`
    = 0；容器 `RestartCount=0`。
  - 部署：`aidetect_sched6`（md5 `d2615dd2095fe6be60d3b067b0eca911`）已进
    `aidetect:current` 容器，用与 `_recover_aidetect.sh` 相同的 env/binds 重建
    （不带 `AIDETECT_PROFILE`/`AIDETECT_INFER_CONCURRENCY`，验证默认值即可工作）；
    重启后 15 任务 running、测试_2 6.1fps、合流 4.8fps、画面有框。
  - 构建：`cmake --build build/aidetect -j8` → `[100%] Built target aidetect`。
- **生产只读**：只在 `.work/libEdgeSystem_prod.so`（既有分析副本）上跑
  `node .work/parse_elf.js` 与 Ghidra headless，没有连生产、没有拷贝/改/重启生产进程。
- **代价/边界**：
  - 并发=2 是按实测选的保守值；`AIDETECT_INFER_CONCURRENCY` 可调大，但 bmcv/JPU 是
    共享硬件，调大不一定更快。
  - 裁剪算法只跑最大的 4 个人形框；画面里同时超过 4 个目标时，最靠后的小目标这一轮
    不做属性判定（基础人员框仍在，下一轮会轮到）。
  - 4K 源检测节奏 450ms（约 2.2Hz）；框间靠预览 LK 跟踪，形态上仍不是 10fps 推理。
- **遗留**：合流瓦片偶发一次彩色花屏（60 帧里 1 帧，单通道正常，见 KNOWN-ISSUES
  KI-030），与本轮调度改动无直接关系，另开条目观察。

### 迭代 39 — 2026-11-12 — 预览框按检测帧绑定；取消 ROI 边缘 1s 残框

- **现象**：合流/单通道里检测框在检测间隔内被“最新检测”按到当前帧，人走动
  时框慢半拍、向右/向上漂；人走出绿色 ROI 后，`rt.active` 的 1s TTL 仍把
  最后一个边缘框/标签留在空过道上（图 1）。
- **生产逆向（只读，未改生产）**：`FramePackage::setBoxes(frameId, boxes)` /
  `getBoxes(frameId)` 按帧号存框；`board::track::IoUTracker::process` 只维护
  id；`TrackBoxesByHandler` 把同一帧的框交给消费方。证明生产不是“最新检测
  结果套任意预览帧”。证据与命令见
  `docs/assets/prod_box_frame_binding_20261112.txt`。
- **修复**：
  - `PreviewTrack` 保存 `(ts, box)` 最近 3s 历史；新检测到达时按 `dets_ts`
    找轨迹在那一帧的姿态，只把检测框与该帧姿态的增量修正到当前帧，绝不把
    框重置回旧帧；
  - 帧间 960 宽灰度 LK（前后向校验；VPU 先 `vpp_resize`，不再 CPU 转
    2560×1440 全帧）+ LK 失败时内框 `matchTemplate` 兜底 + 短时速度外推；
  - `dets_persons_` 保存全帧人员框（不画）；属性算法漏检时把属性框吸附到
    人员框在检测帧的位置，避免标签留在空过道；
  - 每预览帧按框中心判断 ROI，出线当帧删除；最新检测快照无匹配且无人员
    锚点的属性轨迹删除；删除 `rt.active` 1s 缓存；
  - 预览闲置 >1.5s 清轨迹，避免重新打开页面时对旧帧做光流。
- **验证**：
  - 用 20s 事件片段（12:26:38–58 的 b062 通道）临时回放：12:26:44–47 框
    紧贴白衬衫行人随其向镜头走；右侧已在 ROI 外的第二名人员无框；12:26:48
    边缘框仍在，12:26:49 已消失（不再停留数秒）。图：
    `preview_track_follow_20261112.jpg`、`preview_track_two_people_20261112.jpg`、
    `preview_track_edge_gone_20261112.jpg`。
  - 最终 `aidetect_track16`（md5 `af3271e6de6f02b418a0f315ed30d3a8`）已
    部署 `.25`；重启后 29 路任务 running，b062 `health=ok`、
    `2560x1440`；无 `preview worker/queue` 异常。冷启动后复测单通道
    `87/10036ms=8.7fps`、合流 `87/10039ms=8.7fps`。
  - track16 现场复验：13:27:47 b062 通道一名人员走到 ROI 顶端，框紧贴人员，
    同时叠 3 个算法标签；13:27:48 人仍在框内、框仍贴人。图
    `preview_track_live_20261112.jpg`、`preview_track_live_edge_20261112.jpg`。
  - track16 边缘复验：13:30:47 蓝框仍贴 ROI 顶端人员；下一帧人员已出线，
    画面为空过道且无任何框/标签（`preview_track_edge_last_box_20261112.jpg`
    → `preview_track_edge_cleared_20261112.jpg`）。不再有 1s 边缘残框。
  - 帧率（`node .work/browser/count_ws.js`，10s）：单通道
    `85/10032ms=8.5fps`、合流 `77/10028ms=7.7fps`；优化前单通道
    `44/10025ms=4.4fps`。36 路并发、源 25fps 下 10fps 令牌桶上限。
  - 构建：`source env/env.sh && cmake --build build/aidetect -j8` → 100%。
  - 文档：`pwsh -File skill/project-memory/scripts/check-project-memory.ps1
    -Target gh-aibox-dev -DocsRoot docs` → 8 通过 / 3 行数警告 / 0 失败。
- **生产只读**：生产 `.46` 上只执行了 `readelf/nm/objdump/strings` 与 `ls/cat`，
  没有拷贝、写入、安装或重启生产进程；分析副本是开发工作区里已有的
  `.work/libEdgeSystem_prod.so`。
- **现场操作说明**：为可控复验，曾把 b062 的通道地址临时指向
  `.../media/1670034621_aisle_event.mp4`；复验后已改回原 RTSP。该容器
  在切回时出现 PID1 zombie（defunct，端口 9000 被旧 socket 占住），
  `docker restart` 卡住；最终 `kill` shim + `docker rm -f` + 用原 env/binds
  重建 `aidetect:current` + 拷入 `aidetect_track15` + 重启恢复。后续不要
  再用“改通道地址 + restart”的方式做回放，改用独立临时任务。
- **遗留**：底层属性算法仍约 1s 一次，远小目标的人形框本身偏大/偏移；框会
  跟着人形框偏。需要更高检测节奏或更紧的人形框裁剪时再对比。

### 迭代 38 — 2026-11-12 — 预览框逐帧光流跟踪；ROI 边缘残框 1s 消失

- **问题**：检测框只在检测频率更新，视频预览是 10fps，人移动时框看起来
  跟不上；人走出 ROI 后，最后一个边缘框/标签会停留几秒。
- **生产逆向（只读）**：`FramePackage::setBoxes/getBoxes`、
  `board::track::get_iou_tracker`、`TrackBoxesByHandler`，生产把框绑定到
  帧并在帧间做跟踪。
- **修复 1（逐帧跟踪）**：`Task::track_preview_boxes` 在预览线程对每个
  框做 `cv::calcOpticalFlowPyrLK`（640 宽灰度、4x4 点阵、中值位移），
  同一检测快照之间每帧移动框；新检测仍以检测框重置。CMake 增加
  `opencv_video`。
- **修复 2（ROI 边缘残框）**：`rt.active` 保留时间 5s→1s；人离开 ROI 后
  最迟 1 秒清掉最后边缘框，不再停留几秒。
- **验证**：
  - 6488 单通道 10:37 抓图，人物/框/ROI 正常，无花屏
    （`preview_6488_tracking_20261112.jpg`）；
  - 预览线程无异常日志，框跟踪开启后仍正常出图；
  - 光学流只在有观看预览时运行，未影响 36 路检测主循环。
- **说明**：光流只改善“检测间隔内的框跟随”，检测本身仍是 AI 推理频率；
  若人快速变向，框最多跟到光流可估计的范围。
- **命令**：`cmake --build build/aidetect -j8` → 100%；`aidetect_boxtrack`
  已部署 `.25`。

### 迭代 37 — 2026-11-12 — 预览框按帧时间对齐；MP4 通道循环播放

- **检测框同步**：生产 `FramePackage::setBoxes/getBoxes` +
  `TrackBoxesByHandler` 把框绑定到帧；我们原来只用“最新一次检测”的框，
  推理完成较新的帧时会把未来框套到较旧预览帧，造成框跳/滞后。
  现在 `dets_history_` 保存最近 20 次 `(frame_ts, boxes)`，预览选择
  **不晚于当前预览帧时间** 的最新快照，再用 `vx/vy` 外推到 `now`；
  仍保留 1.5s 外推上限，避免长时间无检测时漂移。
- **MP4 通道**：
  - 新增前端协议选项 `MP4`（伪值 99，保存时转后端合法的“手动填写”0，
    编辑时按 `.mp4` 地址还原）；
  - 选择 MP4 后视频地址变为上传栏，AI 新增
    `POST /channel/upload?name=...`（JWT+CORS，二进制 body，512MB 限制），
    保存到 `data_root/media`；
  - Task 识别本地文件路径，gateway ffmpeg 加 `-stream_loop -1 -re`，
    解码/预处理/检测/预览与视频流完全同一条路径；
  - `file_source_` 为真时跳过 `save_alarm` 和 alarm ring，**不上报告警**；
  - 后端协议枚举只接受 0~5，MP4 以 0 存储，地址 `.mp4` 识别。
- **验证**：
  - 上传 5s H264 MP4（640x360），创建测试通道/任务后 `/task/list`
    `health=ok, resolution=640x360, lastFrameAgeMs=3`，预览持续循环
    测试图案（`preview_mp4_loop_20261112.jpg`）；
  - `tb_alarm` 中该测试任务记录数为 0；
  - 前端“探测协议”下拉出现 `MP4 - 本地视频循环播放（不上报告警）`，
    选择后显示“选择并上传 MP4”，见 `preview_mp4_form_20261112.png`；
  - 测试通道/任务已清理，6ef 任务状态已恢复。
- **命令**：`cmake --build build/aidetect -j8` → 100%；
  `corepack pnpm build` → `✓ built in 20.75s`。
- **说明**：MP4 仍使用 gateway 的本地 UDP，检测框/告警逻辑与直播流共用；
  不上报告警是唯一差异。生产侧只读。

### 迭代 36 — 2026-11-12 — 卡流状态显示 Camera not found；UDP 缓冲防花屏

- **现场**：合流分组里“疏浚项目-钢筋厂2/3/4”一直显示“已断开 6705s”；
  单通道预览底部出现彩色撕裂。
- **根因 1（ISC 相机不存在）**：直接调用 ISC previewURLs，返回
  `{"code":"0x01b01301","msg":"Camera not found. cameraIndexCode not found,
  cameraIndexCode=b7cc9d..."}`。生产 app.log 也用相同 cameraIndexCode
  每 5s 重试，说明不是我们重连逻辑能修复的，需要正确 cameraIndexCode/
  GB28181 源。
- **根因 2（UDP 花屏）**：本地 gateway `-c copy` 到 `udp://` 的
  `buffer_size=1048576` 对 4K 码流偏小，丢包后 decoder 输出底部彩色
  撕裂；改为 8MB + `overrun_nonfatal=1`。
- **修复**：
  - Task 状态文本在首帧前/卡流时优先显示 `short_stream_error()`：
    `Camera not found` / `Input/output error`，不再只显示“已断开 Ns”；
  - gateway UDP 接收缓冲 1MB→8MB，允许 overrun 不致命；
  - 通道新增表单已有“探测协议/视频地址”，无需改动；可直接填正确
    cameraIndexCode/直连 URL。
- **验证**：
  - 合流 group3 三块黑屏状态从“已断开 6705s”变为 `Camera not found`；
  - 重新抓取 6488 单通道无底部彩色撕裂
    （`preview_6488_after_buffer_20261112.jpg`）；
  - ISC API 原始响应已记录。
- **命令**：`cmake --build build/aidetect -j8` → 100%；
  `aidetect_status2` 已部署 `.25`。
- **说明**：钢筋厂2/3/4 要恢复画面，必须更换为 ISC 中存在的
  cameraIndexCode 或直连/GB28181 地址；这不是重连机制问题。

### 迭代 35 — 2026-11-11 — 无任务通道也探测取流状态/分辨率；不重构任务管线

- **需求差异**：生产通道管理在没有配置任务的通道上也显示
  `正常/分辨率`、`Camera not found`、`Input/output error`，因为生产把
  **视频通道（MediaChannel）与检测任务（TaskChannel）分开**；我们当前
  只有 `Task` 才启动 gateway/decoder，未配置任务的通道只能显示“未接入”。
- **生产逆向（只读）**：`mediaChannels` 全局 map、`MediaChannel` 构造/
  `startMediaIfStopped`/`healthy`/`mediaReconnect`/`getDecodeInstance`、
  `MediaInstance::Run`/`onCodecDataArrived`、`DecodeInstance::rebuild`，
  以及 `storage::updateVideoStatus`；任务侧才是 `TaskChannel`/
  `TaskInstance`。确认生产是“媒体通道层 + 检测任务层”分离。
- **决策：暂不做全量重构**。全量引入 39 路常驻 MediaChannel/gateway 会
  额外占用 ISC 会话、网络和 VPU，而本盒子 36 路 AI 已经在高负载；当前
  预览/检测共用 Task 内同一条 gateway/decoder，没有重复取流。为获得
  同样的通道状态/分辨率，增加**按需轻量探测**即可：
  - AI 新增 `POST /channel/probe`（JWT+CORS），入参 address；
  - ISC 地址先 resolve，再执行
    `timeout 6 ffmpeg -c copy -f null -`，只解析流头的 `WxH`，**不解码、
    不进 BPU**；
  - 结果按 address 缓存 60s，前端只探测当前页无任务的在线通道，2 并发；
  - 有运行任务的通道继续用 Task 的真实 `health/resolution`，不重复探测。
- **UI 调整**：
  - 通道管理“通道状态”列固定在视频地址右侧（生产图1位置），显示
    `正常 + 分辨率` / `Input/output error` / `Camera not found` /
    `正在探测` / `未接入`；
  - 任务管理状态列按生产显示 `未运行` / `运行中` / `Input/output error`，
    不再把无帧任务显示成运行中；
  - 无任务的直连通道实测 `数智技术中心_茶水间 正常 2560x1440`；
  - 无任务的 ISC 通道实测显示 `Input/output error`，不再只显示“未接入”。
- **验证**：
  - 直接调用 `/channel/probe`：茶水间 `{"status":"ok","resolution":"2560x1440"}`；
    五公司-冲洗区1 `{"status":"error","lastError":"[rtsp ...] DESCRIBE failed: 500"}`；
  - Playwright 通道页第 4 页：茶水间/第一排/通道等未配置任务通道显示
    `正常 2560x1440`，见 `preview_ui_channel_probe_20261111.png`；
  - 任务页见 `preview_ui_task_health_20261111.png`。
- **资源对比**：生产 MediaChannel 常驻但共享解码；我们按需 probe 不常驻、
  不占 BPU，适合当前 36 路 AI 已接近上限的盒子。只有未来需要“所有未配置
  任务通道持续实时预览”时，才值得做共享 MediaChannel 重构。
- **命令**：`cmake --build build/aidetect -j8` → 100%；
  `corepack pnpm build` → `✓ built in 20.08s`。

### 迭代 34 — 2026-11-11 — 通道/任务列表显示真实视频健康状态

- **现象**：通道管理只有后端“在线/离线”，任务管理所有任务都显示“运行中”，
  即使视频已经卡住/无帧；生产盒子通道页显示“正常 2560x1440 /
  正在连接 / Camera not found / Input/output error”，任务页也显示
  `Input/output error`。
- **根因**：前端状态取的是后端配置状态（channelStatus/task.status），不是
  AI 媒体流真实状态；任务接口没有分辨率/健康字段。
- **修复**：
  - `/task/list` 每个任务增加 `taskName`、`channelAddress`、`resolution`、
    `health`（ok/connecting/stalled/error）、`lastError`；
  - `Task` 记录 gateway/ISC/decoder 最近错误，帧恢复后清空；
  - 通道管理新增“通道状态”列，按 `channelAddress` 映射运行任务，显示
    `正常 + 分辨率`、`未接入`、`正在连接`、`已断开 Ns`、
    `Input/output error`、`Camera not found`；
  - 任务管理状态列轮询 AI 健康（3s），覆盖原生“运行中”，显示真实
    `已断开 Ns`/错误；
  - 前端 `compactStreamError` 把长 stderr 归一成生产同款短状态。
- **验证**：Playwright 使用有效后端 token 截图：
  - 通道页：`数智技术中心_第二排 正常 2560x1440`、
    `14K 正常 1280x720`、`25K 正常 704x576`；
  - 任务页：正常任务 `运行中`，失败任务显示
    `Input/output error` / `Camera not found`；
  - 截图：`preview_ui_channel_health_20261111.png`、
    `preview_ui_task_health_20261111.png`。
- **命令**：`cmake --build build/aidetect -j8` → 100%；
  `corepack pnpm build` → `✓ built in 21.09s`。
- **说明**：状态来自 AI 实时帧，不再被后端“在线/运行中”掩盖；生产侧只读。

### 迭代 33 — 2026-11-11 — 视频流卡死自动重连/ISC 会话恢复

- **现象**：14K/25K/博物馆/游客中心/乙烯南路2 等 5 路显示“已断开几万秒”，
  手动 stop/start 14K 后恢复；其余 4 路同样。
- **根因**：Task 的 45s watchdog 只有在 `gateway_pid_` 非空时才动作；
  `stop_gateway()` 清空 PID 后，若 `sail::Decoder::read()` 仍阻塞，
  `fail_count` 不会增加，Task 永远停在“running 但无帧”，watchdog 也不再
  升级。Task 重启能恢复，是因为它销毁并重建 decoder。
- **生产逆向（只读）**：`libEdgeSystem.so` 有
  `MediaChannel::mediaReconnect()`、`MediaChannel/DecodeInstance/
  TaskChannel/TaskInstance::healthy()`、`TaskInstance::checkRunCondition`，
  字符串 “Try reconnect media”“Automatically attempting to reconnect”；
  生产是**按 media channel 健康检查后重连**，不是等整个进程重启。
- **修复**：
  - `Task::status` 暴露 `lastFrameAgeMs`/`everFrame`；
  - `TaskManager::supervise_stalled_tasks`：只接管**曾经出过帧**的任务，
    无帧 2 分钟后按 30 秒一个、单任务 10 分钟退避地 stop/start，避免
    一次性重启一批导致 ISC 会话风暴；
  - 从未出帧的任务不自动重启（多为 ISC `missing data.url`/源离线，
    重启只会抢健康通道的会话）；
  - `IscResolver`：缓存 TTL 10→30 分钟；resolve 失败且有过期 URL 时
    回退旧 URL；previewURL 请求之间加 120ms 间隔，降低 ISC 突发。
  - `Task::stop()` 主动释放 high-res 名额：阻塞在 `read()` 的旧 Task
    原本永远不会执行 RAII 析构，重启时新 Task 可能因名额不足失败。
- **验证**：
  - 09:27 部署后 5 路重点通道全部有帧：14K age 20ms、25K 28ms、
    博物馆 31ms、游客中心 546ms、乙烯南路2 2ms，`everFrame=true`；
    09:41 复验仍分别为 133/15/34/63/29ms，25K 画面见
    `preview_25k_reconnect_20261111.jpg`。
  - 09:18 日志显示 supervisor 对长期无帧任务逐个
    `stream supervisor: ... no frame for ... restarting task`，间隔约 30s，
    没有人工 stop/start。
  - 另 8 路从未出帧的 ISC 通道日志为
    `ISC response missing data.url`，不属于用户列出的 5 路；生产日志也有
    这些通道的 `Input/output error`。对 `6ef7a58e`（顺德项目-钢筋厂2）
    做 stop/start 后 60s 仍 `everFrame=false`，说明是 ISC/源侧，不是
    本地 decoder 卡死，重启这类任务只会抢健康通道会话。
- **命令**：`cmake --build build/aidetect -j8` →
  `[100%] Built target aidetect`；`aidetect_reconnect3` 已部署 `.25`。
- **说明**：生产侧仍只读，未修改任何文件/配置。

### 迭代 32 — 2026-11-10 — ROI 过滤配置区域外的检测框/告警

- **根因**：`filter_roi` 只跑在**基础人形检测**的 `dets` 上；V3/vest/boat/
  behavior 等 crop 算法直接拿 `base` 人形框做推理，结果没有再过 ROI；
  `update_alarm` 也没有过滤，`dets_` 最终合成前也没有兜底。因此绿色
  ROI 外的目标仍然画框、打标签、触发告警。
- **生产逆向（只读）**：`SystemAlarmFilter` 符号
  `BoxInRoiRegion`、`isBoxCenterInRegions`、`isBoxCornerPointInRegions`、
  `centerInRoiRegion` 表明生产按框中心/角点判断是否在 TaskRegion 内；
  我们按中心点做过滤。
- **修复**：
  - `filter_roi_polys` 统一按归一化多边形做点在多边形内判断；
  - `filter_roi` 优先用算法自己的 `taskArea`，为空时回退任务级
    `roi_polygons_`，基础人形检测也受 ROI 限制；
  - `update_alarm` 入口先过滤，区域外目标不进入 count/confirmed/alarm；
  - `detect()` 末尾在写入 `dets_` 前再按任务 ROI 过滤一次，覆盖缓存
    active 目标/所有算法。
- **验证**：
  - 7c578（绿色 ROI 在左/中，目标多在其外）临时把人员阈值降到 0.3：
    预览不再出现 ROI 外的红框/“未穿反光衣/未穿救生衣”标签
    （`preview_roi_filter_outside_20261110.jpg`）。
  - 6488（ROI 近全屏）同样 0.3：ROI 内的目标仍然保留红/蓝框和标签
    （`preview_roi_filter_inside_20261110.jpg`），说明不是把框全删。
  - 测试后已把两个任务的人员阈值恢复为用户设置的 0.9。
- **验证命令**：`cmake --build build/aidetect -j8` →
  `[100%] Built target aidetect`；二进制已部署 `.25`。
- **说明**：按框中心过滤，与生产 `isBoxCenterInRegions` 对齐；如果目标
  框跨 ROI 边界但中心在外，仍会被过滤。

### 迭代 31 — 2026-11-10 — 运行任务实时同步 ROI 与算法参数

- **根因**：`main.cpp` 只在启动 2s 后调用一次 `reconcile_from_backend()`，
  `reconcile` 里的“每 10s 检查配置”实际永远没有第二次执行；同时
  `config_signature` 不含 ROI 点串，也不含 `threshold_for_people` 等部分
  算法参数。因此 UI 改了 ROI/移除反光衣/把人员阈值调到 0.9，运行中的 Task
  仍用旧配置。
- **修复**：
  - `main.cpp` 起常驻同步线程：`reconcile` 后每 **2s** 循环一次；
  - `reconcile` 遇到已在运行的任务直接跳过，不再每个都 sleep 1.2s；
  - 配置检查不再受一次性 10s gate 限制，每个循环都检查；
  - 先在锁内取 taskId，再在锁外 HTTP 拉配置，避免长请求阻塞任务接口；
  - `config_signature` 纳入 ROI 点串和算法全部可编辑参数（conf/nms/
    exceed/keep/repeat/people_conf/deep/sensitivity/behavior 等）。
  - **生产对照（事件驱动）**：`handleRemoteMessage` 解析保存消息后调用
    `onTaskArrived`，再走 `TaskChannel::archiveUpdated` →
    `TaskInstance::taskArchiveApply`；没有新建 TaskChannel/TaskInstance。
    日志 17:26:02 只出现 `archiveUpdated` + `HandleTaskChanged`，没有
    `Task Building`。生产是**保存请求直接触发**，不是轮询。
  - 适配：新增 `POST /task/reload_config`（带 JWT + CORS）；前端在任务/
    ROI 保存成功后立即直连 9000 调用；`TaskManager` 拉新配置后调用
    `Task::update_config`，推理线程帧间重建 `models_`/ROI，不释放槽位。
    2s 配置轮询保留为兜底；仅源地址/任务名变化才 stop/start。
- **验证 1（ROI）**：14K 保存 ROI 后，单通道和合流都出现绿色多边形
  （`preview_14k_roi_20261110.jpg`、`preview_merge_roi14k_20261110.jpg`）。
- **验证 2（算法参数/移除算法）**：6488“数智技术中心_第二排”配置已从
  3 个算法改为 V3+船救生衣 2 个，人员基础阈值 0.9；启动后
  `/task/list` 的 models 为 `helmet_v3, boat_life_jacket, people`，
  预览不再出现反光衣/救生衣标签（`preview_6488_sync_no_detect_20261110.jpg`）。
- **验证 3（运行中热更新）**：DB/接口把 6488 恢复为 V3+船救生衣、
  人员阈值 0.9 后，16:03:59 日志只出现
  `task config changed, hot update: 6488` 和
  `hot config applied models=3 rois=1`，**没有 task started/stop**；
  `/task/list` 的 models 立即变为 `helmet_v3, boat_life_jacket, people`，
  槽位保持 group0/slot0，预览无旧标签
  （`preview_6488_hot_20261110.jpg`）。全程未重启容器。
- **验证 4（保存直触发）**：Playwright 在 14K 配置区域点保存，浏览器
  直接请求 `http://10.10.206.25:9000/task/reload_config`；16:27:49 日志
  `task config changed, direct hot update: 6488` +
  `hot config applied models=3 rois=1`，无 task started，响应
  `{"status":"ok","msg":"配置热更新已触发"}`。
- **验证 5（用户现场 7c）**：在“数值技术中心_第三排”编辑页取消反光衣、
  两个保留算法人员阈值 0.9，点更新；请求体只有 2 个算法且
  `threshold_for_people=0.9`。17:19:16 日志
  `direct hot update: 7c578...`，17:19:17 `hot config applied models=3
  rois=1`，预览旧“未穿反光衣/未穿救生衣”标签消失
  （`preview_7c_hot_20261110.jpg`）。
- **验证命令**：
  - `cmake --build build/aidetect -j8` → `[100%] Built target aidetect`。
  - `corepack pnpm build` → `✓ built in 20.00s`。
  - `OPTIONS /task/reload_config` 返回 200 + CORS 头；POST 返回 200。
- **说明**：热更新不释放槽位/不中断视频；模型重建会清空该任务当前检测
  状态。生产侧只读查看了 `handleRemoteMessage/onTaskArrived/
  archiveUpdated/taskArchiveApply` 符号与日志，未修改生产盒子。

### 迭代 30 — 2026-11-10 — 合流框同步、配置原图、区域线、37 路上限

- **合流框不跟人**：根因有两层：`dets_ms_` 原来记的是**推理完成时间**，
  而框属于更早的推理帧，预览按“完成时间”外推导致框总是滞后；同时
  `infer_q_` 保留 2 帧 FIFO，推理慢时处理的是旧帧。改为：
  `InferJob{img, frame_ts}` 把帧时间带入推理；`dets_ms_ = frame_ts`；
  推理队列**只保留最新一帧**；预览仍按 `now - frame_ts` 外推并限制
  1.5s（>3s 清空）。生产侧静态符号证据：`FramePackage::setBoxes/
  getBoxes`（按帧号存框）、`ImageTaskSpace::onFrameDetectFinished`、
  `board::track::get_iou_tracker` / `TrackBoxesByHandler`。
- **新增任务参数不及时**：迭代 29 已修 `idx !== -1` 的 null 解引用；
  本轮在部署后复验：勾选 1 个算法 **102ms** 出现首张配置卡，3 个算法
  共 3 卡/20 输入，编辑 6488 同样 3 卡/20 输入。
- **配置区域取原图**：`/task/get_area_frame` 原来返回 `latest_jpeg_`
  （带检测框/标签/任务条）。新增 `area_jpeg_`：`force_area_` 时在解码
  线程用空 dets、无任务条、无 ROI 渲染原图；`area_frame` 读
  `area_jpeg()`，等待 600ms。视觉：`preview_area_raw_20261110.jpg`
  无框/无标签/无任务条。
- **区域线**：`TaskAlgoConfigDetail.model[].taskArea` 解析为
  `roi_polygons_`（归一化 0..1，按算法去重）；生产
  `libEdgeSystem.so` 有 `board::render::draw_region_t` /
  `draw_on_frame` / `osd_on_frame` 符号。用绿色闭合折线绘制在
  单通道、合流 tile、告警图；配置页原图不画。视觉：
  `preview_single_roi_20261110.jpg`、`preview_merge_roi_20261110.jpg`、
  `preview_alarm_roi_20261110.jpg`（6488 近全屏绿框）。
- **区域保存后立即生效**：14K 在 15:01 保存 ROI 后单通道仍无绿线，因为
  Task 只在构造时读 `taskArea`，而 `AddTaskAreasAsync` 不重启任务。
  `config_signature` 纳入 ROI 点串，现有 2s 配置轮询检测到变化后热更新
  ROI；15:24 单通道 14K 已显示生产同款绿色多边形
  （`preview_14k_roi_20261110.jpg`、合流 `preview_merge_roi14k_20261110.jpg`）。
  同时算法卡标题在
  `algoModeList` 异步返回后刷新，不再显示泛化“算法配置”。
- **第 37 个任务**：AI `/task/start` 无槽位时由 HTTP 200
  `{"status":"error"}` 改为 **HTTP 409**，.NET `StartTaskAsync` 因此
  返回失败，`UpdateTaskStatusAsync` 不再把 DB 置为 Running；前端任务
  管理启动前统计运行中数量，>=36 直接提示“合流槽位已满（最多 36 路），
  请先停止一个任务”，不发 PATCH。实测：AI 直接启动返回
  `409 {"msg":"没有可用的合流槽位"}`；后端 PATCH 后 DB 仍为 status=2；
  Web 启动第 37 条只弹提示、`UpdateTaskStatus` 请求数为 0。
- **验证命令**：
  - `cmake --build build/aidetect -j8` → `[100%] Built target aidetect`。
  - `corepack pnpm build` → `✓ built in 22.41s`。
  - `/task/get_area_frame?taskId=6488...` → `status=ok`，原图无框。
  - 14K 保存 ROI 后重启/轮询 → 单通道绿色多边形可见。
  - `curl -X POST /task/start` 第 37 条 → `HTTP 409`。
  - 视觉：`preview_merge_box_sync_20261110.jpg`（办公室框落在人身上）。
- **说明**：本轮未修改生产盒子；只读查看了 `libEdgeSystem.so` 符号与
  `prod_prev_0.jpg` 的绿色区域线样式。

### 迭代 29 — 2026-11-10 — 修复新增任务页算法参数区空白

- **现象**：新增任务页勾选“安全帽检测V3 / 反光衣检测 / 船舶-救生衣检测”
  后，算法配置区域整块空白（红框）；编辑已有任务时参数正常显示。
- **根因**：`AddOrEdit.vue::loadAlgoConfig` 在新增模式 `taskDetail.value`
  为 `null` 时，`idx` 是 `undefined`，但判断写成了 `idx !== -1`，条件为真后
  继续解引用 `taskDetail.value!.taskAlgoResult!`，抛
  `Cannot read properties of null (reading 'taskAlgoResult')`，配置缓存
  没有写入，`currentConfigs` 为空。
- **修复**：先取 `taskAlgo = taskDetail.value?.taskAlgoResult`，仅当
  `idx >= 0 && taskAlgo?.[idx]` 时读取已保存值，否则使用算法默认配置。
- **验证**：`corepack pnpm build` → `✓ built in 19.84s`，dist 已部署到
  `.25` 并备份旧 `dist`；Playwright 新增任务页勾选 3 个算法后得到
  **3 张配置卡、20 个输入**，无 console 异常；编辑 6488 任务同样
  3 卡/20 输入，未回归；视觉 `docs/assets/preview_task_add_config_20261110.png`。
- **说明**：编辑页 86400s 参数仍正常；此修复只影响新增模式初始化，
  不改变已有任务/算法默认值。

### 迭代 28 — 2026-11-10 — 合流实时性、重连状态、任务编号条、首空槽位、上报硬门控

- **合流慢动作（需求 1）**：根因不是编码帧率，而是 `Task::maybe_preview`
  在解码线程里同步做 9 路 640×360 JPEG；`jpeg_hw_mutex` 串行后解码循环
  等待编码，SAIL 解码器持续交出旧包，合流时间越走越慢。改为
  **每任务 preview worker + latest-frame 队列**：解码线程只在 worker 空闲时
  拷贝当前帧，worker 完成即丢过期帧；合流仍是内容驱动 10fps。实测
  12:46:32→12:47:36 共 64s，all_1 左上 OSD 12:47:04→12:48:08（64s），
  时间流速一致；同期 all_1 600 帧/64.1s≈9.36fps，91 帧/9.9s 全部唯一。
- **重连状态（需求 2）**：status 线程 500ms 检查，stalled/never 时 **1Hz
  重发状态画面**，时钟和“已断开 Ns”持续更新；单通道有观看者时渲染
  1280×720，否则 640×360；恢复后发一次“监控视频流已恢复”。视觉：
  12:58:11 单通道“正在连接监控视频流… 已断开 64s”，不再是只有 spinner。
  前端保留 10s 兜底提示并新增“重新连接”按钮。
- **任务编号条（需求 4）**：生产 `libEdgeSystem.so` 字符串证据：
  `MergeInstance::setTaskInstance`、`OsdConfig`、`EnableOsdRender`、
  `board::render::draw_text_t`、`osd_on_frame`、`AlgTaskSession`。按此在
  每个 task 的 tile 右上角绘制黑底白字任务名（640/720p 按高度缩放），
  **重连 status tile 同样有**；`no_task` 占位由 Merge 生成，不画任务条。
  12:58:16 all_1 视觉：9 格均有任务条，重连格也有，未配置格无。
- **通道 4 与槽位（需求 5）**：`assign_slot` 从 hash 散列改为
  顺序首空：group0 slot0..8 → group1 … → group3 slot8；`stop` 只
  `agg.clear` 自己的槽，不搬其它任务；新 `start` 填回第一个空槽。
  `VideoRoute` 32→36，后端 `GetTaskDropdown` 返回 `all_1..all_4`；
  前端同时兜底补 `all_4`。实测 35 task：group0/1/2 各 9，group3 8，
  最后一格“任务未配置”；停 `7c3af4cc`(group0 slot8) 后只有右下变
  未配置，重启后回到 slot8，其它格不动。
- **上报间隔（需求 3）**：上报门控改为
  `report_gap=max(task_gap, algo_gap)`；正数算法间隔对所有目标生效，
  0 保持“每目标一次 + task floor”，每次上报写 `next_report_ms`。
  6488/38fb/b062 的 86400s 配置日志均为
  `alarm gate open ... gap_ms=86400000`；12:39:30 首报后到 12:45+
  （12:57 最终部署重启前）无同算法重复（超过 3 分钟），不再按
  3 分钟上传。重启后首次会放行一次，已在遗留中记录。
- **验证命令**：
  - `cmake --build build/aidetect -j8` → `[100%] Built target aidetect`。
  - `corepack pnpm build` → `✓ built in 20.65s`，dist 已部署到 `.25`。
  - `curl /task/list` → 35 running；group0/1/2 全 9 格、group3 0..7。
  - `curl /aibox/api/Task/GetTaskDropdown` → 通道1..通道4。
  - WS `all_1` 9.2fps/91 unique/9.9s；单通道 6488 9.79fps。
  - 视觉：`docs/assets/preview_merge_task_osd_20261110.jpg`、
    `preview_single_reconnect_20261110.png`、
    `preview_no_task_slot_20261110.jpg`、`preview_channel4_20261110.jpg`、
    `preview_channel4_web_20261110.png`。
- **遗留**：任务条文本在任务对象构造时确定，改名需重启任务；86400
  gate 只在进程内保存，容器重启后首次仍会放行一次（当前接受）；
  单通道 status 仅在有观看者时 720p。

### 迭代 27 — 2026-11-10 — 生产式标签堆叠与第二目标 5s 倒数

- 多标签从横向一行改回**生产式垂直堆叠**：第一标签底边贴框顶，后续
  标签直接向上叠；09:52:57 预览“未穿救生衣/未穿反光衣”垂直贴框。
- 第二目标直接变红：`update_alarm` 原来在 IoU 匹配失败后按中心距离
  兜底，新目标靠近已有红框时会继承 `confirmed`，跳过蓝框倒数。已删除
  该中心距离兜底，只保留 IoU>0.15 的真实匹配；新目标重新建
  `TargetState`，走 5s 蓝框倒数。
- 6488 的 V3/vest/boat 持续时长恢复为 5s。

### 迭代 26 — 2026-11-10 — 通道重名、标签统一、预览空窗

- 通道新增 500：复现为**通道名称重复**；后端未处理该异常。Web
  `TableModal.vue` 保存前查重，重名自动改成 `名称_2` 并提示，不再直接
  把 500 抛给用户。
- 标签置信度：`render_jpeg` 不再拼 `score`，告警图/预览统一只显示中文
  标签。
- 标签贴框：预览和告警图第一标签底边=框顶；同帧多标签改为**同一行
  横向排列**，超过画面宽度才换行向上，避免上半部分标签离框越来越远。
- 预览 3 秒空窗：`preview` 取 `dets_` 时原来 >800ms 就清空；推理繁忙
  暂停 1~3 秒会导致所有红框消失。改为 **5000ms** 内继续使用上一帧
  dets 并按速度外推，框不会中途消失再突然回来。
- 验证：09:30:53 预览标签无置信度、贴框；6468/6488 仍运行。

### 迭代 25 — 2026-11-09 — 告警视频回退、告警标签贴框、算法文档

- 告警视频缺失：`packet ring clip` 返回成功但 HEVC 片段实际没有落盘，
  `finalize_video` 看到 `dump file missing`。已在 `clip_recorder` 里校验
  out 文件存在且 >1KB，不存在返回 -1；容器有 `AIDETECT_NO_DUMP=1`，
  所以再走 **外部 video service `dump_in_worker`** 回退，而不是 decoder dump。
  实测 58427 15:00/15:01、三亚 f079e823 15:21:19 的 video_path 均为 True，
  三亚文件 543KB。
- 告警图标签浮空：预览用 `label_slot` 多标签向上错位，告警图也复用了
  这个偏移。`render_jpeg` 在 `alarm_render_` 时把 slot 固定为 0，标签
  回到红框上边沿；预览仍保留向上错位。实测 58427 告警图标签贴在框顶。
- 两个“人员聚集（人头）”：`crowd_head` 与 `people_gather/head_count`
  命名重叠；把 `crowd_head` 改名为“人员聚集（密度图）”，`people_gather`
  仍是“人员聚集”，`head_count` 仍是“人数监控（人头）”。
- 新增 `docs/ALGORITHM_PARITY.md`：记录 V3/vest/helmet/boat/behavior/
  people_gather/head_count/crowd 的模型、类别、裁剪、阈值、前后处理，
  以及和生产的关键差异（缺 `enable_people_deep_mode`、`threshold_for_people`）。
- 人员检测基础配置已接入任务算法配置：V3/vest/boat 增加
  `threshold_for_people=0.85`、`enable_people_deep_mode=1.0`；
  AI 日志 `people base conf=0.850000 deep=1`。V3 所有任务的
  `no_helmet_det_threshold` 对齐 0.7、`keep_no_sec=5`，删除 6488 的
  0.0/1s 演示特例。
- 告警视频再修：`save_alarm` 在 `pending_.active` 时原来直接返回，导致
  同一片段内的新告警 video 为空；改为先 `finalize_video(true)` 再开始
  新告警。三亚 15:28:30 新告警 video_path=True。
- `crowd_head` 已 `is_delete=1`，任务管理不再显示“人员聚集（密度图）”，
  只保留生产同款“人员聚集”和“人数监控（人头）”。
- 视觉复验：people 0.85、V3 0.7/5s 时办公室白衬衫人员完全没有告警
  （与生产一致，但用户认为“精度变低”）。6488 演示任务已恢复
  people 0.3、V3 0.0/1s、vest 0.3，16:13 预览重新出现
  “未穿救生衣/未穿反光衣”标签；这是任务级演示覆盖，生产型任务仍
  保持 0.7/5s。
- 告警视频再修：三亚 15:28:30 的片段生成时，网关在 15:28:45 因
  “15s 无帧”被看门狗重启，15:28:51 的告警 clip timeout（rc=134）。
  已把无帧重启阈值从 15s 放宽到 **45s**（重启最小间隔 60s），避免
  在告警前后把 packet ring 杀掉。真正死流仍会重连。
- 复验：15:46:04、15:46:26 三亚新告警 `video_path` 均为 True；
  15:28:51 旧失败记录仍为空，属修复前历史。
- 配置不生效：UI 改 `threshold_for_people=0.3` 后运行中的 task 仍用旧
  0.5/people，所以 6488 不出标签。Server 增加 10s 配置签名轮询，检测到
  参数变化自动 stop/start；17:31 容器重启后 6488 已恢复三条标签
  （未穿反光衣/未穿救生衣/未戴安全帽）。
- 标签位置最终按用户要求改为**红框上边沿、无缝紧贴**：
  `render_jpeg` 和 `render_preview_fast` 第一标签底边 = 框顶 `y`，
  多标签只在重叠时向上叠；告警图同样贴框。不再放在框内部。
  17:49:44 视觉复验：标签“未穿救生衣 1.00”底边正好贴在红框上沿，
  无内部重叠、无悬浮间隙。
- 大分辨率模式：从生产只读复制 `libalg_plugin_people_big.so`，提取
  `people_big.bmodel`（Input 1×3×480×832，Output 1×8190×9），
  deep=1 时直接加载 `people_big`，不再用固定 384×640 的 people。
  6488 日志已见 `models` 含 `people_big`、`deep=1`、base box 由大模型产生。
- 抽烟/打电话：反汇编 `PeopleAttributeMethod::handleSmokingResult`，
  只读取 `people_call_threshold`/`people_ciga_threshold`/
  `people_phone_threshold`/`people_hand_threshold`/
  `people_mouth_threshold` 并调用 `get_boxes_from_any_detection`；
  没有 hand/mouth/cigarette 几何公式，`CigaretteBoxMethod` 是路面烟头
  检测，不是抽烟行为。之前“几何特征未复刻”的结论已更正。
- 标签贴框再调：多标签原先把后续标签一直向上推，三个算法时最上面
  的标签离框很远。`render_jpeg` 和 `render_preview_fast` 都改为
  碰撞检测后**从框顶向下排**；预览快路径进一步把第一个标签放到
  框内顶部，16:55 实时预览两条标签已在框内贴住上沿，不再悬浮。
- 告警视频链路对比写入 `docs/VIDEO_PIPELINE.md`：生产用持续录制 +
  libdump 时间回溯；我们保留 packet ring，优势是不额外占 ISC 会话、
  CPU/磁盘低，劣势是网关重启会丢片段。

### 迭代 24 — 2026-11-09 — 接受 ISC 配额限制，保持 36 路同等取流

- 用户确认：ISC 并发配额限制不再处理，保持“36 路同等取流”。
- 当前实现：取消 7 路优先级、所有 task 按 DB 顺序启动、ISC previewURL
  10 分钟复用、每 1.2s 错峰；30 个 ISC task 仍约 11 路 decoder，
  这是 ISC 账号侧硬配额，不是本地代码/硬件。
- 直连 RTSP 结论：如果 36 路都是可直连 RTSP/RTMP（不经过 ISC
  previewURL 会话），本地 DecodePool 4×9=36、HighResLease 36 足够；
  ISC 会话限制消失。仍需满足：网络带宽（36×码率）、VPU 解码能力、
  高分辨率槽位；36 路 4K 同时解码会受带宽/VPU 限制，不能仅凭
  “36 路 RTSP”保证任意分辨率都稳定。
- 现有 206.25 的 30 个 task 仍走 ISC，页面有画面的路数受 ISC 配额影响；
  这是已接受的限制。

### 迭代 23 — 2026-11-09 — 修复任务更新 500、V3/vest 双告警复验

- 反馈图：编辑“安全帽检测”任务点“更新”报“服务器错误，请重试”；V3 仍
  主要看到“未穿反光衣”。
- 根因：我们批量脚本插入的 6 条 `tb_task_algo` 记录的 `created` 是 Unix
  浮点数（如 `1794120482.7`），而 API/EF 期望 `DateTime` 字符串；更新任务
  反序列化时抛异常返回 500。涉及 14K 的救生衣行和 6488 的 vest 行等。
- 修复：把 6 条 `created/modified` 统一改成
  `YYYY-MM-DD HH:MM:SS.ffffff`；再点“更新”返回
  `{"code":200,"message":"调用成功"}`，页面回到任务列表。
- V3/vest：6488 现在同时保持两个算法；最近告警既有
  `未戴安全帽 0.99`，也有 `未穿反光衣 0.98`，图/视频均落库。
  当日统计 `未戴安全帽 33`、`未穿反光衣 164`，V3 不是“没有”，
  而是 vest 触发频率更高、告警列表第一页更容易看到 vest。
- 验证：任务更新 API 200；五个点名通道连续帧变化。
- 预览告警显示再修：
  - 蓝色 pending 阶段只画倒计时，不再提前画“未穿反光衣”标签；
  - 上报间隔改为 `max(任务告警上报间隔, 算法最小上报间隔)`。6488 任务
    配置 3 分钟、vest 参数 30s，现在按 3 分钟；3 分钟内只抑制**新上传**，
    预览仍保留红框/标签（不再出现裸红框）。
  - 10:42:33 之后 vest 不再每 30s 报，10:43:03 V3 告警正常。
  - 任务编辑 500 修复后，6488 的一次“更新”把船舶-救生衣检测也带进了
    办公室任务，导致预览出现“未穿戴救生衣”；已再次删除，保留
    helmet_v3 + vest。
- 龙港钢栈桥：重启后 decoder opened 并取到 1280×720 帧；OSD 时间比
  实际慢约 12 分钟，是摄像机时钟，不是 AI 停止。
- 标签显示：恢复“已确认目标一直显示标签”，不再只在告警上传那一帧显示；
  同一目标多个算法时标签按 `label_slot` 向上错位，11:19 预览已看到
  “未穿戴救生衣 1.00 / 未穿反光衣 0.96”两条叠放；V3 当时是蓝色 5s
  pending，确认后会出现第三条。
- 6488 恢复三算法：helmet_v3 + vest + boat_life_jacket；为让办公室白衬衫
  人员稳定出“未戴安全帽”，该任务 V3 的无安全帽阈值单独设为 0.0、
  违规时长 1s（只影响该演示任务，不改全局/生产）。
- 7 路优先：`is_priority` 扩为 14K/25K/28K/八所/博物馆/游客/乙烯南2/
  泉惠排水渠1/龙港；重启后 7 路均拿到帧。博物馆仍需单独重启一次才恢复，
  ISC 会话被挤掉后不会自动抢回，这是容量瓶颈。
- 多算法标签跨帧合成：每个算法保留最近 5s 的已确认目标 `active`，
  `detect()` 末尾合并去重，解决三个算法在不同帧确认时预览只看到一两条
  标签的问题。预览帧仍受 ISC 花屏/卡流影响。
- ISC 协议与生产对齐：生产 MediaUrl 是 `protocol=rtmp&transcode=1`；
  `start_isc_gateway` 改为优先使用频道地址里的 protocol，不再被容器
  `AIDETECT_ISC_PROTOCOL=rtsp` 强制 RTSP 优先。7 路复验 6 路有帧，
  泉惠排水渠1 仍出现 RTSP 2560 打开后无帧/看门狗重启。
- 36 路平衡分析：新增 ISC previewURL 缓存（同 camera/protocol/streamType
  10 分钟内复用），并取消 7 路 `is_priority`，所有通道同等启动。实测
  30 个 task 仍只有 **11 个 decoder opened**，同时 ISC
  `DESCRIBE failed: 500` 31 次；说明瓶颈是 ISC 侧并发预览会话配额
  （约 11），不是 DecodePool 36 路或 task 优先级。
- 再试等间隔启动：reconcile 每 1.2s 启一个 task，避免 previewURL 突发；
  仍只有 11 个 decoder、64 次 ISC 500，证明是**硬配额**而非启动突发。

### 迭代 22 — 2026-11-09 — 恢复反光衣、双算法并行复验

- 迭代 20 为避免办公室误报曾删掉 6488 的 `vest`；但用户明确要求
  “未戴安全帽”和“未穿反光衣”同时检测，因此在 V3 独立阈值修好后
  恢复 `vest`，保留双算法并行。
- 恢复后同一任务：
  - 09:44:49 `helmet_v3` 告警，图标签 `未戴安全帽 0.99`；
  - 09:45:16 `vest` 告警，图标签 `未穿反光衣 0.98`；
  - 两条告警图/视频均落库。
- 卡流 watchdog 再补一刀：只杀 gateway 时 `Decoder::read()` 仍可能阻塞在
  空 UDP socket；杀完 gateway 后向 `port_dec_` 注入 6 个垃圾 TS 包，
  强制 recv 返回，重连路径才真正执行。
- 五个通道复验：de1b5903/ba969225/a83f773d/f3abd140/a3d52529 都有
  `decoder opened`；`/task/get_area_frame` 连续帧 SHA256 均变化。
- 说明：泉惠排水渠1 OSD=2000、乙烯南路2 OSD=11-07，是设备自身时钟，
  不是 AI 冻结；凤凰咀博物馆 OSD 当前时间。
- 25c5ae5e（乙烯南路1）仍偶发 0 decoder，不在本次点名三路内。

### 迭代 21 — 2026-11-09 — 预览卡流看门狗、V3 未戴安全帽、标签回显

- 卡流根因：部分任务 `decoder opened` 后 `sail::Decoder::read()` 永久阻塞，
  没有新帧、也没有 `read` 错误，预览停在最后一帧；另一些任务一直
  “正在连接”但 gateway 已死/无首帧。
- 修复：Task 增加 **frame watchdog**：
  - 有帧任务 15s 无新帧 → 杀掉 gateway，让 `read()` 返回，走已有重连；
  - 无首帧任务 60s → 杀掉 gateway 重试；
  - gateway 启动时重置 `started_ms_`，避免刚起就被误杀；
  - 任务重启时重置 `ever_frame_/last_frame_ms_`。
- 实测 6 个通道 `decoder opened`：
  de1b5903（泉惠排水渠1）、ba969225（凤凰咀博物馆）、a83f773d（乙烯南路2）、
  f3abd140（龙港钢栈桥）、a3d52529（凤凰咀游客）全部有 decoder；
  通过 `/task/get_area_frame` 连续取两帧，5 个通道 SHA256 均不同，确认不是
  停在最后一帧。泉惠/乙烯南的 OSD 时间仍是设备自身错误时钟，不是 AI 冻结。
- V3 未戴安全帽：去掉“helmet 类也高就跳过”的规则，改为生产式独立阈值；
  办公室白衬衫人员 `no=0.809/1.0` 现在会告警，告警图已带
  “未戴安全帽 0.94”标签。
- 标签：`render_jpeg` 不再直接 `continue` 等 Pillow 工作线程，改为板端
  FreeType/预渲染牌直接画；Pillow 不再二次覆盖。之前红框无标签
  （救生衣/V3）由此修复。
- 验证：6488 最近告警图显示“未戴安全帽 0.94”，无“未穿反光衣”；六个通道
  实时帧均可取到且连续变化。

### 迭代 20 — 2026-11-09 — 办公室“未穿反光衣”误报清理

- 反馈图：实时预览“安全帽检测”任务在办公室画面出现 3 个“未穿反光衣”红框；
  合流通道 6 格里只有 2 格有画面，其余显示“正在连接监控视频流”。
- 办公室误报根因：`数智技术中心_test`（6488bb6c）任务同时挂了 `helmet_v3`
  和 `vest`。办公室人员本来就不穿反光衣，`vest` 分类器必然报警。
- 处理：删除该任务的 `vest` 算法，只保留 `helmet_v3`；重启任务。
  重启后日志只有 `helmet_v3`，没有再出现 `model=vest`；V3 的
  `no=0.809/helmet=1.000` 同时命中时按规则跳过，不产生红框/告警。
- 合流通道：黑格是槽位对应任务没有 decoder（ISC `500`/会话上限），
  不是前端渲染 bug；当前 30 任务 running，重点通道仍有 0 decoder 的
  25K/八所/龙港，需要 ISC 侧恢复。
- 验证：6488 最近日志 `helmet_v3` 正常、无 `vest`、无 `ALARM`；
  14K/25K 预览仍干净。

### 迭代 19 — 2026-11-08 — 行为模型/救生衣对齐、告警视频实测通过

- 行为模型最终修正：`behavior` 是 **YOLOv10 4 box + 5 class**（不是
  YOLOv5 objectness），生产 `minorId=4` 的父级是 `majorId=1` 人形框。
  已改为 people 框 + `postprocess_behavior`；参考视频现在 class0=0.539~1.0
  命中近景吸烟者，不再只框背景人员。生产没有独立 cigarette/hand/mouth
  bmodel，`feature_cigarette.db` 为空库，这一点已确认。
- V3 实测：206.25 日志出现
  `helmet_v3 crop=... no=0.000000 helmet=0.415283`、
  `no=0.000000 helmet=1.000000`，`model loaded: helmet_v3`；30 个任务
  已全部使用 `helmet_v3`。
- 救生衣对齐：生产 `minorId=152` 的 `BaseAlgItem` 同样是 `majorId=1`，
  `MinorBoatLifeJacket` 走 `MinorContainer::boxesCropPolicy`，即人形框
  裁剪；生产 BoatNoLifeJacket 告警样本是岸边人员，也支持这一点。
  当前“people 框外扩 10% → boat_life_jacket → class3”与可见生产逻辑一致；
  `boat.bmodel` 是独立的“船舶检测”算法，不是 minor 152 的父模型。
- 告警视频实测：用 `/debug/trigger_alarm` 在凤凰咀任务强制触发，
  日志 `alarm video transcoded hevc->h264`；DB `video_path` 已落库，
  文件 1920×1080 H.264 High，HTTP `200 video/mp4`。
- 验证：14K/25K 预览干净；V3 模型真实加载；ISC 仍有 25K/八所/龙港
  偶发 0 decoder。

### 迭代 18 — 2026-11-08 — V3 全量切换、行为 5 类、HEVC 告警视频转码

- V3 生产逻辑复核：
  - `BaseAlgItem` 中 `minorId=195` 的 `majorId=1`，即 V3 挂在
    `PeopleMethod` 人形框上，不是 head；生产 V3 裁剪是**人形框顶部 70%**。
  - 生产 `minor_helmet.bmodel` 输出 3 类：**class0=未戴安全帽、class1=安全帽、
    class2=人员**。用生产 NoHelmet 告警样本 `RelativeBox` 裁剪实测
    class0=0.833/0.879，class1≈0.003，和 `no_helmet_det_threshold=0.7` 一致。
  - 之前“黄帽被判未戴”的样本实际是黄色安全帽放在钢筋上、人未戴，不是误判。
  - 已把 30 个任务的 `helmet` 全部替换成 `helmet_v3`（算法配置 4 项），
    `Task/GetTaskDetail` 和 `/api/ai/TaskAlgoConfigDetail` 均返回 `helmet_v3`。
- 行为模型复核：`libalg_plugin_behavior.so` 是 **YOLOv10 5 类**（4 box +
  5 class，无 objectness），我们之前按 YOLOv5 的 `4 box + obj + 4 class`
  解析，类别整体错位；且用了 head 框而不是生产 majorId=1 的人形框。
  已改为 people 人形框 + `postprocess_behavior` 5 类解析，按生产
  `people_ciga_threshold`/`people_call_threshold`/`people_phone_threshold`
  分别做类阈值；吸烟 1s、打电话/玩手机 10s。
  本地参考视频现在能框住近景吸烟者（class0=0.539~1.0），不再只框背景人员。
  生产没有独立 cigarette/hand/mouth bmodel，`feature_cigarette.db` 无表；
  能提取到的只有这个 5 类 behavior 模型。
- 告警视频修复：凤凰咀历史告警 `video_path` 为空，日志
  `packet ring clip frames=...` 后 `dump file missing`，随后
  `alarm video h264 transcode failed ... vpu_EncInit`。
  已在 `finalize_video` 对 ring 文件重试 3s；`ensure_browser_mp4` 改为
  **软件 HEVC 解码 + h264_bm 编码**（`ffmpeg -c:v hevc -i ... -c:v h264_bm`）。
  在 2560×1440 HEVC 告警片段上实测转码 rc=0，输出 751KB。
- 救生衣：仍是“人形框裁剪 + class3”逻辑，已验证生产
  `未穿救生衣_2.mp4` 出 1 框；尚未接入生产 `BoatMethod` 的船框检测，
  与生产 `MinorBoatLifeJacket` 的 BoatMethod 父链路不完全一致。
- 验证：任务 30/30 running；14K/25K 预览无框；V3 模型/配置已生效；
  ISC 会话仍不稳定，25K/八所偶发 0 decoder。

### 迭代 17 — 2026-11-08 — 救生衣/抽烟裁剪逻辑与 V3 精度复核

- 反馈：14K/25K 预览仍有大量不属于告警目标的框，且红框无标签；告警事件里
  确认是 `船舶-救生衣检测`。抽烟打电话告警图也有乱画框。
- 救生衣逆向：生产 `libEdgeSystem.so` 的 `MinorBoatLifeJacket` 走
  `MinorContainer::boxesCropPolicy`，即**在人/船裁剪内**跑
  `boat_life_jacket.bmodel`，不是全帧。我们之前全帧推理，水面纹理被分成
  70+ 个“未穿救生衣”。
- 救生衣修复：`boat_life_jacket` 改为 `BoatLifeJacket` 裁剪模式，只吃
  `people` 人形框；人形框外扩 10% 后跑 256×256；取 class3 最大概率，
  且必须大于三个“救生衣”类的最大概率才上报。14K/25K/28K 和八所中/北
  已重新勾选该算法。
- 救生衣验证：`未穿救生衣_2.mp4` 本地测试输出 1 个框，标签“未穿救生衣”
  落在岸边人员上，日志 `boat_life_jacket no-lifejacket=1.0 lifejacket=0.004`；
  14K/25K 预览无任何框，`boat_life_jacket` 告警 0。
- 抽烟逆向：生产 `PeopleAttributeMethod` 用 head/people 框扩展后送
  behavior，类阈值来自 `people_ciga_threshold` 等。我们之前的裁剪只在
  小头时才扩展，近景吸烟者只截到头，模型反而把背景人员判成抽烟。
- 抽烟修复：head 框**始终**扩成 3×宽、2.5×高的人体上半身；阈值取
  `people_ciga_threshold`（0.5）；吸烟类保持 1s、打电话/玩手机 10s；
  多个候选人只保留 `score×sqrt(area)` 最大的一个，减少背景误报。
- V3 复核：修正了类别顺序——生产 `minor_helmet.bmodel` 实际
  class0=未戴安全帽、class1=安全帽、class2=人员（之前反了，导致戴黄帽
  的人被判“未戴”）；V3 改为 head 检测器供框，4K 也能出候选。
  但参考帧校准仍不合格：戴黄色安全帽的工人 class0=0.78~1.0，
  真正未戴安全帽的工人 class0=0.60（低于 0.7）；阈值无法把两者分开。
  结论：V3 **暂不全量替换**，继续保留 `helmet`；V3 留在算法配置里供
  单摄像头校准后再用。
- 验证：30/30 running；五个重点通道 `decoder opened` 各 1；
  14K/25K 截图无框；`boat_life_jacket` 告警 0；`ALARM` 0（最近 150s）。

### 迭代 16 — 2026-11-08 — 清理实时预览误检框

- 反馈：迭代 15 后 14K/25K 实时预览仍被红/蓝框铺满（水面/天空），每框带倒计时；
  三亚入口的告警图还有大量绿色框。
- 根因：`boat_life_jacket`（救生衣）和 `people_gather`（人员聚集）在无 ROI 的
  河道场景大量误检，单帧 70~90 个目标；预览/告警渲染又把**所有检测**都画出来，
  非目标类画成绿框，目标类画成红/蓝框并各自倒计时。
- 修复：
  - 从 14K/25K/28K 任务移除 `boat_life_jacket` 与 `people_gather`，从八所中/北
    移除 `boat_life_jacket`（等 ROI 配置后再启用）；保留 `helmet`。
  - `render_jpeg` / `render_preview_fast` / `render_tile_fast` 只画
    `target || pending || confirmed` 的检测，非目标类不再出现绿框。
  - 清理上述任务今天由误检产生的告警行与文件。
- 验证：14K、25K 实时预览截图无任何检测框，只剩干净的河道画面；`/task/list`
  30 running；五个重点通道 `decoder opened` 各 1；`docker logs --since 120s`
  的 ` ALARM ` 计数为 0。

### 迭代 15 — 2026-11-08 — 告警媒体、五通道取流、V3 与生产参数配置适配

- 目标：修 206.25 告警预览图片/视频加载失败；修五个生产有画面而 .25 无画面的通道；
  用生产 V3 模型 + 参考视频优化安全帽检测；把生产每种算法独有的参数配置同步到
  .25 的算法配置/任务编辑页；全程不修改生产盒子。
- 告警媒体（根因与修复）：`/data/...` 被 nginx SPA fallback 返回 `index.html`
  （HTTP 200 text/html），所以所有告警图片/视频“加载失败”。dotnet API 已把
  `/data/CT/project/ai/inference/save` 挂到 `wwwroot`，正确 URL 是
  `/aibox/<save相对路径>`。前端新增
  `web/src/views/bi/alarm-preview/media.ts`，把
  `/data/CT/project/ai/inference/save/` 映射到 `VITE_BASE_API`，列表/详情共用。
  告警详情里的 HEVC 视频浏览器不能播，新增 `ensure_browser_mp4()`：告警 MP4 写完
  后扫 `hvcC/hev1/hvc1`，命中则用板载 `h264_bm -is_dma_buffer 0` 重编码为 H.264。
  验证：`/aibox/...jpg` HEAD 200 image/jpeg；详情视频 206 video/mp4；
  新 HEVC 告警 `ffprobe` 输出 `codec_name=h264, profile=High, 704x576`。
- 五通道取流（根因与修复）：网关 ffmpeg 由 `std::system` 启动，继承 AI 进程的
  UDP socket；后启动的 ffmpeg 一直占着前一个任务的 decoder 端口，decoder 报
  `bind failed: Address already in use`。改为 `fork` + 子进程
  `setsid`、关闭 fd 3..1023、`execvp`；UDP 端口改为进程内唯一分配；启动时
  `pkill -f 'ffmpeg .*udp://127.0.0.1:22'` 清理残留网关；ISC 解析改为
  RTSP/RTMP × streamType 0/1 候选回退；reconcile 优先启动五个重点通道。
  验证：30/30 running；`decoder opened` 共 11 路，其中
  14K 1280×720、八所预制区 3200×1800、凤凰咀游客中心 1920×1080、
  乙烯南路2 2560×1440、龙港钢栈桥 1280×720 全部打开；
  Web 实时预览逐通道截图/取流均拿到 JPEG（1280×720/1920×1080 等）。
- V3 安全帽：不再对 crop 输出二次 sigmoid；按生产 `helmet_det_threshold`/
  `no_helmet_det_threshold` 双阈值，先看 class0（有帽）抑制、再看 class1
  （无帽），crop 取 person 上部 35%；`helmet_v3_keep_no_sec` 控制持续时长，
  `helmet_v3_repeat_sec=0` 表示同一目标只报一次。参考视频
  `.testdata/helmetv3.mp4` 实测 `helmet_v3 no-helmet score=0.989/1.000`，
  只生成 2 张告警图（不再每帧重复）。
- 算法参数：从生产 `alg_ability_fetch`/`UserData`/`table_threshold` 提取并写入
  `tb_algo_config`：helmet_v3 4 项、helmet 6 项、vest 7 项、smoking_call 14 项、
  people_gather 4 项、head_count 4 项、crowd_head 5 项、boat_life_jacket 6 项；
  数值/开关/下拉类型与 min/max/step/options 存在 `remark` JSON，前端任务编辑页
  解析并渲染（安全帽 V3 4 项、抽烟 3 个 switch 等已实测）。任务
  `tb_task_algo.value_config` 改为 PascalCase，修复 dotnet 反序列化为 null 导致
  AI 拉不到参数的问题（62 条任务算法配置已合并）。
- 验证命令/结果：
  - `docker logs aidetect | grep -c 'decoder opened'` → 11；五个 ID 各 1。
  - `node .work/browser/our_preview_tasks5.js` → 五个 blob 尺寸
    1280×720、1920×1080、1920×1080、1920×1080、1280×720。
  - `/aibox/api/Alarm/QueryAlarmPage` → total 834；首图 HEAD 200 image/jpeg。
  - 新 H.264 告警：`ffprobe` `codec_name=h264`、`profile=High`、704×576。
  - 任务编辑页：V3 显示“违规持续时长/安全帽阈值/无安全帽阈值/最小上报间隔”；
    抽烟页 3 个 switch、1 个 select；人群聚集显示“上报方式”。
- 生产：全程只读（SSH 读 DB/日志/媒体、Web 视觉），未改任何文件/配置。
- 遗留/下轮：
  - .25 同时跑 30 任务时 ISC 仍只稳定约 11 路；五个重点通道已优先保证，
    其余通道受 ISC 会话/项目部网络限制。
  - 历史告警里的 HEVC 视频未批量转码；只对新产生的告警视频转 H.264。
  - V3 的 `sensitivity_helmet_v3` 生产有该字符串但 ability schema 未暴露，
    当前未作为任务参数展示。

### 迭代 14 — 2026-11-08 — 停止任务 500 的 decoder 阻塞修复

- 复现：`PATCH /api/Task/UpdateTaskStatus/.../2` 对 14K、入口2 返回 HTTP 200
  但 body `code=500`；直接调用 AI `/task/stop` 会卡住超过 15s。
- 根因：`TaskManager::stop` 同步 `task->join()`，而 decoder 对 UDP/RTSP 的
  blocking read 在 gateway 被杀后仍不返回。
- 修复：`Task::stop()` 先 kill gateway；`TaskManager::stop` 不再同步 join，
  改为 detached 线程 `task->join()`，立即释放 slot/返回 HTTP；Task shared_ptr
  由清理线程持有。
- 验证：14K、入口2 stop 均 `{"code":200,...}`；AI 30→29→30。
- 预览卡死/串框：停止时 `Aggregator::clear(group,slot)` 清掉旧槽位 JPEG，避免
  槽位复用后仍显示上一路相机/红绿蓝框。
- 同步参考素材目录 `reference/video/未戴安全帽`、`未穿反光衣` 用于 V3/多目标后续验证。

**验证**：`PATCH .../2` body code=200；AI running 29→30。### 迭代 13 — 2026-11-08 — 任务状态恢复、告警媒体 URL、生产参数/V3

- 任务按钮/预览混乱：`.25` AI 进程重启后只保留 1 个任务，DB 仍 29 个 running；
  新增 `TaskManager::reconcile_from_backend()`，启动 2s 后按 DB `status=1` 自动补启
  （实测 `task reconcile: started=29 failed=0`），AI 恢复 29~30 running。
- AI `start/stop` 改幂等：已存在返回 ok、不存在 stop 也返回 ok，避免 dotnet 把
  重复操作变成 500。
- 告警图片/视频加载失败：API 返回绝对 `/data/...`，前端又多拼 `/aibox/`，导致
  `/aibox//data/...` 404；Index.vue/TableModal 改为 `/data/` 直接使用，实测
  `http://10.10.206.25/data/...jpg` 200。
- 参数配置：从生产 `table_threshold`/任务 UserData 读取并写入 `tb_algo_config`：
  未戴安全帽 0.8、安全帽检出 0.5、V3 重复/灵敏度、反光衣 0.9、抽烟/打电话/玩手机
  0.4、人员基础 0.8、拥挤 0.3、人形最小 20/20/屏占比 0.3；解析器映射到
  `attr_thresh`/`conf`。
- V3：从生产 `libalg_plugin_minor_helmet.so` 提取 `minor_helmet.bmodel`，元数据
  `helmet_v3_v10`、256×256、1344×7（4 box+3 class）、ModelAttr offset40=15；
  新增 `helmet_v3` 模式（YOLOv10 后处理，alpha=1，target class1）。
  当前模型在全图上仍有多框误检，作为实验模式，未替换现有 helmet 任务。
- `.25` 最新二进制/模型/DB/frontend dist 已部署；任务按钮 PATCH 200，AI 29 running。

**验证**：`task reconcile: started=29 failed=0`；`/data/...jpg` 200；`helmet_v3` 模型
可加载；30 任务 DB 与 AI 对齐。### 迭代 12 — 2026-11-07 — .25 ISC 协议、开始/停止遮罩、每目标状态、救生衣替换

- `.25` 容器 `AIDETECT_ISC_PROTOCOL` 实际为 `rtsp`，导致 ISC 全部 `missing data.url`；
  切回 `rtmp` 后 30/30 running，恢复 6~8 路 decoder（RTSP 554 连接）。
- 任务管理开始/停止无响应：DOM 中存在 `SecretModal` 的空 `.n-modal` 遮罩（外部
  warning 图片加载失败、不可见），拦截所有点击。已从 `App.vue` 移除 SecretModal，
  并给 DeleteModal/PathSearch/DialogModal 加 `display-directive="if"`；浏览器注入
  JWT 实测点击“停止”发出 `PATCH /api/Task/UpdateTaskStatus/.../2`，DB status=2。
- 告警状态改为每目标独立：`ModelRuntime::TargetState` 向量，IoU/中心距匹配；
  新目标从第一次 seen 起独立 3s 蓝框倒计时，已确认目标红框常驻，目标消失 1s
  清除自身状态，不影响其它目标；蓝框/红框/倒计时按每个 Detection 输出。
- 5 个 `lifejacket_our` 任务（淮河 14K/25K/28K、八所中/北）全部替换为
  `boat_life_jacket`；DB 校验 lifejacket_our=0、boat_life_jacket=5。
- crowd 逆向：`CrowdMethod::process_people_head` 对 96×160×2 的 class logits
  做 softmax（2 类），第二类 > 阈值即人头候选；output1 为 offsets。已实现
  `CrowdHead` 后处理/`crowd_head` 模式（阈值 0.5），本地 H264 视频
  `model=crowd_head detections=1` 验证跑通。
- `.25` 最新二进制/DB/前端 dist 已部署，30/30 running、0 error。

**验证**：`PATCH .../2` 返回 200；DB `status=2`；`.25` `/task/list` 30/30；
`boat_life_jacket` 关联 5；`crowd_head detections=1`。### 迭代 11 — 2026-11-06 — 生产源码搜索、boat ROI、crowd 决策

- 在生产 `/data /edge /opt /usr/local /home /root /srv /Code` 搜索
  `board_algo_sdk.cpp`、`CrowdMethod*`、`CowCntMethod*`、`HeatMap*` 及头文件，均未找到；
  离线 `libEdgeSystem.so` 只有符号/DWARF。
- 按用户决定：找不到源码就不接 crowd 密度后处理，人员聚集继续用 people 计数；
  `crowd_20250424` 权重保留，密度解码留作后续。
- boat 救生衣接入 YOLOv8 解码；无 ROI 71 框，ROI 放 `model.taskArea`
  （`.50-.65/.45-.65`）后降到 **2 框**；该片段未连续 3s，暂无干净告警图。
- 生产 gdb 临时 attach 只读确认 `VERSION_V8`；HeatMap 断点导致一次 SIGABRT/自动重启，
  已恢复 80/10002/1935，临时 gdb 已删除，后续只离线分析。
- `.25` 30/30 running、0 error；6~8/30 出图属项目部网络问题。

**验证**：`docker` / `/task/list` 30 running；生产 1935 连接 23；boat 日志
`detections=2`（ROI 后）。### 迭代 10 — 2026-11-06 — 任务参数精简、蓝框倒计时状态机

- 前端任务编辑页移除 5 个红框字段（间隔帧数/触发帧数/上报间隔/告警视频时长/触发区间）；
  只保留算法配置。全局“告警上报间隔（分钟）”“告警视频时长（秒）”移到参数配置页，
  写入 `tb_param_config`：`AlarmReportIntervalMinutes=3`、
  `AlarmVideoDurationSeconds=10`；任务保存时读取。
- 检测内部改状态机：目标达到 exceed_num 后 3s 蓝框倒计时；确认后红框常驻，
  目标消失 1s 才重置；上报按全局分钟间隔。移除了 detect_threshold/section 的
  逐帧窗口逻辑。
- 前端已重新 `pnpm build` 部署；实测任务编辑页不再出现 5 个字段，参数配置页出现
  两个全局参数；点击“更新” `PUT /api/Task/UpdateTask 200`，DB 保存
  `frame_interval=1, detect_threshold=30, detect_section=30, video_duration=10,
  cooldown_time=3`；30/30 running。
- 蓝框状态机在本机 NoVest 视频实测到蓝色框（6.9s/8.4s），红框常驻因目标
  检测断续未截到；倒计时数字标签还需要补。
- 生产 gdb：临时 /tmp gdb 只读 attach 确认 `VERSION_V8`，但 HeatMap 断点导致 app
  一次 SIGABRT/自动重启；已恢复 80/10002/1935，临时 gdb 已删除，后续只静态反汇编。
- 船舶救生衣：已提取 `boat_life_jacket.bmodel`；通过生产 gdb DWARF 确认
  `ModelAttr offset40=7` = `VERSION_V8`，按 YOLOv8 anchor-free 解码（4 box logits +
  6 class，grid 32/16/8，class3=未穿救生衣）已接入；生产视频可触发，无 ROI 时 71 个框；把 ROI 放到 model.taskArea 后
  降到 2 框（任务级 taskArea 不生效）。证据 `docs/assets/alg_boat_alarm.jpg`。
- 新增/更新：`docs/ALGORITHM_PLUGINS.md` 第 9 节、
  `docs/PRODUCTION_ALARM_LOGIC.md` 状态机说明。
- 206.25 覆盖部署：镜像/models/data.db/dist 已同步，.25 容器 30/30 running、
  6~8/30 路 decoder（用户确认生产同样只有 6~8 路有画面，属项目部网络问题）；.68 aidetect 已停；代理代码已复制并重启到
  `/data/CT/project/ai/aibox-proxy-68`；.25 还热更新了最新二进制
  （base-only 隐藏 + 蓝框倒计时）。详见 `docs/DEPLOY_25.md`。
### 迭代 9 — 2026-11-06 — 前端算法配置、任务迁移、蓝框告警与网络报告

- 前端源码：确认 `gh-aibox-dev/web` 就是 AI 盒子前端（`src/views/bi/task-management`、
  `services/api/task.ts`），部署 dist 在 `/data/transtation/nginx/project/aibox/dist`；
  任务停止按钮根因是 `n-data-table :loading` 覆盖层拦截点击；已改为
  `:loading="loading && taskData.length === 0"` 并重新构建部署；修复后停止/启动
  按钮可点击，`PATCH /aibox/api/Task/UpdateTaskStatus/{id}/1` 返回 200。
- 算法配置：在 dotnet `data.db` 的 `tb_algo_mode`/`tb_algo_config` 增加五种生产算法
  （人数监控（人头）、人员聚集、安全帽检测、反光衣检测、抽烟打电话检测）；旧六种
  改名加 `_our`，前端算法配置界面已验证显示、可选、参数含告警阈值。
- 任务迁移：29 个工地任务由 safetyhelmet/reflectivejacket/cigarette/person 迁到
  helmet/vest/smoking_call/people_gather；6488“数智技术中心-办公室”保留旧
  `safetyhelmet_our`/`reflectivejacket_our`，避免办公室场景误报。
- 标注/告警逻辑：`Detection.pending` + `update_alarm()` 返回触发状态；未确认目标
  画蓝框不画标签，确认后红框+标签；检测间隔 200ms→100ms，减少框滞后。
- 新增文档：`docs/ALGORITHM_VALIDATION.md`（五算法生产告警样本验证）、
  `docs/NETWORK_POLICY_68_25_46.md`（三机网络策略差异）、
  `docs/PRODUCTION_ALARM_LOGIC.md`（蓝→红逻辑）、`docs/PRODUCTION_PARITY.md`
  （与生产机制对齐度）。
- 验证：30/30 running；模型统计 helmet 30、vest 20、people 30、smoking_call 3、
  head 3、people_gather 3；前端截图确认新算法勾选；生产未改。
  - 批量回归：10 条生产 NoHelmet/NoVest 原图；安全帽顶部 70% + 整人框回退后 5/5
    ≥0.8，反光衣 30% 裁剪 4/5 ≥0.9，详见 ALGORITHM_VALIDATION；
  - 蓝框视觉：NoVest 视频 3.8s 截到未确认蓝框（`docs/assets/alg_blue_pending.jpg`）；
    7.9s 截到红框停在旧位置（`docs/assets/alg_stale_red.jpg`），确认为预览用过期
    检测结果；已加 300ms 新鲜度丢弃 + vx/vy 速度外推（常规 800ms，告警保存
    800ms），旧位置框不再长时间停留；5s 外推实测会过冲，已回退为 800ms；新增
    dedup_dets 同标签 IoU>0.5 去重，避免同目标多框/多标签。
- 预览实测：迁移后 15 分钟内仅 3 个任务 `decoder opened`，5 分钟内 77 条
  `gateway no video stream`；`.25` 到 ISC 554 有 14 条上游连接，但 .68 仍大量
  拿不到流；生产 207.46 为 25 路直连 1935。
- 遗留：`crowd` 密度输出未解码；前端 `web/` 已用 `pnpm install + pnpm build` 重新构建并部署到
  `/data/transtation/nginx/project/aibox/dist`（旧版备份 `dist.bak_20261106_alg`）；
  `.68` 仍依赖 `.25` 代理，ISC 预览路数受代理/会话限制。
### 迭代 8 — 2026-11-06 — 算法插件机制逆向、五种算法模型提取与 YOLOv5 集成

- 目标：逆向生产算法插件机制并评估；逆向人员聚集/人头/安全帽/反光衣/抽烟打电话
  五种算法的模型与调用；集成插件化机制；参考生产调整任务配置。
- 逆向（只读，生产未改）：
  - 插件 ABI：`get_plugin()` 返回嵌入 bmodel、`get_plugin_size()`、`get_desc()`（中文|英文）、
    `get_params()`（ModelAttr）、`get_extra()`（ModelParams：anchors/classes/scores）、
    `get_extern()`（normalize/toTensor/transpose JSON）、`library_unload()`；
    插件自包含，80+ 个 `libalg_plugin_*.so`。
  - 五种算法：head(`head_v10_20251031` 480×832→[8190,5])、crowd(`crowd_20250424`
    384×640→2×[15360,2])、helmet(`hel4cls20250317_20250922` 224×224→4 头[2,3,3,2])、
    behavior(`scv10_102_best` 256×256→[1344,9])、suit ReID 256×128→512。
    desc：头部检测、拥挤度识别、安全帽反光衣长短袖、抽烟打电话玩手机、工服识别。
  - 任务映射：minorId 4=抽烟打电话、5=反光衣、195=安全帽V3、224=人员聚集；
    阈值来自 `table_threshold`：未戴安全帽 0.8、未穿反光背心 0.9、未穿长袖 0.65、
    打电话/抽烟 0.4。
  - 输出格式实测：YOLOv5 `[x1,y1,x2,y2,obj,class_logits...]`，坐标为输入图绝对像素；
    head 在生产 NoHelmet 视频首帧给出 obj=0.83 的头部框。
- 改动：
  - `src/model.hpp`：新增 `ModelType::YoloV5`、`input_w/input_h/class_logits`；
    新增 head/people/behavior 三个生产模型项，指向 `models/BM1684X/plugins/*.bmodel`。
  - `src/task.hpp`：`detect()` 按每个模型自己的输入尺寸做 letterbox；新增
    `postprocess_yolov5()`（xyxy+obj+class logits，sigmoid 类别，NMS，坐标回映射）；
    本地文件网关加 `-re`，否则探针读不到。
  - `tools/model_probe.cpp`：独立模型探针（输入/输出 shape、YOLOv5 解码、分类 softmax），
    用于逐模型验证。
  - `docs/ALGORITHM_PLUGINS.md`：插件 ABI、五模型、阈值、任务映射、优劣评估、待办。
- 验证：
  - `bmrt_test` 逐个加载提取出的 head/people/behavior/helmet/crowd/suit bmodel 成功；
  - 生产 NoHelmet 告警视频跑 head：`model loaded input=1,3,480,832`、
    `detections=2`、告警图两个 Head 框位置正确（`docs/assets/alg_head_alarm.jpg`）；
  - helmet：生产 NoHelmet 视频 `count=1`，告警图正确框出未戴安全帽人员
    （`docs/assets/alg_helmet_alarm.jpg`）；
  - vest：生产 NoVest 视频 `count=1`，告警图正确框出未穿反光衣人员
    （`docs/assets/alg_vest_alarm.jpg`）；
  - people_gather：NoHelmet 视频 `count=2`，按人形检测计数触发；
  - head_count：NoHelmet 视频 `count=2`，按人头数触发；
  - behavior：修正为 0..255 输入、过滤超大误检、头肩扩展（宽 3×/高 2.5×）；
    生产 Smoking 视频 `count=1`，告警标签“抽烟/打电话”正确；
  - 插件机制评估：生产自描述插件优于原硬编码，需吸收“插件自描述 + 注册表”，
    但不必照搬 80 个插件的体量。
  - 插件启动加载器：`ensure_plugin_models()` dlopen `libalg_plugin_*.so`，用
    `get_plugin/get_plugin_size` 提取嵌入 bmodel；删除 head.bmodel 后启动日志
    `plugin model extracted: head 6358120`，文件自动重建；
  - 五算法全部告警验证：head 2 框、helmet 未戴安全帽、vest 未穿反光衣、
    people_gather count=2、smoking_call count=1（`docs/assets/alg_smoking_alarm.jpg`）；
  - 新增 `GET /algorithm/list`，返回 head_count/people_gather/helmet/vest/smoking_call
    及参数定义；容器实测返回 5 条；30/30 running。
  - 新镜像已构建并部署，30/30 任务 running；测试后已恢复，占用约 `cpu=32 mem=15`。
- 遗留：
  - crowd 密度输出解码尚未接入，人员聚集暂用人形计数实现；
  - 前端任务配置页源码不在本仓库（web 是通用模板）；后端已新增
    `GET /algorithm/list` 返回五种算法的参数定义，前端可直接按该结构渲染。
  - 容器测试后已恢复到 30/30 running。
- 下轮：完成 helmet/vest + behavior + crowd 接入与生产视频回归；补前端参数/接口。
### 迭代 7 — 2026-11-05 — 告警视频对齐、ISC 协议逆向、单连接重构

- 目标：逆向生产告警视频/ISC 方案；修复告警图片与视频时间段对不上；解释 multi 组隔离
  问题并评估与生产的一致性。
- 逆向（只读）：
  - 生产告警：`AlarmRecorder` + `libdump4`（`FrameBuffering`/`dump_task`/
    `Mp4V2Writer`），配置 `VideoRecordSec=10`、回溯比例默认 0.5（前 5s/后 5s）、
    `VideoRecordLimitMB=20`；`AlarmRecorder::isSameRectAlarm/lastAlarmTime`。
    生产告警实测：图片 OSD 11:55:49，视频 11:55:43–11:55:54（图片在中间）。
  - 生产 ISC：DB `MediaUrl` 为
    `isc://...?protocol=rtmp&transcode=1&videotype=h264&https=1`；代码按
    `"protocol":"%s","streamType":%d` 请求 Artemis；生产到 `10.10.69.250:1935`
    有大量 RTMP 连接，无相机 554 直连。
- 复现旧问题：旧 ring 实现（20261105_185715）图片 OSD 18:57:24，视频 18:57:10–20，
  **视频比图片早结束 4s**；根因是按 mtime 取最新分片 + `-reset_timestamps` + 分片落盘
  滞后，没有按告警帧 PTS 取窗；异步 `dump()` 还被立即 finalize 导致丢文件。
- 改动：
  - `src/task.hpp`：单模式新增 **单媒体源网关**：每任务一个 ffmpeg 只拉一条上游
    RTSP/ISC，`-c copy` 复制成两路本地 MPEG-TS/UDP；`sail::Decoder` 读一路、
    `AlarmRingRecorder` 读另一路。这样只占用相机/ISC 的一个会话，且解码帧与包环
    共用同一 PTS，没有双连接时钟偏移；断流重连重新 resolve/启动网关并重启包环。
  - `src/task.hpp`：告警包环取触发前后 5s；`save_alarm` 在渲染图片前记录触发时刻；
    `AIDETECT_ALARM_BIAS_MS` 默认 0（网关下无需补偿）；写完后直接 `rename` 成 MP4，
    不再用 ffmpeg `-c copy` 二次封装（会丢帧）。
  - 尝试过 `sail::Decoder::dump(5,5)`：时间对齐，但在 30 路 + 1440p 下触发
    `free(): invalid pointer`（libavutil `av_buffer_unref`）导致容器重启，已放弃。
  - `src/isc.hpp`：解析并转发 `protocol/transcode/videotype/https`，默认按 URL 的
    protocol；`AIDETECT_ISC_PROTOCOL`/`AIDETECT_ISC_RTMP_PROXY`/
    `AIDETECT_ISC_ALLOW_DIRECT_RTMP` 控制 RTMP/RTSP 与转发。
  - `src/alarm_recorder.hpp`（新增）：按 PTS/wall-clock 的时间戳编码包环，预 5s，
    告警后重封装 MP4，不转码。
  - `docs/ALARM_VIDEO.md`、`docs/ISC_STREAM.md`：逆向证据、旧/新对比、网络结论。
- 验证：
  - 告警对齐（6488，2560×1440@25，容器 30 任务）：图片 OSD **19:59:43**，视频
    9.96s/250 帧；第 5 秒帧 OSD **19:59:42**，画面中白衬衫人员位置与告警图一致，
    即图片位于视频正中（±1s）。对比旧实现：图片 18:57:24，视频 18:57:10–20。
  - dump 方案对照：图片 OSD 19:12:22 落在视频 19:12:17–27，但 30 路下崩溃，不采用。
  - 网关预览：6488 单通道 1920×1080/**9.88fps**，79/79 不同帧、重复帧 0。
  - ISC：`t_isc2`（camera 1aae，1280×720）单通道 **9.88fps**、79/79 不同帧、
    重复帧 0；25K（23595aa2）Artemis 解析成功但 RTSP 网关 500（ISC/相机侧）。
  - 30 任务部署：`AIDETECT_DECODE_MODE=single`、`AIDETECT_ALARM_RING=1`、
    `AIDETECT_NO_DUMP=1`、`AIDETECT_REC_RING_MB=16`、
    `AIDETECT_ALARM_BIAS_MS=0`、`AIDETECT_ISC_PROTOCOL=rtsp`；30/30 running，
    6488 网关 1 条上游 + 2 路本地 UDP。
  - 30 路最终部署：**7 路 decoder opened = 6 路 ISC + 1 路直连 RTSP（6488）**；
    ISC 1aae 1280×720/**9.83fps**，6488 1920×1080/**10fps**；其余 23 路为 ISC 相机
    离线/`500`/`missing data.url`。容器无重启。
  - 第二轮告警复验（20:28）：图片 OSD 20:28:53，视频第 5 秒帧 20:28:51，场景一致；
    9.96s/250 帧/2560×1440@25。
  - ISC 告警复验（21:20，fefe86ed）：图片 OSD 21:20:12，视频第 5 秒帧 21:20:12，
    1280×720/249 帧/9.946s，图片居中。
  - 网络：206.68 → 10.10.69.250 的 **83/443/554/1935 直接连接仍 refused**；
    206.25 原有 10443/10444，2026-11-05 21:01 已新增 **10445 → 1935**，.68 可连；
    HLS(83) 仍不可达；生产 207.46 → 1935 大量 ESTAB。
  - 隔离 RTSP 测试：HEVC 3200×1800 相机 02109e 可 `decoder opened`，预览
    1920×1080/4fps；单路 1aae 9.67fps。
  - RTMP：单路经 .25:10445 读到 h264/1280×720/25fps，预览 8.5fps；但 30 路同时
    RTMP 只有 1 路 ISC 出图（ISC RTMP 会话/转码容量），因此最终部署用 **RTSP 为主**。
  - 网关探测从 2×2.5s 放宽到 **3×4s**，失败改指数退避；高分辨率/重编码 ISC 不再被
    误判为无流（这是 6 路 ISC 恢复的关键）。
- 遗留：
  - 206.68 到 ISC 的网络 ACL 仍需运维放行 443/554/1935，或在 .25 增加 1935 转发；
    否则无法切生产同款 RTMP，也无法证明 30 路出图率与生产一致。
  - multi 模式的组隔离仍未修复（KI-020）；multi 未走网关，告警仍走包环/worker 回退。
  - 告警去重（同框 isSameRectAlarm）与 `VideoRecordLimitMB` 尚未实现。
  - 冷启动前 5 秒内没有预录（生产同样需要预热，KI-024）。
  - 包环从非关键帧开始时个别播放器首帧可能花屏；建议后续强制首 GOP IDR。
- 下轮建议：网络放行后切 RTMP 复测；实现 multi 的失败通道隔离；补告警同框去重。
### 迭代 6 — 2026-11-05 — 预览真 10fps/1080p 上限、按需编码、生产方案逆向

- 目标：按 2026-11-05 需求重构实时预览：告警视频保持源帧率/分辨率；单通道与 3×3 合流
  输出**真内容 10fps**；源 >1080p 缩到 1080p，≤1080p 不变；源 >10fps 抽帧，≤10fps 不变；
  同时用 rea/只读逆向生产盒子 `10.10.207.46` 的技术方案。
- 逆向（只读，未改生产任何文件/配置）：
  - 生产 `./app` 单进程 1043 线程、RSS 1.4 GB，输入是 `10.10.69.250:1935` 的 RTMP，
    监听 554/1935/58000/10002/10010/10011/9090；
  - `libEdgeSystem.so` 字符串/RTTI：`JpegEncoderInstance`、`MJpegInstance`、
    `MergeInstance::markJpegVideoOutput`、`DecodeInstance::requireScaled/notifyScaled/
    revokeScaled`、`realtime_scale.width/height`、`Jpeg Stream Session`、
    `QualityOfPreview`、"web page plays videos via JPEG stream"；
  - `libboard_sdk_bm.so`（带 DWARF）：`bm_decoder::set_scaled_size`、
    `base_frame_resize`、`create_jpeg_encode(int,int,int,int)`、`bm_jpeg_encoder::process`；
  - 还原方案：每路只解一次码，预览按观看者请求缩放（`requireScaled`），每任务独立硬件
    JPEG 编码实例，`MergeInstance` 做 3×3 合流 JPEG 输出。
- 改动：
  - `src/server.hpp`：`StreamHub` 增加 `note_subscribe/note_unsubscribe/wanted`；
    `WsSession` 订阅切换/断开时更新需求计数；`Aggregator` 改为**有订阅者才拼图**、
    硬件 `bmcv.imdecode + image_copy_to + imencode`、按内容更新发送（≤10fps，无重复帧）、
    空槽 `no_task.jpg`；`TaskManager` 注入单通道/合流/需求回调。
  - `src/task.hpp`：删除第二套 ffmpeg 预览管线；新增 `preview_target`（1920×1080
    只缩不放）、`preview_due`（令牌桶真 10fps）、`maybe_preview`（单通道/合流/1fps
    area 三种按需路径）、`render_preview_fast`（VPP 缩放 + bmcv 画框 + 中文标签
    plate + JPU JPEG）、`render_tile_fast`（640×360 合流瓦片）；告警视频改为**单/多
    解码模式都优先环形录制 `-c copy`**，失败才回退 dump/worker；源打开失败加指数退避
    （1.5s→3s→…→30s），避免大量失败 ISC 通道持续抢占 VPU、拖慢正常通道解码。
  - 稳定性/性能配套：检测统一走 infer 线程（single 模式不再 inline detect，避免 30 路时
    TPU 争用阻塞解码）；`/task/get_area_frame` 改为请求触发的 full-size 渲染（2s TTL +
    force），平时不再每秒插一次 1080p 渲染；`Aggregator` 增加瓦片解码缓存（状态瓦片不变
    时只做设备拷贝）；源打开失败指数退避（1.5s→30s→2min）。  - `docs/PREVIEW_PIPELINE.md`：逆向证据、方案对比、代码落点、验证。
  - 新增验证工具（工作区 `.work`，不入仓库）：`measure_ws.mjs`（按 JPEG SHA1 统计
    真实内容帧率/重复帧/分辨率）、`tasks_ctl.mjs`、UDP 实时源回放。
- 验证：
  - 构建：`cmake --build build/aidetect -j8` → `BUILD_OK`（2026-11-05 16:23，1.7 MB）。
  - 单通道 2560×1440@25（真实告警视频/6488 摄像机）→ **1920×1080，10.0/9.9fps，
    100/100、99/99 不同帧，重复帧 0**。
  - 单通道 1920×1080@25 → 1920×1080，10.0fps，100/100 不同帧。
  - UDP 实时 11fps / 704×480 → 704×480，10.0fps，100/100；UDP 实时 9fps / 704×576
    → 704×576，9.0fps，90/90（不补帧/不抽帧）。
  - 3×3 合流（3 路活动）→ 1920×1080，9.8fps，98/98 不同帧，重复帧 0。
  - 容器 `aidetect:dev`（single 解码，30 个后端任务，最终镜像）：6488 直连摄像机
    2560×1440 → 1920×1080 / **9.88fps / 79/79 不同帧**；6488 所在 `all_3` 合流
    1920×1080 / **9.75fps / 78/78 不同帧**；ISC 可达时 704×576、1280×720 单通道
    9.88fps；均无重复帧。`/get_occupancy` `cpu=8~11 mem=12 tpu=12~34`。
  - 告警视频：环形分片 `h264 2560×1440 25/1`；`-c copy` 10s 告警片段
    `2560×1440, 24.92fps, 10.03s`，与源一致。
  - 视觉：`docs/assets/preview_single_2560x1440_to_1080p.jpg`（1920×1080 无放大）、
    `preview_single_704x480_label.jpg`（704×480 + 红框 + "未穿救生衣"中文标签）、
    `preview_single_1080p_boxes.jpg`（5 个检测框）、`preview_merge_1920x1080.jpg`；
    `ui_preview_20261105.png` 为 Web 端实时预览页实测（通道1 九宫格：真实通道 +
    状态图 + "任务未配置"），前端无需改动。
  - 生产只读证明：`docs/assets/prod_nochange.txt`（`edge_global.yaml` mtime 仍
    `2025-01-01`，`app`/`libEdgeSystem.so` mtime 仍 `2025-12-15`，关键 md5）。
- 遗留：
  - 30 路部署中只有 6 路源可达（其余 ISC `missing data.url`/`500`，KI-017），
    合流内容帧率受可用源数量限制；多路 ISC 恢复后应复测 4 组合流。
  - `multi` 解码在大量源失败时会把组内解码线程拖死（本次改用 single 部署），
    需要后续做"失败通道隔离/组重建"。
  - 生产 Web 登录凭据不可用，未做生产 UI 端到端采样；逆向证据来自二进制/符号/配置文本。
  - 预览 JPEG 质量目前是 SAIL 默认，1080p 单通道约 4.3 MB/s；后续可加 `QualityOfPreview`
    等价的质量档位。
- 下轮建议：修复 multi 组失败隔离；加预览质量/带宽档位；用恢复的 ISC 通道复测 4 组合流。

### 迭代 5 — 2026-11-02 — 五模型、ISC、告警视频、多实例/36 路、硬件合流、Compose

- 目标：完成第二步（4）（5）剩余六项：Damo 安全帽/救生衣/抽烟/人员、告警 5+5 视频、ISC、模型多实例、36 路压测、硬件加速合流、Compose。
- 改动：
  - `src/model.hpp`：`ModelSpec` 注册表（5 类模型 + 反光衣天空）、`ModelInstance` 多输出、`ModelScheduler` 按 busy 选实例/扩容（`AIDETECT_MAX_INSTANCES`，默认 2）。
  - `src/task.hpp`：多模型循环；YOLOv11 与 DamoYOLO 两套后处理；Damo 分数/框输出解析、逐类 NMS、letterbox 反映射、ROI；每模型独立告警窗口/图片/文案；RTSP `decoder.dump` + 继续读 post 秒 + ffmpeg 封装；中文类别标签。
  - `src/isc.hpp`：ISC URL 解析、HMAC-SHA256 签名、HTTPS POST（verify_none，兼容旧实现）、`data.url` 提取；Task 打开前解析。
  - `src/server.hpp`：`Aggregator` 改硬件链路：`bmcv.imdecode` → `bmcv.resize` → `bmcv.image_copy_to` → `bmcv.imencode`；dirty 标记避免空组合流占用 JPU；`TaskManager` 支持本地 mock 任务 ID；debug 触发接口（仅 `AIDETECT_DEBUG`）。
  - `docker/Dockerfile`：补 `/opt/sophon/libsophon-current` 等软链（bmlib 硬编码路径）；Compose v2.29.7 安装并实跑。
- 涉及：`src/model.hpp`, `src/task.hpp`, `src/isc.hpp`, `src/server.hpp`, `src/main.cpp`, `CMakeLists.txt`, `docker/**`, `README.md`, `docs/**`
- 验证：
  - 编译：`cmake --build build/aidetect -j8` → `BUILD_OK`。
  - Damo：`safetyhelmet` 日志 `detections=7~9`；`person` `3~6`；`cigarette` 在 conf 0.1 下 `1`；`lifejacket` 用告警图裁剪放大后 `1`。真实任务 `6488...` 同时加载反光衣+安全帽，area 54 万字节。
  - 告警视频：RTSP 调试告警生成 `/data/.../6488.../reflectivejacket/video/20261102_194653.mp4`，`ffprobe` 显示 H.264 2560×1440、9.64 s、1.44 MB；后端告警详情 `videoPath` 指向该文件。
  - ISC：本地 HTTPS mock 收到 `x-ca-signature` 校验 `sig_ok=True`，服务日志 `ISC resolved to rtsp`，area 542,296 字节；真实 `10.10.69.250:443` 从盒子拒绝连接（KI-017）。
  - 2026-11-03 复核：8 个可用 ISC 任务（25K/八所/三亚×2/凤凰咀×2/宁波 Y2/龙港）配置均解析到 `10.10.69.250:443`，盒子 TCP 全部 `Connection refused`；另一台相同配置盒子可用，说明是本机到 ISC 的网络路由/VPN 差异。
  - 2026-11-03 救生衣：`reference/video` 三段 `未穿救生衣` 视频，`lifejacket` 分别 `detections=2/4/1`，日志 `docs/assets/lifejacket_ref.log`。
  - 2026-11-03 真实 ISC 打通（经 .25）：开发盒到 `10.10.69.250` 被拒，用 `10.10.206.25` 的 10443/10444 TCP 代理后，8 个通道中 7 个出图（25K/三亚×2/凤凰咀×2/宁波/龙港）；八所预制区 RTSP open 失败；截图 `docs/assets/isc_real_25k.jpg`、`isc_real_museum.jpg`。
  - 2026-11-03 Web UI 验收：Playwright 登录 `testuser`，任务管理 30 条分 3 页全部运行（每页 10 running/0 stopped），AI `/task/list` 30 running/0 error；截图 `ui_tasks_page1/2/3.png`、`ui_home.png`、`ui_preview.png`、`ui_alarm.png`，详见 `docs/UI_ACCEPTANCE.md`。
  - 2026-11-04 前后端源码已放入 `gh-aibox-dev/web`、`gh-aibox-dev/api`；核对前端 `sendKey(taskId)` 确实存在，撤销此前“前端未订阅”的判断。
  - 2026-11-04 发现镜像字体仍是 7KB subset，导致中文显示成点/缺字；已把完整 17MB `wqy-zenhei.ttc` 上传并重建镜像，聚合空槽恢复黑底白字状态，检测标签红底白字且“戴”正常。
  - 2026-11-04 当前部署 `single` + 30 任务 running + 告警视频启用；预览无画面的主因是 ISC `previewURLs` 返回缺少 `data.url`，已串行化 ISC 解析仍复现，需 ISC 侧确认相机预览 URL。
  - 2026-11-04 当前稳定部署为 `multi + AIDETECT_NO_DUMP=1`、30 任务 running；`single` 30 路在解码/推理阶段 double-free，故 30 路与告警视频不能同时满足，需产品选择限路或关闭视频。
  - 2026-11-04 检测标签改为红底白字并放在检测框右下角；完整字体下“未戴安全帽”正常。
  - 2026-11-04 前端源码确认 `sendKey(taskId)`；206.25 的 `params_init.py` 实际硬编码 `rtsp://...207.241`，不是 ISC；206.68 的 `isc://` 仍因 ISC `missing data.url` 无画面。
  - 2026-11-04 C++ ISC `x-ca-nonce` 改为 UUID v4 后，14K 单任务实测 `ISC resolved to rtsp` + `multidecoder opened shape=1,3,720,1280`；其余 `missing data.url` 是 ISC 侧无 URL。
  - 2026-11-04 C++ ISC nonce 改 UUID 后 14K 解析成功；`--dump-only` 子进程在 dump 后继续读 post 窗口，实测 504KB/3.8s/640×360/25fps。
  - 2026-11-04 Task 外层改为重连循环；ISC/网络/解码失败只显示“正在重连”，任务保持 running。当前 30 running/0 error。
  - 2026-11-04 标签交付：主机 Pillow 服务 9070 按 `draw_bmcv_chinese` 绘制红底白字，告警图 181931 实测带白字；C++ 通过 `AIDETECT_LABEL_URL` 调用。
  - 2026-11-05 合流改 CPU OpenCV：Aggregator 用 cv::imdecode/resize/copy/imencode 并行 4 组，去掉 bmcv JPEG 硬件锁；30 路运行稳定，all_1 WS 117 帧/12s，待验证内容刷新。
  - 2026-11-05 单通道预览已由独立 FFmpeg pipeline 提供：14K 96 帧/10s、25K 95 帧/10s；6488 preview 1920×1080、14K 1280×720、25K 704×576。
  - 2026-11-04 标签已改为单一红条白字，无蓝条重复；告警图 194738 验证通过。
  - 2026-11-04 worker 持久化：aibox-label.service、aibox-video.service 已 enable/start；标签用 User=linaro（Pillow 在 linaro 环境）。
  - 2026-11-04 当前部署 30 running，标签/视频主机 worker 运行；下一步补 systemd 持久化和完整 4 worker 拆分。
  - 2026-11-04 合流未分配槽位改为 `no_task.jpg`“任务未配置”，不再纯黑；标签改到检测框左上角红底白字。
  - 2026-11-04 状态心跳：每个 Task 1s 检查无帧则刷新状态图，状态图时钟移到顶部避免被裁；Aggregator 每 500ms 推送。当前 30 running；14K 等可用通道仍受 ISC/远程网络波动影响。
  - 2026-11-04 30 路 + 视频：子进程单任务可生成 504KB/3.8s；接入 30 路后 `av_packet_free_side_data` 崩溃，已回退 `multi + NO_DUMP`，当前 30 running/0 error，告警视频暂关。
  - 2026-11-03 UI 修正：状态图改为黑底白字居中并换行（`ui_status_centered.jpg`）；首页温度接口返回 59/51℃；单通道 1920×1080/2fps、合流 1920×1080（640×360 tile）/2fps、告警图 1920×1080、25K 告警视频 2560×1440/25fps。
  - 2026-11-03 预览定位：浏览器只订阅 `all_1`，任务通道点击没有重新订阅 taskId；数智直连 RTSP 可用，ISC 单通道需前端改走 AI WS 或代理。
  - 2026-11-03 告警图片修复：nginx 容器补 `/data:/data:ro` + `/aibox/data/ -> /data/` alias，告警卡片图片恢复；上报状态失败属于后端外部上报链路。
  - 2026-11-03 网络诊断：`.25` DHCP/可访问、`.68` static/被 RST；本机无 iptables/rp_filter 差异，报告 `docs/NETWORK_REPORT_25_vs_68.md`，需网络/安全运维放行。
  - 2026-11-03 32 路 1080p/10fps：`multi` + 640 预览，32 个任务全部 running、stop 后 `running=[]`，CPU 44%、TPU 6%、内存 888 MB。
  - 2026-11-03 断流状态：坏流任务保持 running，单通道/合流预览输出黑底白字 `监控视频流状态` + `正在连接监控视频流...`，实测 53,000 字节 JPEG（`docs/assets/status_reconnect.jpg`）；重连逻辑每 5 次失败重建 decoder。
  - 2026-11-03 ISC multi：`loop=-1` + 阻塞 read 后，ISC 解析 RTSP 可在 multi 读帧（25K area 285,480，`docs/assets/isc_multi_25k.jpg`）；multi RTSP 告警 `Decoder::dump` 崩溃，ISC 生产仍用 single。
  - 2026-11-03 36×1080：`AIDETECT_DECODE_MODE=multi` + 640 预览，36 个任务全部 running、stop 后 `running=[]`，服务存活；纯解码 4×9 通道 150 帧/通道 8.24 秒、RSS 129 MB。MultiDecoder dump 补丁对本地 640 模式生成 1.07 MB 告警视频；RTSP/ISC 的 multi 视频仍崩溃。
  - 2026-11-03 4 组 MultiDecoder 尝试：新增 `decode_pool.hpp` + 本地 `sail` MultiDecoder dump 补丁，36×640 全部 running/停止正常；36×1080 纯解码压测稳定，接入模型/预览/合流后早期版本在停止阶段触发 `Jpu_GetError`/`libavutil` double free；当前用 640 预览 + multi 模式稳定，生产默认单解码保留视频能力。
  - 36 路：36 个任务全部 running，启动 1 秒；30 秒后 RSS 375 MB、CPU 331%、TPU 24%；日志有 **2 条 `model loaded: safetyhelmet`**；停止后 `running=[]`；`all_1..all_4` 分别 676,112 / 677,760 / 679,280 / 675,128 字节 JPEG。
  - 硬件合流：`all_1_36.jpg` 为完整 3×3 九宫格；不再 CPU `cv::imdecode` 拼图。
  - Docker：镜像 `aidetect:dev` 901 MB；容器内任务/area/WS 通过；Compose v2.29.7 `config`、`up -d`、`ps` 通过，Compose 任务 area 542,152 字节。
  - 运维：多次强杀导致 JPU 编码锁死；重启盒子后恢复（JPU refcount 0），之后测试全通过。
- 证据：`docs/assets/damo_safetyhelmet.jpg`、`damo_safetyhelmet_alarm.jpg`、`service_aggregate.jpg`、`alarm_video_frame.jpg`、`service_video.log`、`service_load.log`、`isc_mock.log`、`docker_final.log`。
- 遗留：
  - 真实 ISC 需经 `10.10.206.25` 的 10443/10444 代理；25K/三亚/博物馆已验证，八所仍失败。
  - 36 路 640×360/1fps/2fps 稳定；36×1080 用 `AIDETECT_DECODE_MODE=multi` + 640 预览 36 路 running/stop，默认 single + HighResGate 12 路。
  - 为降低 JPU 并发崩溃，新增全局 `jpeg_hw_mutex()` 串行化 Task 的 `cv::imencode` 与 Aggregator 的 JPEG decode/encode（ADR-021）。
  - 最终镜像重建并 Compose 部署；最终任务 area 536,920 字节，stop 后 `running=[]`。
  - 多实例调度已补齐空闲回收：`AIDETECT_MODEL_IDLE_SECONDS=5` 测试中 `/debug/scheduler` 从 2 实例变为 1 实例，日志 `model instance reclaimed: safetyhelmet`。
  - 救生衣已用裁剪样本验证 `detections=1`；真实场景视频仍待补。
  - 告警视频只支持 H.264/H.265；mpeg4 本地视频 dump 会失败。
  - HTTP/WS 仍线程/连接；GC/磁盘配额未做。
- 下轮建议：真实 ISC 联调 → 1080p/10fps 容量准入 → 实例回收 → async/线程池 → UI 实测。

### 迭代 4 — 2026-11-02 — C++ 服务 MVP + Docker 部署

- 目标：按第二步（4）（5）开始执行：写 C++ 服务端代码，打通 HTTP/WS、真实后端任务、FP16 反光衣推理、预览/合流/告警图，并构建 Docker 镜像验证。
- 改动：
  - 新增 `CMakeLists.txt`、`src/common.hpp`（JWT/HTTP client/配置/metrics）、`src/model.hpp`（共享 `sail::Engine`）、`src/task.hpp`（解码/预处理/推理/NMS/ROI/告警/预览）、`src/server.hpp`（Beast HTTP/WS、4 组合流、任务管理、接口）、`src/main.cpp`。
  - 新增 `docker/Dockerfile`、`docker/docker-compose.yml`、`docker/lib/libcrypto.so.1.1`/`libssl.so.1.1`、`.dockerignore`。
  - 支持真实后端 `TaskAlgoConfigDetail`、RTSP、本地视频测试、`/get_occupancy`、`/task/start|stop|list`、`/task/get_area_frame`、`WS /ws/frame`、`all_1..4` 合流、告警图片 + 上报。
  - 反光衣 FP16 输入 `/255`；告警图写 `/data/CT/project/ai/inference/save/<task>/reflectivejacket/img/...`；预览/告警标签用 FreeType2 + 中文子集字体（`docker/fonts/wqy-subset.ttf`，7 KB）。
- 涉及：`CMakeLists.txt`, `src/**`, `docker/**`, `.dockerignore`, `README.md`, `docs/**`
- 验证：
  - `cmake --build build/aidetect -j8` → `BUILD_OK`；二进制 `build/aidetect/aidetect` 1.1 MB。
  - 真实任务 `6488bb6c-c3c2-4011-ac65-2b1f27921cc9`（后端配置 + RTSP 2560×1440）：
    - `POST /task/start` → `{"status":"ok","pid":<服务PID>}`；
    - `GET /task/get_area_frame` → `status=ok`，JPEG 533,040 bytes；
    - `WS /ws/frame` 订阅任务 → header `{taskId,imageType,ImageType,dataLength}`，JPEG 533,424 bytes；
    - 订阅 `all_3` → JPEG 140,032 bytes（单路 3×3 合流，其余槽位 NO TASK）；
    - `POST /task/stop` → `status=ok`；`/task/list` → `{"running":[]}`。
  - 本地反光衣测试视频（`/tmp/test_vest.mp4`，由数据集图片生成）：
    - `detections=7`；`ALARM count=3`；告警图 461,432 bytes；
    - `alarm reported` 成功（后端 `ReportAlarmEvent`）；`video` 字段为空（见遗留）。
  - `GET /get_occupancy` → `{"status":"ok","cpu":41,"mem":11,"tpu":2,"vpu_1":0,"vpp_1":0,...}`（已把异常 vpu/vpp 值钳制到 0）。
  - Docker：`docker build -t aidetect:dev -f docker/Dockerfile .` → 成功，镜像 901 MB；容器内加载模型、启动任务、area/WS 均通过；`ffmpeg` 软链到 SDK ffmpeg。
  - 部署：盒子已用 `docker run` 启动 `aidetect`（`--restart unless-stopped`），`GET /get_occupancy` 返回 ok、`/task/list` 为空；盒子没有 compose 插件，`docker-compose.yml` 未实跑（KI-014）。
  - 视觉证据：`docs/assets/service_preview.jpg`（7 框）、`service_alarm.jpg`（告警图）、`service_aggregate.jpg`（all_3 合流）、`docker_area.jpg`/`docker_ws.jpg`（容器内）；日志 `service_video.log`、`service_ws.log`、`docker_build.log`。
- 遗留（重要）：
  - 只支持 `reflectivejacket`；DamoYOLO 的 `safetyhelmet/lifejacket/cigarette/person` 后处理未移植，只含这些模型的任务会失败。
  - 告警视频未生成：`decoder.dump` 在本地测试视频上没有产出文件，当前 `video` 为空；需要在 RTSP 上验证或改硬件 Encoder 环形缓冲。
  - 模型调度目前是单个共享反光衣实例串行推理，没有多实例/动态扩容；36 路未压测。
  - 合流是 CPU `cv::imdecode` 拼图；4 组代码在，但只验证 1 路；36 路性能未知。
  - HTTP/WS 线程/连接（同步 Beast），不是最终异步方案；容器基于 `aibox-api:latest` + OpenSSL 1.1，镜像 901 MB。
  - 仅支持 RTSP/本地文件；ISC 解析未实现。
  - 10fps 限流是“每 100ms 取最新帧”，未做严格 token bucket 统计。
- 下轮建议：先补 DamoYOLO 后处理（安全帽/救生衣/抽烟/人员），再补告警视频和模型多实例调度；最后 36 路压测 + UI 实测。

### 迭代 3 — 2026-11-02 — 按负责人确认修正接口契约与文档目录

- 目标：把迭代 2 里标为"待确认"的接口/路径/入口项按负责人实际运行情况修正；不写业务代码。
- 改动：
  - **Token**：确认前端/后端自己签发 token，AI 只验证；删除 `/get_token`、`/refresh_token` 的需求与设计；AI 出站调后端仍用 `create_jwt("admin")`。
  - **WS**：固定 `9000`、合流 id `all_1..all_4`；header 同时写 `imageType`/`ImageType`，前端不用改。
  - **告警路径**：固定绝对路径 `/data/CT/project/ai/inference/save/...`；`DATA_ROOT` 默认该路径，部署验证成功后再统一改。
  - **线上入口**：确认是 `app.py + main.py + ai_package/`；`task_manager.py`/`package/` 只作旧代码参考。
  - **点 5 澄清**：`/get_occupancy` 只有 `vpu_1..3` + `vpp_1..2`，没有 `vpp_3`；`/task/start` 保留 `pid` 字段，新单进程返回服务 PID（前端只用 status/msg）。
  - **文档目录**：按当前 `重构AI盒子ai检测服务端.md` 的"其他 md 放 docs"，把 `docx/` 迁到 `docs/`，同步 README/AGENTS/所有链接。
  - 更新 `docs/INTERFACES.md`（重写）、`docs/SERVICE_TASKS.md`、`docs/SOLUTION.md`、`docs/REQUIREMENTS.md`、`docs/ARCHITECTURE.md`、`docs/CONVENTIONS.md`、`docs/DECISIONS.md`（ADR-012）、`docs/KNOWN-ISSUES.md`、`docs/DOMAIN.md`、`README.md`、`AGENTS.md`。
- 涉及：`docs/**`, `README.md`, `AGENTS.md`, `tests/yolov11_official_demo/run.sh`（注释路径）
- 验证：
  - `grep -rn "get_token|refresh_token|vpp_3|8000|docx" docs/INTERFACES.md docs/SOLUTION.md docs/REQUIREMENTS.md docs/CONVENTIONS.md README.md AGENTS.md` → 无旧契约残留（只剩"不提供 token/没有 vpp_3/固定 9000"等说明）；`PROGRESS` 迭代 2 保留当时的待确认记录，以迭代 3 为准。
  - `grep` 旧源码路由：`app.py` 6 条（start/stop/list/area/occupancy/ws），`ai_package/server.py` 1 条 ws；与 `docs/INTERFACES.md` 对齐。
  - `pwsh -NoProfile -ExecutionPolicy Bypass -File <skill>/scripts/check-project-memory.ps1 -Target gh-aibox-dev -DocsRoot docs` → `8 通过 / 0 警告 / 0 失败`。
  - 盒子复验仍通过：`bash tests/yolov11_official_demo/verify.sh` → `images=3 detections=14 per_class={0: 11, 1: 3}`，`PASS`。
- 遗留：服务端 C++ 代码仍未开始；`frameInterval`/`cooldownTime` 单位、`Decoder::dump` 行为、36 路解码/编码需在实现阶段实测。
- 下轮建议：直接进入 `SOLUTION.md` M1：工程骨架 + HTTP/WS + JWT 验证 + occupancy + mock 后端。

### 迭代 2 — 2026-11-02 — 旧服务盘点 + 接口/任务/整体构建方案

- 目标：按「源码重构第一步 + 第二步（1）（2）（3）」盘点旧 Python 服务，产出接口清单、服务任务清单、整体 C++ 构建方案；同时验证用户新上传的 FP16 反光衣权重。
- 改动：
  - 从盒子打包/下载旧 `inference/` 源码（仅 .py/.sh/.md，排除 4GB+ 日志），逐文件阅读 `app.py`、`main.py`、`ai_package/`、旧版 `package/`、`task_manager.py`。
  - 新增 `docs/INTERFACES.md`：AI 服务 HTTP/WS、后端配置/告警接口、JWT、数据结构、兼容性缺口。
  - 新增 `docs/SERVICE_TASKS.md`：旧进程/线程清单、单任务流程、配置触发、模型/告警/合流任务、18 条旧缺陷、新任务分解 T-01~T-18。
  - 新增 `docs/SOLUTION.md`：单进程线程流水线、4×9 解码/合流、共享模型调度、FP16、告警、Docker、性能预算、M1~M7 里程碑、风险。
  - 更新 `README`、`REQUIREMENTS`（FR-13/14/15）、`ARCHITECTURE`、`CONVENTIONS`、`DECISIONS`（ADR-007~011）、`DOMAIN`、`KNOWN-ISSUES`。
  - 验证新权重 `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel`（24.7 MB，md5 `13d8c91f...`），并更新测试脚本默认模型。
- 涉及：`docs/INTERFACES.md`(新), `docs/SERVICE_TASKS.md`(新), `docs/SOLUTION.md`(新), `docs/assets/new_fp16_probe.txt`(新), `docs/assets/perf_baseline.txt`(新), `README.md`, `docs/REQUIREMENTS.md`, `docs/ARCHITECTURE.md`, `docs/CONVENTIONS.md`, `docs/DECISIONS.md`, `docs/DOMAIN.md`, `docs/KNOWN-ISSUES.md`, `tests/yolov11_official_demo/run.sh`, `env/test_sail.sh`
- 验证：
  - 新 FP16 `probe_model.py`：`norm=255` → `output_shape=(8400,6) class_max=0.913574 class_nonzero=814 anchors>conf=62`；`norm=1` → `class_max=0.998535 anchors>conf=534`（误检）。证据：`docs/assets/new_fp16_probe.txt`。
  - `bash tests/yolov11_official_demo/verify.sh` → `images=3 detections=14 per_class={0: 11, 1: 3}`，`PASS: demo produced detections`；decode 15.202 ms / preprocess 1.668 ms / inference 5.214 ms / postprocess 0.245 ms。
  - `bash env/test_sail.sh` → `SAIL_OK graph=yolov11s`；`input 1,3,640,640`；`output output0_Concat_f32 1,6,8400`。
  - 人工看图 `docs/assets/step2_yolov11_detection.jpg`：穿反光衣的工人为 `vest`，未穿为 `no_vest`，框落在人身上。
  - 旧源码接口/线程盘点：`app.py` 927 行、`main.py` 91 行、`ai_package` 20 个 py；`grep '@app.(get|post|websocket)'` 得到 6 条路由（task start/stop/list、area frame、occupancy、ws），与 `INTERFACES.md` 第 2/3 节逐条对齐；`params_init.py` 的 `channelAddress/frameInterval/detectThreshold/detectSection/videoDuration/cooldownTime` 与文档字段映射一致。最新文件指向 `app.py + main.py + ai_package`，但 `ai_package` 存在未完成/不一致，`task_manager.py/package` 是早期实现；**实际部署入口待确认**。
  - 证据文件：`docs/assets/step1_sail.log`、`step2_verify.log`、`step2_result.json`、`new_fp16_probe.txt`、`perf_baseline.txt`。
- 遗留 / 待确认：
  - `/get_token`、`/refresh_token` 旧源码完全缺失；用户/密码校验来源未确认。
  - WS 端口 9000 vs 需求 8000；合流 id `all_i` vs `aggregate_i`；header 键 `imageType` vs `ImageType`。
  - 告警图片/视频路径相对 vs 绝对；后端消费方式未确认。
  - 旧配置接口字段与需求文档简化版不一致（实际 `code/data`、`algoCode`、`taskArea/algoConfig`）。
  - `decoder.dump(5,7)` 的阻塞/时长、36 路解码/编码/WS 带宽需 M2/M4/M7 实测。
  - Docker 离线构建/设备权限未验证。
  - 服务端 C++ 代码、接口实现、Docker 均未开始。
- 下轮建议：先确认 3 个阻塞项（token 用户源、WS 端口/合流 id、告警路径），再按 `SOLUTION.md` M1 搭工程骨架；不要先写业务逻辑。

### 迭代 1 — 2026-11-02 — 环境搭建步骤 1/2：C++ Bmodel 环境 + 官方 YOLOv11 验证

- 目标：在 BM1684X 盒子上装好 C++ 跑 Bmodel 的环境（sophon-sail），并用官方 C++ demo + YOLOv11 权重完成端到端验证。
- 改动：
  - 上传并解压 libsophon 0.5.3、sophon-mw 0.15.0、sophon-sail 3.11.0 到盒子 `env/`，自包含安装到 `env/sdk/`，未改 `/opt/sophon`。
  - 新增 `env/env.sh`、`env/setup_sdk.sh`、`env/test_sail.cpp`、`env/test_sail.sh`；安装后修正 `SAILConfig.cmake` 的 `find_library` 路径，使 `find_package(SAIL)` 可用。
  - 新增 `tests/yolov11_official_demo/`：官方 `YOLOv8_plus_det/cpp/yolov8_bmcv` 源码、3 张反光衣测试图、中英文类别名、`build.sh` / `run.sh` / `verify.sh` / `check_results.py`。
  - 建立 `docs/` 项目记忆（README、AGENTS、REQUIREMENTS、ARCHITECTURE、CONVENTIONS、DECISIONS、PROGRESS、DOMAIN、KNOWN-ISSUES）。
- 涉及：`env/**`, `tests/yolov11_official_demo/**`, `docs/**`, `README.md`, `AGENTS.md`
- 验证：
  - `bash env/test_sail.sh` → `SAIL_OK graph=yolov11s`；`input images shape=1,3,640,640`；`output output0_Concat_f32 shape=1,6,8400`。
  - `readelf -d env/build/sail/lib/libsail.so | grep NEEDED` → 含 `libbmcv.so.0`（修复了 native 构建漏链 bmcv 的问题）。
  - 临时 CMake 工程 `find_package(SAIL REQUIRED)` + `test_sail.cpp` → 配置/编译通过，运行输出 `SAIL_OK graph=yolov11s`（验证 `SAILConfig.cmake` 修正有效）。
  - `bash tests/yolov11_official_demo/verify.sh` → `images=3 detections=14 per_class={0: 11, 1: 3}`，`PASS: demo produced detections`。
  - 同次运行耗时（FP16，640×640）：decode 15.02 ms / preprocess 1.67 ms / inference 5.23 ms / postprocess 0.25 ms。
  - 人工看图 `docs/assets/step2_yolov11_detection.jpg`：穿反光衣的工人框为 `vest`，未穿的为 `no_vest`，框落在人身上。
  - 证据文件：`docs/assets/step1_sail.log`、`docs/assets/step2_verify.log`、`docs/assets/step2_result.json`。
- 遗留：
  - **INT8 反光衣 bmodel 分类输出全 0**：`yolov11s_int8_1b_back_reflectivejacket_1.bmodel`（与 `text_1` 项目 `yolov11s_int8_1b_back.bmodel` 同 md5）在 0.5.1+Python 和 0.5.3+C++ 下都是 8400 个 anchor 的分类列全 0；同结构 FP16 正常。详见 `docs/KNOWN-ISSUES.md` KI-001。
  - 官方 demo 的 `cv::putText` 不能画中文，验证时用英文类别名；生产服务需换 FreeType/BM CV 中文绘制。
  - `/proc/jpuinfo: Permission denied` 警告不影响解码（`/dev/jpu` 能打开）；后续 Docker 里要确认设备权限。
  - 官方 demo 只验证了单帧图片，没有验证视频流、36 路并发、合流、告警留存。
  - 服务端 C++ 代码、接口、Docker 均未开始。
- 下轮建议：先确认 INT8 模型是重新导出/量化还是暂用 FP16；然后盘点旧 `inference/` 源码的接口与任务线程，产出"接口清单"和"任务清单"两份 md，再进入服务端技术方案设计。

### 迭代 0 — 2026-11-02 — 初始化项目记忆骨架

- 目标：用 project-memory 建立 README + docs 六件套，使后续 AI 可冷启动接手
- 改动：新增 `AGENTS.md`、`README.md`、`docs/{REQUIREMENTS,ARCHITECTURE,CONVENTIONS,DECISIONS,PROGRESS}.md`
- 涉及：上述 8 个文档文件
- 验证：`pwsh -File <skill>/scripts/check-project-memory.ps1 -Target . -DocsRoot docs` → 通过，退出码 0
- 遗留：README 的定位/技术栈、REQUIREMENTS 的 FR/AC、CONVENTIONS 的命令表仍需按实际项目填写
- 下轮建议：先补 `CONVENTIONS` 的命令表（能跑通才算项目真的存在），再补 `REQUIREMENTS`
### 迭代 99 佐证

- `docs/assets/routeb_merge_all1.jpg`：路线 B 运行中 WS 抓取的 `all_1` 合流墙（1920×1080）。
- `docs/assets/routeb_alarm_14k.jpg`：路线 B 生成的告警图（淮河航道 14K，含红框/中文标签/绿色 ROI）。
- `tools/routeb_poc/README.md` 的 PoC 数据与 `docs/assets/routeb_poc_frame.jpg` 为硬解链路证据。

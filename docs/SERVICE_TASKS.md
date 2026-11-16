<!-- pm:meta
iteration: 5
updated: 2026-11-02
commit: -
-->

# 服务任务清单（旧实现盘点 + 新实现任务分解）

> 依据：`/data/CT/project/ai/inference` 旧源码；线上入口已确认是 **`app.py` + `main.py` + `ai_package/`**（按 `app.py` 的 import 链）。
> `task_manager.py`、`package/`、`run_all.sh` 是旧代码，本文在第 7 节单列，不再作为重构依据。

## 1. 进程/线程清单（旧实现）

| 组件 | 形态 | 职责 | 队列/通信 | 生命周期 |
|---|---|---|---|---|
| `app.py` | 主进程（FastAPI/uvicorn） | HTTP 9000、WS `/ws/frame`、任务 start/stop/list、occupancy、area frame | `running_tasks`、`frame_dict`、`ws_clients` | 常驻 |
| `start_agg_processes` | 4 个 `mp.Process` | 每组合流 9 路 → 3×3 大图 | 每进程一个 `mp.Queue(maxsize=2)` | 随 app 启动 |
| `get_img` | 4 个线程（app 内） | 从聚合队列取图，写 `frame_dict` | `agg_queues[i]` | 随 app |
| `check_task_loop` | 1 个线程 | 任务/整机健康检查与重启 | 无 | 随 app |
| `get_show` | 每个 WS 客户端 1 个线程 | 轮询 `frame_dict`，把图送 asyncio 队列 | `frame_queue` | 随 WS 连接 |
| `sender_loop` | 1 个 asyncio task | 广播 binary WS 包 | `asyncio.Queue(maxsize=1)` | 随 app |
| `main.py` | 每任务 1 个进程 | 任务总入口 | 进程内线程 | 任务 start→stop |
| `Analyzer_Video` | 每任务 1 个线程 `get_frames` + 1 个 `video_gc_worker` | 解析 URL、开 decoder、读帧、预处理、发帧、告警视频 dump | `FrameBus`、`show_queue(maxsize=1)`、`report_dump_queue(maxsize=5)` | 随任务 |
| `Show` | 每任务 1 个线程 | 画框、JPEG 编码、ZMQ 发给聚合端 | ZMQ PUSH | 随任务 |
| `ModelManager.init_model` / `end_model` | 每任务 2 个线程 | 按配置加载/卸载模型 | `models` dict + lock | 随任务 |
| `YOLO11` / `DamoYOLO` 的 `infer` | 每模型 1 个线程 | 取帧、推理、后处理、ROI 过滤、告警窗口 | `frame_queue(maxsize=1)`、`report_queue(maxsize=10)` | 随模型 |
| `YOLO11` / `DamoYOLO` 的 `report` | 每模型 1 个线程 | 画告警图、上报 HTTP、登记视频 dump | `report_queue`、`report_dump_queue` | 随模型 |
| `VideoAggregator.receiver` | 每聚合进程 9 个 `receiver_worker` 线程 | 每个端口收 ZMQ 帧 | `decode_queue(maxsize=1)` | 随聚合进程 |
| `VideoAggregator.decode_worker` | 每聚合进程 9 个线程 | JPEG 解码 + resize 640×360 | `decode_queue` | 随聚合进程 |
| `VideoAggregator.display` | 每聚合进程 1 个线程 | 3×3 拼接、JPEG 编码、`all_i` 命名 | `task_img_queue` | 随聚合进程 |
| `TaskParams.params_update` | 每任务 1 个线程 | 每 5 秒拉任务配置 | `buffer_lock` | 随任务 |

**数量级**：36 任务时约 36 个 Python 进程；每进程至少 6 个线程（视频、GC、显示、模型 init/end、模型 infer/report），即 200+ 线程；另有 4 个聚合进程 ×（9 receiver + 9 decode + 1 display）≈ 76 线程。这是旧方案资源占满的直接原因之一。

## 2. 单任务完整流程

1. `app.py:/task/start` 分配聚合端口（9011~9046），`subprocess.Popen(["python3","main.py","--task=...","--socket_agg_port=..."])`。
2. `main.py` 创建 `TaskParams`，构造时 `GET TaskAlgoConfigDetail`，并启动 `params_update` 每 5 秒轮询。
3. `TaskParams` 把后端字段映射为 `url / model_data / frame_interval_detect / detect_threshold / detect_section / video_duration / cooldown_time`；`taskArea` 归一化坐标按 `video_size` 乘宽高。
4. `Analyzer_Video.get_decoder()`：
   - `rtsp://` 直接用；`isc://` 调 Artemis `previewURLs` 换 RTSP（HMAC-SHA256 签名）。
   - `sail.Decoder(rtsp_url, True, 0)`；`compute_fps` 读 200 帧算平均 FPS；`enable_dump(12)` 预热。
5. `get_frames()` 循环 `decoder.read`：每帧都读；只有 `frame_id==0 || frame_id%5==0` 才：
   - 必要时 resize 到 1920×1080；
   - `preprocess_bmcv`：`crop_and_resize_padding` 到 640×640 RGB + `convert_to`；
   - `bus_frame.publish((frame_id, pre_frame, frame, ratio, txy))`；
   - `show_queue.put_nowait((frame_id, frame))`。
6. `ModelManager.init_model()` 每 5 秒看配置；新模型名则创建 `DamoYOLO`（或 `YOLO11`）实例，`FrameBus.subscribe(maxsize=1)`，启动 `infer` + `report` 两条线程。
7. `infer()` 取帧；`frame_id%5==0` 且（历史窗口有目标或 `frame_id%50==0`）才推理；后处理 → ROI 过滤 → `report_num = count(class==target_id)`；`detection_window` 满 10 帧后，`sum>=6` 且冷却通过且当前帧有目标 → `report_queue.put(...)`。
8. `report()` 取告警：
   - 生成 `save/<task>/<model>/img|video/YYYYmmdd_HHMMSS.*`；
   - `report_dump_queue.put((ori_video_dir, video_dir))`；视频线程 `decoder.dump(5,7,ori_video_dir)`，ffmpeg `-c copy` 转存；
   - `draw_bmcv_chinese` 只画 target 类别的红框 + 中文标签；`bmcv.imwrite`；
   - `POST ReportAlarmEvent`，`create_jwt("admin")`，timeout 5s。
9. `Show.send_data()` 从 `show_queue` 取帧，画所有模型的框（target 红、其它绿）和 ROI 蓝线，`bmcv.imencode(".jpg")`，ZMQ PUSH 到 `tcp://0.0.0.0:<socket_agg_port>`；每次 sleep 到 0.19s。
10. 聚合进程按端口收帧，JPEG 解码 → resize 640×360 → 3×3 → JPEG 编码 → `task_img_queue`；`app.get_img` 写 `frame_dict[task_name]`；WS `get_show` 每 0.198s 取最新图，`sender_loop` 广播。
11. `app.check_task_loop` 每 30 秒检查每任务 RSS/CPU；连续 3 次超限重启；整机 CPU/MEM >90% 连续 3 次（每 60 秒）则杀掉所有任务 + 聚合进程再重启。

## 3. 配置与触发语义（旧实现 vs 需求）

| 配置 | 后端字段 | 旧实现用法 | 需求语义 | 结论 |
|---|---|---|---|---|
| 推理间隔 | `frameInterval` | **未使用**，硬编码每 5 帧 | 每 N 帧推理 | 必须修复 |
| 告警窗口 | `detectSection` | **未使用**，硬编码 `deque(maxlen=10)` | 最近 N 帧 | 必须修复 |
| 告警帧数 | `detectThreshold` | **未使用**，硬编码 `sum>=6` | 窗口内 M 帧 | 必须修复 |
| 冷却 | `cooldownTime` | 后端分钟 `*60`，再乘 FPS 得帧数 | 秒 | 单位要确认 |
| 视频时长 | `videoDuration` | 仅用于 buffer 长度（buffer 又基本没用） | 告警前后 10 秒 | 必须修复 |
| 区域 | `taskArea[].points` | 归一化坐标乘 `video_size` | 多边形 ROI | 要处理分辨率未就绪 |
| 置信度 | `algoConfig.conf_thresh` | 有使用 | 有 | 保持 |
| NMS | `algoConfig.nms_thresh` | 有使用 | 有 | 保持 |
| 人数阈值 | `algoConfig.exceed_num` | 有使用 | 有 | 保持 |

## 4. 模型任务

| 模型 | 旧类 | 旧权重 | 新权重（本轮） | 输入归一化 | 备注 |
|---|---|---|---|---|---|
| `person` | `DamoYOLO` | Damo FP16 | 同左 | 0-255 | 人员聚集 |
| `cigarette` | `DamoYOLO` | Damo FP16 | 同左 | 0-255 | 抽烟 |
| `lifejacket` | `DamoYOLO` | Damo FP16 | 同左 | 0-255 | 未穿救生衣 |
| `safetyhelmet` | `DamoYOLO` | Damo FP16 | 同左 | 0-255 | 未戴安全帽 |
| `reflectivejacket` | `YOLO11` | YOLOv11 INT8 | **YOLOv11 FP16** `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel` | **/255** | 本轮实测正常 |
| `reflectivejacket_sky` | `DamoYOLO` | Damo FP16 | 同左 | 0-255 | 天空/远景反光衣 |

旧 `Model_Manager` 的 `if model_data["model_name"] is not "reflectivejacket":` 用 `is not` 比较字符串，是隐患；新实现用模型注册表 + `enum/type`，不靠字符串身份。

## 5. 旧实现的明确缺陷/风险

| 编号 | 现象 | 根因 | 影响 |
|---|---|---|---|
| OLD-01 | 36 路时进程/线程爆炸、内存泄漏 | 一任务一进程；每模型两线程；每任务一套 decoder/BMImage；Python 对象长期持有 | 掉帧、OOM、几天宕机 |
| OLD-02 | 模型随任务加载，不能共享 | `ModelManager` 是任务内的；模型字典只在本进程 | 同模型 36 份，显存/内存浪费 |
| OLD-03 | 配置的 `frameInterval/detectSection/detectThreshold` 不生效 | 代码硬编码 5/10/6 | 与前端配置不一致 |
| OLD-04 | 输出约 5fps，不是 10fps | `Show` sleep 0.19s；`get_show` sleep 0.198s | 预览卡顿 |
| OLD-05 | 合流路数/位置不稳定 | 按 `frames` 字典插入顺序拼图，无固定槽位 | 通道跳位 |
| OLD-06 | 合流 CPU 开销大 | 每路先 JPEG 编码，聚合再 JPEG 解码、resize、再编码 | CPU 被编解码吃掉 |
| OLD-07 | INT8 反光衣无输出 | 这批 INT8 分类列为 0（KI-001） | 已决策改用 FP16 |
| OLD-08 | FP16 归一化错误 | 旧 `ab=[1,1,...]` 按 0-255；新 FP16 需 /255 | 误检；必须按模型配置归一化 |
| OLD-09 | 告警视频 5+7 秒，不是 5+5 | `decoder.dump(5,7,...)` 硬编码 | 与需求不一致 |
| OLD-10 | 告警上报路径依赖 cwd | 旧实现写 `save/...`，靠工作目录 `/data/CT/project/ai/inference` 变成实际路径 | 新实现固定绝对路径 `/data/CT/project/ai/inference/save/...` |
| OLD-11 | 配置接口无限重试 | `while` 循环无总超时 | 后端故障时任务卡死 |
| OLD-12 | `DamoYOLO` 解包 5 元组为 3 元组 | `frame_queue` 发布 5 个元素 | 可能直接 ValueError |
| OLD-13 | 任务/整机资源超限就重启 | `check_task_loop` 的 240MB/80%/90% 阈值 | 掩盖泄漏、频繁重启 |
| OLD-14 | 无固定任务→合流组映射 | 端口顺序分配 | 4 组合流不均/不固定 |
| OLD-15 | 重复遗留代码 | `task_manager.py`/`server.py`/`socket_server` 8001 | 维护混乱 |
| OLD-16 | 中文绘制依赖 PIL/字体 | `draw_bmcv_chinese` | CPU 拷贝/字体路径依赖 |

## 6. 新 C++ 服务任务分解

> 目标是「一个服务进程 + 线程池 + 共享模型调度」，不是把 Python 进程逐个翻译。

| 编号 | 任务 | 输入/输出 | 验收 |
|---|---|---|---|
| T-01 | HTTP/WS 控制面 | JSON over HTTP、binary WS | 接口契约测试通过；token 过期/错误码正确 |
| T-02 | 认证/JWT | HS256、7 天、iss/aud | 只验证入站；出站用 `create_jwt("admin")`；有效/过期/错签/错 iss/aud 测试 |
| T-03 | 配置同步 | 后端 `TaskAlgoConfigDetail` | 字段映射正确；超时/退避；配置 diff |
| T-04 | 任务生命周期 | start/stop/list | 状态机；重复操作幂等性符合约定；资源释放 |
| T-05 | 解码 | RTSP/ISC → BMImage | 4 组合流组；断流重连；FPS 测量 |
| T-06 | 帧总线/背压 | 每任务最新帧 + 环形缓冲 | 慢消费者丢帧不阻塞生产；无泄漏 |
| T-07 | 模型注册表 | 6 模型路径/类别/归一化/target | FP16 反光衣正常；Damo 路径正确 |
| T-08 | 模型调度器 | 任务请求 → 模型实例 | 同模型多任务共享；过载扩实例；空闲回收 |
| T-09 | 预处理/后处理 | BMImage → tensor → detections | letterbox/归一化/ROI/NMS 与 Python 对齐 |
| T-10 | 告警状态机 | detect_section/threshold/cooldown | 配置生效；误报/漏报测试 |
| T-11 | 告警证据 | 带框图 + 5+5 秒视频 | 图片含中文框；视频无框；路径正确 |
| T-12 | 告警上报 | `ReportAlarmEvent` | 重试/超时/待上报队列 |
| T-13 | 实时预览 | 每任务 10fps JPEG → WS | 10fps 上限；慢客户端丢帧 |
| T-14 | 合流 | 4 组 ×9 → 3×3 1080p | 固定槽位；`all_1..all_4` |
| T-15 | 资源指标 | occupancy | 字段与旧接口一致（`vpu_1..3` + `vpp_1..2`，无 `vpp_3`） |
| T-16 | 存储/GC | save 目录、磁盘配额 | 不无限增长；删除策略 |
| T-17 | Docker | 镜像 + 设备/卷/网络 | 容器内接口与盒子直跑一致 |
| T-18 | 测试/压测 | 单元/集成/36 路 | 30 分钟稳定；内存增长 <5% |

## 6.1 实现状态（迭代 5 MVP+）

| 任务 | 状态 | 说明 |
|---|---|---|
| T-01 | 部分 | HTTP/WS 已通；线程/连接同步 Beast |
| T-02 | 已完成 | HS256/iss/aud/exp 验证；缺 token/错签 401；出站 `create_jwt("admin")` |
| T-03 | 已完成 | 真实后端 `TaskAlgoConfigDetail` 拉取；无配置 diff/退避 |
| T-04 | 已完成 | start/stop/list 已通；5 类模型；ISC 解析 |
| T-05 | 部分 | RTSP/本地/ISC；每任务一个 Decoder；无断流重连 |
| T-06 | 部分 | 预览按间隔取最新；发送队列最多 2 帧丢旧 |
| T-07 | 已完成 | 6 个模型注册（反光衣 + Damo 5 类） |
| T-08 | 已完成 | 多实例按 busy 扩容 + janitor 空闲回收（默认 120s；测试 2→1） |
| T-09 | 已完成 | 反光衣 `/255` + Damo 0-255 两套后处理；NMS/ROI 对齐 |
| T-10 | 部分 | `detect_section/threshold/cooldown` 生效；误报/漏报未系统测试 |
| T-11 | 已完成 | 告警图 + RTSP 5+5 视频；mpeg4 不支持 |
| T-12 | 部分 | POST 已通；无重试/待上报队列 |
| T-13 | 部分 | 10fps 可配；36 路压测用 2fps；无慢客户端压测 |
| T-14 | 已完成 | 4 组×9 槽位；36 路下 4 路合流均输出 |
| T-15 | 已完成 | 字段一致；异常值钳 0 |
| T-16 | 未开始 | 无 GC/配额 |
| T-17 | 已完成 | 镜像构建/容器内接口/Compose 验证 |
| T-18 | 部分 | 36 路 640×360/2fps 30s；1080p/10fps + 30 分钟未做 |

## 7. 早期/备用实现（已确认不用于线上）

- 线上入口已确认是 `app.py` + `main.py` + `ai_package/`（`app.py` 的 import 链）；下面这些不再作为重构依据，只看它们的硬件用法。
- `task_manager.py`：9 端口（9011~9019）、无 WS、启动 `main.py`；入口 `run_all.sh` 曾指向它。与 `app.py` 的 36 端口/4 合流不一致。
- `package/`：`VideoManager` 用 `sail.MultiDecoder(16)` + `sail.Encoder` 持续录像，`ModelManager` 有模型共享/容量监控的雏形，但 `init_model` 的字典初始化不完整、`TaskManager` 与当前 `ai_package` 不兼容。可作为“共享模型 + 硬件编码”的参考。
- `ai_package/server.py`：独立 WS 9000 + TCP 8001，和 `app.py` 功能重叠；线上不是它。
- `ai_package/model/base_model.py`：按模型拆类的重构版，但 `Model_Manager.py` 仍 import 旧 `Yolo`/`Damo_Yolo`，属于半成品。

## 8. 性能基线与容量假设

- YOLOv11 FP16：preprocess 1.668 ms / inference **5.214 ms** / postprocess 0.245 ms（3 张图平均）。
- 1080p 单路 `sail.Decoder`：短片段 49 帧 / 0.203 s ≈ **241 fps**（burst 指标，非 30 分钟稳态）。
- 36 路 × 1 fps 常态：36 × 5.214 ms ≈ **0.19 s/s** TPU 时间，余量充足。
- 36 路 × 25 fps 全速：900 × 5.214 ms ≈ **4.7 s/s**，单 TPU 不可能；必须限制 burst 路数（建议 ≤4 路全速，约 0.52 s/s）。
- 4 组合流组 × 9 路：与 4 个解码 worker / 4 个聚合 worker 对齐；每路输出 ≤10 fps。
- JPEG 编码/合流/WS 带宽需在 M4 实测；旧方案每路 JPEG→聚合再解码，是已知 CPU 热点，新方案应尽量在 device 内存里 resize/拼图，只编码一次。

## 9. 验证计划

1. 单元：JWT、配置映射、告警状态机、模型注册表、ROI。
2. 集成：mock 后端 + 本地视频文件，跑 start/stop/area/WS/occupancy。
3. 模型：新 FP16 权重 `probe_model.py` 两种归一化对比；官方 demo 14 框通过。
4. 合流：4 组 ×9 黑帧/实帧，检查固定槽位和 `all_i` 命名。
5. 压测：36 路本地视频循环，30 分钟，记录 RSS/TPU/VPU/掉帧/WS 延迟。
6. Docker：容器内重跑 1–5。

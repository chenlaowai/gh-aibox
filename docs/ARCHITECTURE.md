<!-- pm:meta
iteration: 45
updated: 2026-11-12
commit: -
-->

# 架构与代码地图

> 只写**现存**结构。当前已有一个可运行的 C++ MVP；完整目标架构见 `docs/SOLUTION.md`，实现偏差见本文末尾。

## 目录职责

```
gh-aibox-dev/
  CMakeLists.txt                # C++17 服务构建（Boost/OpenSSL/sail）
  src/
    main.cpp                    # 入口：加载模型、启动 HTTP/WS、合流、任务管理
    common.hpp                  # AppConfig/TaskConfig、JWT、HTTP client、metrics、文件工具
    model.hpp                   # ModelSpec 注册表 + ModelInstance + 多实例 ModelScheduler
    isc.hpp                     # ISC 签名/HTTPS 解析
    task.hpp                    # Task：RTSP/ISC/本地解码、多模型推理、告警图/5+5 视频、预览
    server.hpp                  # StreamHub/WsSession、4 组合流、TaskManager、HttpServer、occupancy
  docker/
    Dockerfile                  # 基于 aibox-api:latest + env/sdk + OpenSSL1.1
    docker-compose.yml          # privileged/host network/模型与 save 挂载
    lib/                        # libcrypto/libssl 1.1（Debian12 基础镜像缺）
  env/                          # C++ 运行/编译环境（脚本 + 盒子上的生成物）
    env.sh                      # source 后设置 SDK 的 LD_LIBRARY_PATH / PATH / SAIL_DIR
    setup_sdk.sh                # 解压 libsophon 0.5.3 + mw 0.15.0，编译安装 sail 3.11.0
    test_sail.cpp               # C++ sail 冒烟测试：加载 bmodel 并打印输入/输出形状
    test_sail.sh                # 编译并运行 test_sail
    packages/                   # 厂商 tar.gz（盒子生成物，不提交）
    src/                        # 解压出的 sail 源码（盒子生成物，不提交）
    build/sail/                 # sail 构建目录（盒子生成物，不提交）
    sdk/                        # 自包含 SDK 安装目录（盒子生成物，不提交）
  tests/
    yolov11_official_demo/      # 步骤 2：官方 YOLOv11 C++ demo 的验证工程
      cpp/yolov8_bmcv/          # 官方 sophon-demo 源码（YOLOv8_plus_det）
      data/images/              # 3 张反光衣测试图（来自盒子数据集）
      data/reflectivejacket.names     # 中文类别名（生产用）
      data/reflectivejacket_en.names  # 英文类别名（官方 demo 的 cv::putText 只能显示 ASCII）
      models/                   # FP16 测试模型副本（盒子生成物，不提交）
      bin/                      # 编译产物（盒子生成物，不提交）
      results/                  # 检测图 + JSON（盒子生成物，不提交）
      build.sh / run.sh         # 编译 / 运行
      verify.sh                 # 编译 + 运行 + 断言检测数 > 0
      check_results.py          # 解析 results/*.json 并打印每类数量
  docs/                         # 项目记忆与设计文档（本目录）
    INTERFACES.md               # 接口清单
    SERVICE_TASKS.md            # 旧服务任务/线程盘点 + 新任务分解
    SOLUTION.md                 # 目标整体构建方案
    assets/                     # 验证截图/日志/JSON 证据
```

## SDK 目录布局（盒子）

```
env/sdk/
  libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3/
    include/  lib/  bin/        # 头文件、.so、bm-smi/bmrt_test
  sophon-mw-soc_0.15.0_aarch64/opt/sophon/
    sophon-ffmpeg_0.15.0/       # include/ lib/
    sophon-opencv_0.15.0/       # include/opencv4 lib/
    sophon-sample_0.15.0/       # 官方 C++ sample 源码
  sophon-sail/
    include/sail/               # sail C++ 头文件
    lib/libsail.so              # 已显式链接 libbmcv.so.0
    lib/cmake/SAILConfig.cmake  # 安装后修正 find_library 路径，供 find_package(SAIL) 使用
```

## 模块边界与依赖方向

- `tests/yolov11_official_demo/*.sh` 依赖 `env/env.sh` 与 `env/sdk/`；反向不成立，`env/` 不得引用 `tests/`。
- 官方 demo 源码（`cpp/yolov8_bmcv/`）保持上游原样；构建差异放在 `build.sh` 的编译参数里，不改源码。
- 测试脚本统一用 `TEST_DIR/../..` 定位仓库根，不写死当前工作目录。
- `docs/` 里的 `ARCHITECTURE`/`CONVENTIONS` 描述现状；`INTERFACES`/`SERVICE_TASKS`/`SOLUTION` 是**目标设计/契约**，实现落地情况见本文"当前 C++ MVP"。

## 当前 C++ MVP+（迭代 6）

### 模块

- `main.cpp`：启动后 2s 首轮 `reconcile_from_backend`，随后常驻线程每 2s 同步兜底；前端保存成功后直接 POST 9000 `/task/reload_config`（CORS+JWT），`TaskManager` 拉新配置后调用 Task 热更新（推理线程帧间重建 models/ROI，不释放槽位）；仅源地址/任务名变化才 stop/start。每轮还调用 `supervise_stalled_tasks`：曾出帧、2 分钟无帧的任务按 30s 间隔逐个 stop/start（10 分钟退避），用于打破 `Decoder::read()` 永久阻塞；从未出帧的任务不重启，避免 ISC 会话被抢。
- `common.hpp`：`AppConfig`/`TaskConfig` 解析、JWT HS256、HTTP client、`/proc`/`sail` metrics、文件工具。
- `model.hpp`：`ModelSpec` 注册表（reflectivejacket/reflectivejacket_sky/safetyhelmet/lifejacket/cigarette/person）；`ModelInstance` 支持多输出；`ModelScheduler` 按 busy 选实例、按 `AIDETECT_MAX_INSTANCES` 扩容。**推理准入是 FIFO 票据闸门**：`enter_queue()` 按到达顺序放行、最多 `AIDETECT_INFER_CONCURRENCY`（默认 2）个并发，取代原全局 `infer_mu_`（ADR-079）；每模型实例池 `max(1, 并发/2)`，避免同模型任务挤在一个 engine 锁上。`AIDETECT_PROFILE`（或容器内 `/tmp/aidetect_profile`）打开每模型 `n/avg/wait_avg/max/peak_pending` 统计，用来区分“等锁”和“TPU 计算”。
- `isc.hpp`：`isc://` 解析（protocol/transcode/videotype/https/streamType）、HMAC-SHA256 签名、HTTPS POST、`data.url` 提取；按生产参数请求 Artemis，断流重连重新 resolve；206.68 到 ISC 的网络仍被 ACL 拒绝，当前用 `.25` 的 443/554 转发。
- `task.hpp`：一个任务一个线程；`sail::Decoder` 读 RTSP/本地文件（ISC 先解析）；多模型循环；YOLOv11/DamoYOLO 两套后处理/NMS/ROI；**按需预览**（`preview_target` 1080p 只缩不放、`preview_due` 令牌桶真 10fps、`render_preview_fast` VPP+矩形+中文 plate+JPU、`render_tile_fast` 合流瓦片）；预览编码在独立 `preview_thread_`，解码线程只 PUSH 最新帧（worker busy 时丢帧），避免 9 路串行 JPEG 阻塞解码造成合流慢动作；推理队列改为 `InferJob{img,frame_ts}` 且只保留最新帧，`dets_ms_=frame_ts`；预览用“帧时间 + 轨迹历史”绑定：`PreviewTrack` 保存 `(ts,box)`，新检测按 `dets_ts` 找轨迹当时的姿态并只修正增量，帧间用 LK（960 宽、VPU 预缩放）+ 内框模板匹配推进，属性框漏检时用全帧人员框重锚，框中心离开 ROI 当帧删除（见 `docs/PREVIEW_PIPELINE.md` 第 13 节）；`publish_status` 1Hz 心跳重连画面（时钟/已断开秒数），并在 tile 右上绘 `taskName` OSD（重连状态也绘、no_task 不绘）；`taskArea` 解析为 `roi_polygons_`，单通道/合流/告警图绘绿色区域线；检测结果在 `update_alarm` 入口和 `dets_` 写回前按框中心过滤 ROI（算法 region 为空回退任务级 ROI），区域外不画框/不告警；`/task/get_area_frame` 走无框原图 `area_jpeg_`；每模型独立告警窗口（`report_gap=max(task_gap, algo_gap)` + `next_report_ms` 硬门控）；**单模式用单媒体源网关**（一个 ffmpeg 只拉一条上游，`-c copy` 出两路本地 MPEG-TS/UDP：解码一路、时间戳包环一路），告警取前后 5s；multi 模式用包环/worker 回退；高分辨率任务走 `HighResGate`。
- `server.hpp`：线程/连接 Beast HTTP + WebSocket；`StreamHub` 按 taskId 广播并统计订阅需求（`note_subscribe/note_unsubscribe/wanted`）；`Aggregator` 4 组×9 槽位，仅在有订阅者时用 `bmcv.imdecode`/`image_copy_to`/`imencode` 硬件链路拼图+编码，并按内容更新发送；Task/Aggregator 的 JPEG/JPU 调用用全局 `jpeg_hw_mutex()` 串行化；`TaskManager` 拉后端配置、启停任务、**按 group0.slot0→…→group3.slot8 首空分配**（停止只清自己的槽，新任务填回空槽，不搬已有画面），启动失败（无槽位/拉配置失败）返回 **HTTP 409** 让 .NET 后端不把任务置为 Running；注入预览路由；`get_occupancy`。
- `docker/`：基于 `aibox-api:latest`（Debian 12）的离线镜像；拷贝 SDK/OpenSSL 1.1；补 `/opt/sophon/libsophon-current` 软链；`--privileged --network host` 运行；Compose v2.29.7 已安装。

### 当前服务数据流

1. `POST /task/start` → `TaskManager::fetch_config`（后端 `TaskAlgoConfigDetail`，本地可用 `AIDETECT_CONFIG_FILE`）→ 分配 `(group,slot)` → 创建 `Task` 线程。
2. `Task` 若是 `isc://` 先 `IscResolver::resolve`，再打开 `sail::Decoder`；每帧读入；每 `frame_interval` 帧按任务配置的多模型依次推理。
3. 预览按需：`StreamHub::wanted(taskId)`/`wanted("all_N")` 决定路径——单通道（1080p 上限）/合流瓦片（640×360）/无人观看时 1fps area 图；令牌桶保证输出 ≤10fps 且为真实解码帧。
4. 每个模型独立 ROI/告警窗口；告警保存 `save/<task>/<model>/img`；单模式从本地网关的时间戳包环取 [触发-5s, 触发+5s]，直接 rename 成 MP4；multi 模式回退包环/worker；最后 POST `ReportAlarmEvent`。
5. 预览 JPEG → `StreamHub`（taskId）与 `Aggregator`（group/slot）；聚合线程只在有订阅者且瓦片更新时拼 3×3，硬件编码为 `all_<g+1>`。
6. `/task/get_area_frame` 返回 `Task::latest_jpeg()` base64；`/task/list` 返回 `taskName`/`channelAddress`/`resolution`/`health`/`lastError`，前端通道页按地址、任务页按 taskId 显示真实视频状态。无运行任务的通道走 `POST /channel/probe`：ISC resolve 后 `timeout 6 ffmpeg -c copy -f null -` 只读流头，解析 `WxH`，60s 缓存、2 并发、不占 BPU；不引入 39 路常驻 MediaChannel。

### 当前实现与目标方案的偏差

- 4 个 `MultiDecoder` 组已实现（`src/decode_pool.hpp`），`AIDETECT_DECODE_MODE=multi` 时启用；但大量 ISC 源失败时组内解码线程会被拖死，当前生产容器改用 `single` 部署（见 KI-020）。
- 模型调度已多实例扩容 + 空闲回收（janitor，默认 120s；测试 5s 从 2 实例回收为 1）。
- 告警视频单模式走"网关 + 时间戳包环"（H.264/H.265，源分辨率/帧率不变，实测图片位于视频中间）；multi 模式走包环/worker。`sail::Decoder::dump` 在 30 路 1440p 下会崩，仅作回退。
- HTTP/WS 是线程/连接，不是文档里的 4 线程 async Beast。
- 合流硬件链路已验证 1920×1080/9~10fps；30 路部署受可用源数量限制（当前 6 路可达）。
- 真实 ISC 网络：206.68 到 10.10.69.250 的 443/554/1935 均被 RST，只能经 206.25 的 443/554 转发；生产 URL 请求 rtmp，当前显式用 rtsp。
- 预览 JPEG 质量暂用 SAIL 默认，尚无质量档位。

## 关键数据流（步骤 2 验证）

1. `env.sh` 把 `env/sdk/*/lib` 放到 `LD_LIBRARY_PATH` 最前，覆盖系统旧运行时。
2. `build.sh` 用 `g++` 编译官方 `main.cpp` + `yolov8_det.cpp`，链接 `bmcv/bmrt/bmlib/yuv/opencv/ffmpeg`，写入 SDK rpath。
3. `run.sh` 调 `bin/yolov11s_reflectivejacket_demo --bmodel=... --input=data/images`。
4. demo 内：`cv::imread` → `cv::bmcv::toBMI` → `Yolov8_det::Detect`（letterbox + `bmcv_image_convert_to` + `bmrt_launch_tensor` + NMS）→ 画框 → `results/images/*.jpg` + `results/*.json`。
5. `check_results.py` 读 JSON，断言 `detections > 0`，打印 `images / detections / per_class`。

## 外部依赖与接口契约

| 依赖 | 用途 | 契约要点 |
|---|---|---|
| libsophon 0.5.3 | BMRT/BMCV/BMLIB、设备管理 | `bmrt` 版本 1.0.0；SoC 模式；`/dev/bmdev-ctl` 等设备节点 |
| sophon-mw 0.15.0 | 硬件加速 OpenCV/FFmpeg | `libopencv_*.so.4.1`、`libav*.so.58`；编译需要头文件 |
| sophon-sail 3.11.0 | C++ 推理/多媒体封装 | 依赖 libsophon + mw；本次用 `BUILD_TYPE=pcie` 原生编译 |
| YOLOv11 FP16 bmodel | 步骤 2 验证模型 / 生产候选 | `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel`，24.7 MB，md5 `13d8c91f...`；输入 `1×3×640×640` FP32；输出 `1×6×8400`；2 类，0=穿反光衣，1=未穿反光衣；输入 `/255` |
| YOLOv11 INT8 bmodel | 暂缓（KI-001） | 输入同上；输出 `1×8400×6`；分类列全 0，本轮不用 |

环境变量（`env/env.sh` 导出）：

- `SOPHON_SDK_ROOT`：`env/sdk`
- `LIBSOPHON_HOME` / `FFMPEG_HOME` / `OPENCV_HOME` / `SAIL_HOME`
- `LD_LIBRARY_PATH`：SDK 四个 lib 目录前置
- `PATH`：`$LIBSOPHON_HOME/bin` 前置
- `SAIL_DIR`：`$SAIL_HOME/lib/cmake`

服务端口（已实现）：AI 服务 HTTP/WS 都在 `9000`，合流 id `all_1..all_4`（见 `INTERFACES.md` 3.1）。

## 禁区（不要动）

- `/opt/sophon`：盒子原厂运行时，旧 Python 服务依赖它；新 SDK 只放 `env/sdk/`。
- `env/sdk/`、`env/build/`、`env/src/`：`setup_sdk.sh` 生成，手改会被下次构建覆盖。
- 官方 demo 源码：只在 `build.sh`/`run.sh` 做外部适配；需要改逻辑时先记 ADR 并说明与上游的差异。
- 模型文件、告警图片/视频、SDK tar.gz：体积大且敏感，不提交仓库。
- 凭证：盒子 SSH 密码、Web 账号、GitHub token、JWT secret 不得写入仓库或日志。

## 变更记录

- 2026-11-13（迭代 45）：检测裁剪改用缩放帧（`det_src` + `crop_norm`），无人可裁时
  不缩放；性能对比统一在任务上量完成后测（见 ADR-084）。
- 2026-11-13（迭代 44）：裁剪算法按各自间隔刷新（`algorithm_interval_ms`），
  预览每帧只缩放一次（1280×720）供跟踪/单通道/瓦片共用（对齐 requireScaled）。
- 2026-11-13（迭代 43）：行为检测框改回人形框（标签空行根因）；标签按开启的
  行为开关拼文案；底板改构建期预渲染（tools/gen_label_plates.cpp + labels.json）；
  单通道预览对齐生产 1280×720；推理并发默认 4，队列积压时按 load() 拉长检测间隔。
- 2026-11-13（迭代 42）：标签底板改为“先画后量”（putText + inRange/boundingRect），
  修末字被裁；33 任务实测确认为解码/VPP/JPU 容量上限，并发保持 2（ADR-081）。
- 2026-11-12（迭代 41）：每个算法标签用 FreeType 现渲染底板（`ensure_label_plate`），
  修 FreeType badge 左上角原点；告警目标 TTL 1s→2s（`AIDETECT_TARGET_TTL_MS`）；
  预览轨迹按 label 关联；待确认/确认同框只保留一种状态；合流瓦片校验 SOI/EOI。
- 2026-11-12（迭代 40）：推理准入改 FIFO 票据闸门 + 有界并发（ADR-079），删除全局
  `infer_mu_`；加每任务检测节奏/裁剪人数上限；预览把同一目标的多个算法轨迹聚类成
  一个框 + 竖排标签，漏检保留 600ms 人形锚点；合流瓦片独立令牌桶。

- 2026-11-12（迭代 39）：预览框改为按检测帧绑定（`PreviewTrack` 帧时间
  历史 + 检测增量修正），LK 失败加内框模板匹配；用全帧人员框重锚属性框；
  框离开 ROI 当帧删除，移除 1s `active` 边缘框缓存；跟踪前先 VPU 缩到 960。
- 2026-11-12（迭代 38）：预览线程对检测框做 LK 光流逐帧跟踪；确认目标
  保留 5s→1s，人出 ROI 后边缘残框快速消失。
- 2026-11-12（迭代 37）：预览框按 `dets_history_` 选择不晚于当前帧的
  检测快照并速度外推；新增 MP4 上传/本地循环播放，文件源跳过告警上报。
- 2026-11-12（迭代 36）：任务/分组状态显示 `Camera not found`；gateway
  UDP 接收缓冲 1MB→8MB + `overrun_nonfatal`，修复 4K 底部撕裂。
- 2026-11-11（迭代 35）：新增 `/channel/probe` 轻量取流探测（ffmpeg -c
  copy，无 BPU），未配置任务的通道也能显示状态/分辨率；决定暂不重构
  常驻 MediaChannel 层。
- 2026-11-11（迭代 34）：`/task/list` 增加 health/resolution/lastError；
  通道页“通道状态”和任务页状态改为 AI 实时健康，显示生产同款
  `正常 + 分辨率` / `Input/output error` / `Camera not found`。
- 2026-11-11（迭代 33）：`TaskManager` 增加 2min 无帧 supervisor（仅曾
  出帧任务、逐个 30s 间隔），`/task/list` 暴露 `lastFrameAgeMs`；
  ISC previewURL 缓存 30min、失败回退旧 URL、请求间隔 120ms。
- 2026-11-10（迭代 32）：crop 算法/告警/最终 dets 统一按框中心过滤 ROI，
  区域外不画框、不告警。
- 2026-11-10（迭代 31）：保存后前端直连 `/task/reload_config`（生产
  `handleRemoteMessage → onTaskArrived` 同类事件驱动），2s 轮询兜底；
  配置签名覆盖 ROI 与全部算法参数，按 `archiveUpdated → taskArchiveApply`
  原地热更新，不释放槽位。
- 2026-11-10（迭代 30）：检测框绑定推理帧时间 + 推理队列 latest-only；
  配置区域取无框原图；单/合流/告警图绘制 ROI 绿线；启动失败返回 409。
- 2026-11-10（迭代 28）：预览编码改 async latest-frame，修合流慢动作；
  status 1Hz 心跳 + 任务编号 OSD；合流槽位首空顺序分配、通道4（VideoRoute=36）；
  告警上报 `max(task, algo)` 硬门控。
- 2026-11-10（迭代 27）：标签恢复生产式垂直堆叠；删除目标中心距离
  兜底，第二目标重新走 5s 蓝框倒数。
- 2026-11-10（迭代 26）：通道重名前端自动改 `_2`；标签同行贴框且无
  置信度；预览 dets 保留 5s，消除推理空窗。
- 2026-11-09（迭代 25）：告警视频 ring 落盘失败回退 decoder dump；告警图
  标签固定贴框；`crowd_head` 改名“人员聚集（密度图）”；新增
  `docs/ALGORITHM_PARITY.md`。
- 2026-11-09（迭代 24）：接受 ISC 并发配额限制，保持 36 路同等取流；
  直连 RTSP 可绕过 ISC 会话，但 36 路 4K 仍受带宽/VPU 限制。
- 2026-11-09（迭代 23）：修复 `tb_task_algo.created` 时间格式导致的任务
  更新 500；pending 不画标签；上报间隔取任务/算法较大值；已确认标签常驻，
  多算法标签按 slot 错位；V3/vest/boat 三算法复验；取消 7 路优先，
  ISC previewURL 缓存、1.2s 错峰启动；ISC 硬配额约 11 路。
- 2026-11-09（迭代 22）：恢复 6488 vest，V3+vest 双算法并行；看门狗
  杀 gateway 后注入 UDP 包唤醒 decoder。
- 2026-11-09（迭代 21）：Task 增加 frame watchdog；V3 独立阈值；告警标签
  改板端绘制。
- 2026-11-09（迭代 20）：办公室演示任务去掉 vest，只保留 helmet_v3；
  合流通道黑格确认是 ISC 无 decoder。
- 2026-11-08（迭代 19）：behavior 改 YOLOv10 5 类 + people 框；救生衣确认
  minor152 走 major1 人形框；HEVC 告警视频实测转码落库。
- 2026-11-08（迭代 18）：V3 全量替换 helmet，按 people 顶部 70% 裁剪；
  behavior 补 5 类/分类阈值；HEVC 告警视频改软件解码 + h264_bm。
- 2026-11-08（迭代 17）：救生衣改人形裁剪推理；抽烟/打电话固定上半身裁剪、
  单候选人、分类别持续时长；V3 修正类别顺序并改 head 供框。
- 2026-11-08（迭代 16）：预览/告警渲染只画目标框；无 ROI 的
  `boat_life_jacket`/`people_gather` 从河道任务移除，待 ROI 后再启用。
- 2026-11-08（迭代 15）：网关改为 fork/exec 关闭继承 fd + 唯一 UDP 端口；告警媒体
  前端统一映射到 `/aibox/<save相对路径>`；HEVC 告警视频落盘后转 H.264；
  `tb_algo_config` 增加生产独有参数（类型/范围/选项存 `remark` JSON）。
- 2026-11-02（迭代 5）：增加 `isc.hpp`；模型调度器/多模型/Damo 后处理/告警 5+5 视频/硬件合流；更新 36 路与 Compose 现状。
- 2026-11-02（迭代 4）：新增 `src/` C++ MVP、`CMakeLists.txt`、`docker/`；记录当前模块/数据流/实现偏差。
- 2026-11-02（迭代 3）：按确认修正：线上入口 `app.py+main.py+ai_package`；WS 固定 9000/`all_1..4`；告警固定绝对路径；`vpp` 只有 1/2；`docs/` 目录名与需求一致。
- 2026-11-02（迭代 2）：新增 `docs/INTERFACES.md`、`SERVICE_TASKS.md`、`SOLUTION.md`；补充 FP16 新权重元数据与 WS 端口待确认项。
- 2026-11-02（迭代 1）：建立 `env/` 自包含 SDK 与 `tests/yolov11_official_demo/` 验证工程；项目记忆目录定为 `docs/`。

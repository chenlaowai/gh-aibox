<!-- pm:meta
iteration: 73
updated: 2026-11-21
commit: -
-->

# 已知问题与坑

> 两类都记：已知但暂不修的 bug，以及「看起来像 bug 其实是有意为之」的陷阱（后者更值钱）。

## 已知缺陷

### KI-001 INT8 反光衣 bmodel 分类输出全为 0，FP16 正常

- 现象：用盒子上的 `yolov11s_int8_1b_back_reflectivejacket_1.bmodel` 推理，输出 `class_max=0.000000 class_nonzero=0`，8400 个 anchor 的两个分类列全是 0，因此 demo 一个框都出不来。FP16 新权重 `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel` 在同一张图、同一套 letterbox 下 `class_max=0.913574`，62 个 anchor 超过 0.25。
- 复现（在盒子 `/data/CT/project/ai/gh-aibox-dev` 下）：
  ```bash
  python3 tests/yolov11_official_demo/probe_model.py \
    --model /data/CT/project/ai/models/BM1684X/yolov11s_int8_1b_back_reflectivejacket_1.bmodel \
    --image tests/yolov11_official_demo/data/images/reflective_jacket_1280x720.jpg --norm 1
  # 输出：output_shape=(8400, 6) class_max=0.000000 class_nonzero=0 anchors>conf=0

  python3 tests/yolov11_official_demo/probe_model.py \
    --model /data/CT/project/ai/models/BM1684X/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel \
    --image tests/yolov11_official_demo/data/images/reflective_jacket_1280x720.jpg --norm 255
  # 输出：output_shape=(8400, 6) class_max=0.913574 class_nonzero=814 anchors>conf=62
  ```
- 触发条件：所有测试到的 INT8 YOLO bmodel 都有此现象，包括 `text_1` 项目的 `yolov11s_int8_1b_source.bmodel`、`yolov11s_int8_1b_nopre.bmodel`、`yolov11s_int8_1b_back.bmodel`、`yolov8s_int8_1b_nopre.bmodel`，以及部署目录里的 `yolov11s_int8_1b_back_reflectivejacket_1.bmodel`（与 `_back.bmodel` md5 相同）。旧 0.5.1 + Python sail 和新 0.5.3 + C++ bmrt 都复现；C++ 路径 3 张图 0 框，但 inference 仍有 2.755 ms，说明推理在跑、只是分类输出为 0。
- 临时绕过/决策：**暂缓 INT8，反光衣改用 FP16**；C++ 代码路径不变。见 `DECISIONS.md` ADR-009。
- 根因猜测：INT8 bmodel 的 Transpose/分类分支在转换或量化后输出被清零；不是单个文件损坏，更像这批 INT8 模型的量化/导出流程有问题。需要重新导出 ONNX、检查 qtable/校准数据，或换一个已知可用的 INT8 模型对比。
- 相关文件：`tests/yolov11_official_demo/probe_model.py`, `run.sh`, `docs/assets/new_fp16_probe.txt`, `docs/DECISIONS.md` ADR-009

### KI-002 官方 demo 不能显示中文类别名

- 现象：用中文 `reflectivejacket.names` 时，检测框上的标签显示 `???`。
- 原因：官方 demo 用 `cv::putText`，OpenCV 的 Hershey 字体只支持 ASCII。
- 临时绕过：验证时用 `reflectivejacket_en.names`（`vest` / `no_vest`）；**新 C++ 服务已改用 `cv::freetype::FreeType2` + 中文子集字体**（`docker/fonts/wqy-subset.ttf`），预览/告警图能显示“穿反光衣/未穿反光衣”；官方 demo 本身仍是 ASCII。
- 相关文件：`tests/yolov11_official_demo/data/reflectivejacket_en.names`, `cpp/yolov8_bmcv/yolov8_det.cpp`, `src/task.hpp`, `docker/fonts/wqy-subset.ttf`

### KI-003 运行 demo 时出现 `/proc/jpuinfo: Permission denied`

- 现象：每次解码图片打印两条 `Failed to open /proc/jpuinfo: Permission denied`，随后仍打印 `Open /dev/jpu successfully`，解码正常。
- 影响：目前只影响日志观感；Docker 化时要确认容器内 `/dev/jpu`、`/dev/bm*` 设备与权限都映射正确。
- 相关文件：无（libsophon/JPU 运行时行为）

### KI-004 链接时出现 `.dynsym local symbol ...` 警告

- 现象：用 GNU ld 链接厂商预编译的 `libbmcv.so` / `libopencv_*.so` 时出现大量 `.dynsym local symbol at index N (>= sh_info of 3)`。
- 影响：只是警告，产物可正常生成并运行；这是新 binutils 对旧 `.so` 的兼容性提示。
- 处理：不抑制、不手改厂商 `.so`；如果以后升级工具链导致链接失败，再记录新 ADR。
- 相关文件：`env/sdk/**/lib/*.so`

### KI-005 旧实现忽略配置的检测节奏

- 现象：后端下发的 `frameInterval`、`detectSection`、`detectThreshold` 在 `ai_package` 里没有生效，代码硬编码每 5 帧、窗口 10、`sum>=6`；告警视频固定 5+7 秒。
- 影响：前端配置改了但行为不变；告警灵敏度/视频长度与需求不符。
- 处理：新实现把这些全部放进 `TaskContext`/`AlarmEngine` 配置，禁止硬编码；见 `SERVICE_TASKS.md` OLD-03/OLD-09。
- 相关文件：`ai_package/Yolo.py:438/468/470`、`ai_package/Damo_Yolo.py:433/463/465`、`ai_package/video_analysis.py:443`

### KI-006 当前 C++ 服务只支持反光衣模型（已解决，迭代 5）

- 原现象：只移植 YOLOv11 后处理，Damo 模型任务失败。
- 现状：`ModelSpec` 注册 5 类 + 反光衣天空；Damo 两输出后处理、逐类 NMS、ROI、每模型告警已接。实测安全帽 7~9、人员 3~6、抽烟 1（conf 0.1）；救生衣参考视频 `2/4/1`（`docs/assets/lifejacket_ref.log`）。
- 残留：真实 ISC 任务仍未联调。
- 相关文件：`src/task.hpp`, `src/model.hpp`, `docs/assets/damo_*.jpg`, `docs/assets/lifejacket_ref.log`

### KI-007 告警视频未生成，`video` 为空（已解决，RTSP）

- 原现象：本地 5 秒 mpeg4 视频 `decoder.dump` 无文件。
- 现状：`dump` 只支持 H.264/H.265，且调用后必须继续读 post 秒；RTSP 实测生成 9.64 秒、2560×1440 H.264、1.44 MB 文件，后端告警详情 `videoPath` 正确。
- 残留：mpeg4 本地视频仍不能 dump；`decoder.dump` 路径字符串必须保持生命周期（已用 `pending_.tmp_path`）。
- 相关文件：`src/task.hpp`, `docs/assets/alarm_video_frame.jpg`

### KI-008 模型调度只有单实例，不是目标调度器（已被 ADR-079 取代）

- 现状：`ModelScheduler` 按 busy 选择实例，满载时扩容到 `AIDETECT_MAX_INSTANCES`（默认 2）；36 路日志出现 2 条 `model loaded: safetyhelmet`。
- 现状：janitor 按 `AIDETECT_MODEL_IDLE_SECONDS`（默认 120s）回收空闲实例；测试 5s 时 `/debug/scheduler` 从 2 实例变为 1。
- 2026-11-12（迭代 40）：全局 `infer_mu_` 已删除，改为 FIFO 公平闸门 + 有界并发（ADR-079）；15 任务并行时等锁 0ms，见 `docs/assets/infer_scheduler_measure_20261112.txt`。
- 相关文件：`src/model.hpp`

### KI-009 4 组合流只验证了 1 路，36 路未压测（部分解决）

- 现状：36 路 640×360/2fps 30 秒，`all_1..all_4` 均返回 1920×1080 3×3 JPEG；硬件 `bmcv` 合流。2026-11-03 复测 36 路全部 running/stop 正常。
- 残留：36 路 1080p/10fps 未证；4 组 MultiDecoder 36×640 可跑，但 36×1080/RTSP 停止会 `Jpu_GetError`/double free，生产仍每任务 Decoder。
- 相关文件：`src/server.hpp`, `docs/assets/service_aggregate.jpg`

### KI-010 Docker 镜像 902 MB，依赖本地 `aibox-api` 基础镜像

- 现状：已用 `/opt/sophon` 软链修复 bmlib `memory_op.so`；镜像 902 MB，依赖 `aibox-api:latest`。
- 影响：首次构建依赖基础镜像；基础镜像升级可能改变 glibc/依赖。
- 处理：见 ADR-015；后续可导入 ubuntu 20.04 rootfs 或多阶段裁剪。
- 相关文件：`docker/Dockerfile`

### KI-011 HTTP/WS 线程/连接，36 路 WS 客户端有上限

- 现象：每个 HTTP/WS 连接一个线程；WS 还有独立发送线程；发送队列最多 2 帧。
- 影响：少量客户端没问题；36 路预览 + 前端多页面时线程数偏高。
- 处理：见 ADR-014；后续换 async Beast 或固定线程池。
- 相关文件：`src/server.hpp`

### KI-012 本地视频读到 EOF 后忙循环

- 现象：本地测试视频结束后，`decoder.read` 持续返回错误，`Task` 每 200ms 打印 `decoder read failed, retry`；生产 RTSP 不会 EOF，但测试日志会刷。
- 影响：只影响本地测试；不崩服，但日志量大、CPU 空转。
- 处理：本地文件读到 EOF 后应退出或 sleep 更长；RTSP 重连另做。
- 相关文件：`src/task.hpp` `run`

### KI-013 ISC 地址未实现（已实现，真实网络不通）

- 现状：`IscResolver` 已实现 HMAC-SHA256 签名 + HTTPS POST + `data.url`；本地 HTTPS mock 验签 `sig_ok=True`，解析出 RTSP 后任务 area 542,296 字节。
- 现状：2026-11-03 探测用户给的 8 个可用通道，配置全部解析到 `10.10.69.250:443`；本盒子 TCP 全部 `Connection refused`，另一台相同配置盒子可用，属于本机网络路由/VPN 差异。
- 相关文件：`src/isc.hpp`, `docs/assets/isc_mock.log`

### KI-014 盒子没有 docker compose 插件，Compose 文件未实际验证（已解决）

- 现状：已安装 Compose v2.29.7 静态二进制到 `/usr/local/bin/docker-compose` 和 `/usr/local/lib/docker/cli-plugins/docker-compose`；`config/up/ps` 与任务 area 实测通过。
- 注意：Compose 文件要求 `AIDETECT_JWT_SECRET` 环境变量，否则 `config` 直接报错。
- 相关文件：`docker/docker-compose.yml`, `README.md`

### KI-015 救生衣模型缺清晰测试样本（已解决）

- 现象：`lifejacket` 原图 704×576 人物太小，整图未检出。
- 现状：`reference/video/未穿救生衣_1/2/3.mp4` 三段视频分别 `detections=2/4/1`，见 `docs/assets/lifejacket_ref.log`；裁剪样本也曾 `1`。
- 残留：无（更多现场样本仍可补）。
- 相关文件：`docs/assets/lifejacket_ref.log`, `docs/assets/damo_lifejacket.log`

### KI-016 36 路压测不是 1080p/10fps 全规格

- 现象：默认 36 路测试使用 640×360、`frameInterval=25`（1fps 检测）、`AIDETECT_PREVIEW_INTERVAL_MS=500`（2fps 预览）。
- 结果：36 个任务 30 秒稳定，RSS 375 MB，CPU 331%，TPU 24%，2 个模型实例；默认不宣称 36×1080p/10fps。
- 现状：`AIDETECT_DECODE_MODE=multi` + 640 预览时，32/36×1080 输入可 32/36 running、stop 后 `running=[]`，服务存活；32×1080/10fps 实测 CPU 44%、TPU 6%。MultiDecoder dump 补丁对本地 640 告警生成 1.07 MB 视频；RTSP/ISC 的 multi 视频 `Decoder::dump` 崩溃，ISC/RTSP 仍用 `single`。默认 `single` 保留视频。`HighResGate` 默认 12 路。
- 相关文件：`docs/assets/service_load.log`
- 相关文件：`docs/assets/service_load.log`

### KI-017 真实 ISC 需要代理，开发盒直连被拒

- 现象：开发盒 `10.10.206.68` 到 `10.10.69.250:443` TCP `Connection refused`；同网段 `10.10.206.25` 可直连。
- 现状：通过 .25 的 10443（HTTPS）/10444（RTSP）TCP 代理，8 个通道中 7 个出图；八所预制区 RTSP open 失败，25K 偶发 DESCRIBE 500。ISC 解析出的 RTSP 可在 multi 读帧，但 multi 告警视频崩溃，生产用 single。
- 处理：正式网络放行或把代理做成 systemd；见 `docs/ISC_PROXY.md`。
- 相关文件：`src/isc.hpp`, `docs/ISC_PROXY.md`

### KI-018 模型空闲实例不回收（已解决）

- 原现象：`ModelScheduler` 只扩容不回收。
- 现状：janitor 每 5s 扫描，超过 `AIDETECT_MODEL_IDLE_SECONDS` 且 busy=0 的冗余实例被回收；测试 5s 从 2 实例变 1。
- 相关文件：`src/model.hpp`

### KI-019 强杀进程可能锁死 JPU，需重启恢复

- 现象：反复 kill 测试进程后，`cv::imencode`/`bmcv.imencode` 卡在 `jpu_ioctl`；本地 640 也卡；重启盒子后 JPU refcount 0，全部恢复。
- 影响：生产 stop 要走优雅停止；`/task/stop` 会 join 线程，不要直接 kill -9。
- 处理：服务端已按 task stop/join；运维避免强杀；如锁死只能重启。
- 相关文件：无（驱动行为）

### KI-038（已修复）MP4 通道反复黑屏「正在重连」= 直连解码未节流 + EOF 当故障

- 现象（2026-11-19）：所有 MP4 通道预览每 2~4s 在画面和黑底白字「正在重连」之间来回闪，
  看起来像网络抖动，实际 MP4 是最稳定的源。
- 根因：`sail::Decoder` 直连本地文件时按解码速度跑（3~9s 的测试片 1 秒内播完），
  播完 `read()` 返回 EOF；代码把 EOF 当“连续 5 次读失败”，每次都
  `publish_status("正在重连监控视频流...")` 推一张黑底白字状态帧，然后重开——
  `decoder reconnected` 实测 **125 次/分钟**。
- 修复（迭代 70）：`probe_file_fps()` 用 libavformat 取帧率，按 `1000/fps` 节流；
  文件 EOF 视为循环点，立即重开且**不推状态帧**；读失败连续 3 次（≈0.6s）才显示重连。
- 验证：18 路 MP4 全 ok、60s health 零翻转、`decoder reconnected` 0、WS 抓 106 帧无黑底帧。
- 别再退回“文件走 ffmpeg 网关”：实测 18 路 MP4 网关 + 全通道 ring 会触发 codec-open
  风暴把 VPU 会话耗尽（全盒 `Failed to open codec`，只能重启盒子）。

### KI-039（已修复）告警视频消失 + 文件名 19700101

- 现象（2026-11-19）：告警图片在、视频目录空；偶尔有文件但名字是 `19700101_101259.mp4`。
- 根因：① 容器沿用压测 env `AIDETECT_NO_DUMP=1` + `AIDETECT_ALARM_RING=0`，packet ring
  与 sail dump 都被关，只剩 `--dump-only` worker，而 worker 打不开 ISC 一次性 URL
  （rc=5/139）；② `save_alarm` 把 `now_ms()`（steady_clock）当 wall clock 写文件名。
- 修复（迭代 70）：告警时间用 `system_clock` 反推帧时刻，`AlarmRingRecorder` 同步改
  wall clock；packet ring 只在有网关 fan-out 的通道启用（不额外占相机会话），
  其余通道继续走 worker。
- 验证：ISC 任务 `/debug/trigger_alarm` → `packet ring clip frames=249 duration_ms=9946`，
  1.2MB H.264 1280×720/9.95s；图片名 `20261119_235229.jpg`。
- 注意：`NO_DUMP=1` 下 RTSP 直连通道的 worker 只有后置窗口，没有 pre-roll（见 README 下一步 13）。

### KI-040（已记录）codec-open 风暴会耗尽 VPU 会话，重启进程无效

- 2026-11-19 调试 MP4 网关方案时复现：几十路解码器同时 open 失败重试后，全盒
  `bmvpu_dec_create failed` / `VPU_DecIssueSeqInit queueing fail`，此时连本地单个 MP4
  都打不开（`--dump-only` rc=124/5），`docker restart` 无效，**只能重启盒子**。
- 预防：部署前先确认解码路径不产生“全通道同时重开”；改配置后看
  `Failed to open codec` 计数是否持续增长，一旦增长立即停手，别叠加实验。

### KI-041（已修复）人形分数被类别 sigmoid 砍半，0.8 阈值永远不过

- 现象：MP4 通道（生产告警视频片源）完全没有人形框，反光衣/安全帽/抽烟/救生衣全不触发。
- 根因：`postprocess_yolov5` 用 `score = obj * sigmoid(class_logit)`；`people`/`people_big`
  的 4 个类别列在导出模型里恒为 0，`sigmoid(0)=0.5`，0.89 的人形被记成 0.44，低于
  `threshold_for_people`（生产默认 0.8，任务里常配 0.8）。生产按 obj 计分。
- 修复（迭代 71）：`ModelSpec.obj_only`，people/people_big 用 obj 计分、只出 1 类框。
- 验证：同帧人形 obj 0.89 → 检测框 score 0.89 → 反光衣/抽烟立即出框。
- 别再改回：任何“给 people 乘类别概率”的写法都会让人形整体消失。

### KI-042（已修复）deep 人形模式只换模型不切瓦片，远处的人直接消失

- 现象：4K/2.5K 通道画面里明显有工人，模型一个框都不出（整帧 max_obj=0.05）。
- 根因：整帧 letterbox 到 832×480 后远处工人只剩几十像素；生产 `enable_people_deep_mode`
  实为 `get_deep_mode_board_boxes_from_any_detection`：按 0.6×W/H 切 4 块（20% 重叠）
  逐块推理再合并（REA：字面量 0.6/0.4 + 长度 4 的矩形数组）。
- 修复（迭代 71）：`run_detection()` 对 deep 任务跑 4 瓦片 + NMS，结果按
  `AIDETECT_PEOPLE_DEEP_INTERVAL_MS`（默认 1200ms）缓存。
- 验证：测试_1 同一帧 0.6 瓦片 obj 0.89（整帧 0.05），测试_6 4K best 0.887。
- 代价：每任务每 1.2s 4 次推理，整机 CPU 约 +100%。

### KI-043（已实现）生产目标框状态机：蓝框→蓝标签→5s 红框红标签

- 生产告警图实证：NoVest 告警画**人形框**+红底白字；NoHelmet 告警画**人头框**+红底白字。
- `MinorHelmetDet::boxesCropPolicy` 反编译可见 `状态==2 && >5000ms` 的确认逻辑 → 5 秒。
- 我们：基础人形/人头框蓝色常显；告警目标出现时蓝框+蓝底白字（带倒计时）；5s 后红框+红底白字；
  V3 用人头框、反光衣/安全帽/抽烟/救生衣用人形框；同一人被属性框覆盖时隐藏蓝色基础框。
- 验证：测试_18 蓝框+「未戴安全帽 5s」蓝标签+红标签；测试_1 5s 后红框红标签。

### KI-044（已修复）反光衣裁剪用“上 30%”导致人人判“未穿”

- 生产 `MinorVest::boxesCropPolicy`：CropPolicy 矩形 = 源人形框，宽高比例 0xFF/0xFF（100%）。
- 我们原来取人形框上 30%（只含头部），反光衣模型看不到衣服 → 全部“未穿反光衣”。
- 已改为整框裁剪；但 4 头分类器在 38~50px 小裁剪上仍不区分穿/不穿（见 KI-045）。

### KI-045（记录，未完全解决）4 头反光衣分类器在低分辨率裁剪上不区分穿/不穿

- `tools/attr_probe` 实测（测试_18，704×576）：穿橙色/黄色背心的工人与穿棕色外套的工人
  head0 都是 0.95~1.00；已排除归一化（1/255）、BGR、拉伸 vs letterbox 的差异。
- 4K（测试_2，155~215px 宽）无背心人员 head0=1.00（正确），但缺高分辨率“穿背心”正例校准。
- 缓解：`AIDETECT_ATTR_MIN_CROP_W/H`（默认 40×80）跳太小裁剪；根治需生产 4K 正例做类别/阈值校准。



### KI-046（已修复）看门狗 _exit 不释放 ISC 会话 → 全通道 500 / 无画面  - 2026-11-21 15:26：dmabuf=1003，看门狗直接 `_exit(0)`，解码器未关闭 → 上游 ISC 会话堆积 →   ISC 对所有通道返回 500，容器随后 Exited(255)（`--restart` 未生效，需 `docker start`）。 - 修复：`TaskManager::stop_all()`（逐个 `Task::stop()`+`join()`，触发 RTSP TEARDOWN），   看门狗先优雅停任务再退出（v1.0.9）。  ### KI-047（未解决）dmabuf 缓慢泄漏  - 现象：20h 从 ~130 涨到 1003，触发看门狗。 - 推断：`process_frame` 每帧 `sail::BMImage copy(handle, ...)` + `make_shared` 供推理队列，   未池化（生产是池化输入）。下一步改成每任务双缓冲池 + 在飞标记。  ### KI-048（取舍）基础出框阈值恢复 0.8，小目标不再出框  - 迭代 72 用 0.5 让测试_17/4 出框，但 4K 远景纹理 0.5~0.6 的误检全部被画出来（“框乱飞/屋檐框”）。 - 恢复生产阈值 0.8；测试_17/4 这类 obj 0.47~0.56 的通道暂时无框，待确认生产是靠人头/人数模型出框。  ### KI-049（已改）抽烟误报：行为算法改用上半身裁剪  - 整人裁剪 +10% 时 256×256 里香烟只有几像素；改为上 60%（宽 1.10、上边 -5%），贴近生产 minorId=4。

## 刻意为之的「坑」

### TRAP-001 `BUILD_TYPE=pcie` 在 aarch64 上表示“原生编译”，不是 PCIe 卡

- 看起来像：以为要插 PCIe 加速卡，或者以为编错了平台。
- 实际是：sail 上游脚本用 `BUILD_TYPE=pcie` 表示 native 构建分支；在 aarch64 盒子上直接编出 SoC 可用的 `.so`。交叉编译才用 `BUILD_TYPE=soc`。
- 依据：`DECISIONS.md` ADR-002
- 改了会怎样：改成 `soc` 会走交叉编译分支，要求 `aarch64-linux-gnu-g++` 和 `LIBSOPHON_BASIC_PATH`，当前盒子没装交叉工具链，直接失败。

### TRAP-002 构建命令里的 `-Dthe_libbmcv.so=...` 不能删

- 看起来像：一个多余的、带点的奇怪变量。
- 实际是：上游 native 分支靠这个变量把 bmcv 加进 `basic_link_libs`；我们绕过了 `find_package(libsophon)`，不传就会留下 44 个未定义 bmcv 符号，`libsail.so` 的 `NEEDED` 里也没有 `libbmcv.so.0`。
- 依据：`DECISIONS.md` ADR-003
- 改了会怎样：`test_sail` 可能在加载/调用 bmcv 时失败；更隐蔽的是 demo 自己链接了 bmcv，可能“看起来能跑”，但其他只链 sail 的程序会挂。

### TRAP-003 官方 demo 的 classnames 解析会吃掉每行最后一个字符

- 看起来像：类别名文件随便用 LF 换行就行。
- 实际是：`yolov8_det.hpp` 里 `line = line.substr(0, line.length() - 1);` 假设行尾有 `\r`；用 LF 文件时会把 `vest` 变成 `ves`。
- 处理：本仓库的 `.names` 文件用 CRLF；新增类别名文件也必须 CRLF，或者改解析代码。
- 相关文件：`tests/yolov11_official_demo/data/*.names`, `cpp/yolov8_bmcv/yolov8_det.hpp`

### TRAP-004 反光衣 FP16 必须 `/255`，不要照搬旧代码的 `ab=[1,1,...]`

- 看起来像：所有 YOLO bmodel 都应该按 0-255 喂，或者都按 0-1 喂。
- 实际是：新反光衣 FP16 `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel` 实测 `norm=1` → 534 个 anchor 误检；`norm=255` → 62 个正常。旧 Python 的 YOLO11 分支用 `convert_to(alpha=1, beta=1)`（0-255），不能照搬。
- 依据：`docs/assets/new_fp16_probe.txt`
- 改了会怎样：0-255 会让分类分数虚高、误检暴增；0-1 喂 Damo 模型则分数塌陷。归一化必须作为模型注册表元数据。

### TRAP-005 `SAILConfig.cmake` 的 `find_library` 路径要补 `${SAIL_LIB_DIRS}`

- 看起来像：装完 sail 后 `find_package(SAIL)` 应该直接可用。
- 实际是：上游 `SAILConfig.cmake.in` 里的 `find_library(... PATHS ${SAIL_LIB_DIRS} ...)` 在 `configure_file()` 时被提前展开成空，安装后的 config 变成 `PATHS  NO_DEFAULT_PATH`，`SAIL_LIBRARY` 为 NOTFOUND。`setup_sdk.sh` 安装后用 `sed` 补回了 `${SAIL_LIB_DIRS}`。
- 处理：如果手工重装 sail，记得重新跑 `setup_sdk.sh`，或手动检查 `env/sdk/sophon-sail/lib/cmake/SAILConfig.cmake`。
- 依据：`DECISIONS.md` ADR-003 同类的“上游构建脚本坑”
- 改了会怎样：`find_package(SAIL)` 静默找不到库，CMake 报 `SAIL_LIBRARY-NOTFOUND`。

### TRAP-006 AI 服务只验证 token，不签发

- 看起来像：AI 服务应该提供 `/get_token`、`/refresh_token`。
- 实际是：token 由前端/后端自己签发；AI 只在 HTTP/WS 入站时验证（HS256/iss/aud/7 天），出站调后端时用 `create_jwt("admin")` 自己签。旧源码也没有签发接口。
- 依据：`docs/INTERFACES.md` §1
- 改了会怎样：多写两个没人调的接口，还要凭空造用户库；更容易把认证职责搞乱。

### TRAP-007 `/task/start` 的 `pid` 是服务进程 PID，不是任务线程/子进程

- 看起来像：前端应该用这个 pid 去停任务。
- 实际是：服务是单进程，`pid` 只是兼容字段；前端停任务只传 `taskId`，AI 按 `taskId` 停对应线程。
- 依据：`docs/INTERFACES.md` §2.1
- 改了会怎样：如果前端把 pid 当任务句柄，重启/多任务时会停错或停不掉；不要引入“pid 即任务”的假设。

### TRAP-008 Docker 基础镜像 Debian 12 需要手工补 OpenSSL 1.1

- 看起来像：基础镜像里有 `libssl.so.3` 就够了。
- 实际是：二进制的 `libsail`/OpenSSL 依赖 `libcrypto.so.1.1`，Debian 12 只有 `.so.3`；Dockerfile 从 `docker/lib/` 拷贝 1.1，并设置 `LD_LIBRARY_PATH`。`ffmpeg` 也要用 SDK 的，不是系统自带的。
- 依据：`docker/Dockerfile`, ADR-015
- 改了会怎样：容器启动时报 `libcrypto.so.1.1: cannot open shared object file`，或告警视频 `ffmpeg: not found`。

### TRAP-009 `image_copy_to` 要求输入/输出格式一致

- 看起来像：任意 `BMImage` 都能直接贴到 canvas。
- 实际是：`cv::imdecode`/`resize` 的格式可能是 `BGR_PACKED`，canvas 若是 `BGR_PLANAR` 会抛 `mismatch BMImage` 甚至 abort；Aggregator 在 copy 前显式 `convert_format` 到 canvas 格式。
- 依据：`src/server.hpp` `Aggregator::loop`
- 改了会怎样：合流线程崩溃/黑图。

### TRAP-010 `decoder.dump` 只支持 H.264/H.265，且必须继续读 post 秒

- 看起来像：调用 `dump(5,5,path)` 后 sleep 等文件即可。
- 实际是：`dump` 只开始写，文件在继续 `decoder.read()` 的 post 秒内才落盘；且 mpeg4 会报 `only support h264 and h265`。另外传给 `dump` 的 path 字符串必须活到写完。
- 依据：`src/task.hpp`, `docs/assets/service_video.log`
- 改了会怎样：视频为空、内存踩坏/`corrupted size vs. prev_size`。

### TRAP-011 告警视频 path 生命周期

- 看起来像：`std::string tmp = ...; decoder.dump(..., tmp);` 返回后就能扔掉。
- 实际是：C++ `dump` 可能保留 path 指针异步写；必须用 `pending_.tmp_path` 成员保持生命周期。之前用局部 string 导致 dump 后堆损坏。
- 依据：`src/task.hpp` `save_alarm`
- 改了会怎样：进程 abort。

### TRAP-012 Docker 里 bmlib 仍找 `/opt/sophon/libsophon-current`

- 看起来像：设置了 `LD_LIBRARY_PATH=/opt/sophon-sdk/...` 就够了。
- 实际是：bmlib 的 a53lite runtime 硬编码 `/opt/sophon/libsophon-current/lib/dyn_load/memory_op.so`；Dockerfile 必须建 `/opt/sophon/libsophon-current` 软链，否则容器启动 `bmlib api fail`/`bm_memset_device failed`。
- 依据：`docker/Dockerfile`, `docs/assets/docker_final.log`
- 改了会怎样：容器反复重启。

### KI-020 multi 解码组在大量失败源下会拖死组内正常通道

- 现象：30 个后端任务（多数 ISC `missing data.url`/`500`）用 `AIDETECT_DECODE_MODE=multi`
  启动后，4 个 MultiDecoder 组的解码线程被失败通道占住，正常通道（如 6488 摄像机）
  也停止出帧；切换 `single` 后 6 个可达通道恢复（704×576/1280×720/1920×1080）。
- "组内隔离"解释：multi 模式是 4 个 `sail::MultiDecoder` 实例 × 9 个 channel，同一组
  的 9 路共享该实例的读/重连线程和 VPU 队列；某一路源失败时，SAIL 会在组实例里反复
  reconnect/报错，其他路即使源正常也可能排不到帧。single 模式是每个 Task 一个
  `sail::Decoder`，失败只影响自己（但更耗句柄/内存），所以当前部署用 single。
- 影响：多路部署的预览/检测容量取决于源可达性，不能宣称 30 路全部可用。
- 绕过：当前生产容器用 `AIDETECT_DECODE_MODE=single`（每任务独立 Decoder）。
- 待办：失败通道隔离（组内重建/降级）、按源失败率踢出任务、恢复后重新加入。
- 依据：`docs/PROGRESS.md` 迭代 6、容器 `single`/`multi` 对比。

### KI-021 生产 Web 登录凭据不可用，未做生产 UI 端到端采样

- 现象：`http://10.10.207.46/` 登录页需用户名/密码，已知的 `Admin/admin` 组合无法登录；
  为避免影响生产，未做口令尝试。
- 影响：生产预览的端到端（浏览器实际播放）没有采样；技术方案结论来自只读的进程/连接/
  二进制符号/配置帮助文本，以及开发盒同硬件的复现。
- 绕过：用 `libEdgeSystem.so` RTTI/字符串 + `libboard_sdk_bm.so` DWARF 符号 +
  `QualityOfPreview` 配置说明还原方案（见 `docs/PREVIEW_PIPELINE.md`）。
- 待办：拿到生产只读账号后，用浏览器抓一次生产预览的 JPEG 流做帧率/分辨率对照。

### KI-022 预览 JPEG 质量暂无档位

- 现象：`bmcv.imencode` 使用 SAIL 默认质量，1080p 单通道约 4.3 MB/s；生产有
  `QualityOfPreview`（0/1/2 或 5~100）。
- 影响：带宽/画质不可调；低带宽现场可能需要降质。
- 待办：增加 `AIDETECT_PREVIEW_QUALITY`，映射到硬件 JPEG 质量参数。
### KI-023 206.68 到 ISC 的直连 ACL 仍存在；10445 转发已加，30 路 RTMP 仍受限

- 现象：`206.68 -> 10.10.69.250` 的 83/443/554/1935 直接连接仍 `refused`；`.25` 的
  10443/10444/10445 转发可用；生产 `207.46 -> 10.10.69.250:1935` 大量 ESTAB。
- 影响：当前只能经 `.25`。30 路 RTMP 经 10445 仅 1 路 ISC 出图（ISC RTMP 会话/
  转码容量）；30 路 RTSP 经 10444 为 **6 路 ISC + 1 路直连**，仍低于生产。
- 绕过/现状：部署以 `AIDETECT_ISC_PROTOCOL=rtsp` + 10443/10444 为主；10445 RTMP 已
  作为可选路径保留（单路 8.5fps 验证通过）。
- 待办：网络放行 443/554/1935，或提升 .25/ISC 的 RTMP 并发/转码容量后再切 RTMP。

### KI-024 任务冷启动前 5 秒内告警没有完整预录

- 现象：任务刚启动、decoder 缓冲不足 5s 时触发告警，dump 只能给出已有部分或只有 post；
  这是所有"前 5s 预录"方案的共性（生产同样需要预热）。
- 影响：刚启动立刻告警时，图片可能靠近/位于视频开头，而不是正中。
- 待办：任务启动后前 N 秒的告警在 reason 里标注"预录未就绪"，或先丢弃前 5s 告警。

### KI-025 部分 ISC 相机在流媒体网关侧返回 500/超时

- 现象：Artemis `previewURLs` 对相机 `23595aa2...`/`f72a...`/`75019c...` 能返回 URL，
  但 RTSP 网关 `DESCRIBE` 返回 `500 Internal Server Error` 或读超时；`6fbed442...`
  直接是 `Camera not found`。
- 影响：这些通道无论怎么重连都没有画面，属于 ISC/相机侧，不是本机解析失败。
- 待办：ISC 侧核对相机在线状态、`streamType`、`transcode` 与网关会话上限；恢复后复测。

### KI-026 multi 模式的告警视频仍不是"单媒体源"

- 现象：单模式已改为"ffmpeg 网关（一条上游）+ 两路本地 MPEG-TS"，解码与包环共用同一
  PTS；multi 模式未走网关，仍直接用相机/ISC URL + 包环/worker，可能再建一条流连接。
- 影响：multi 部署下 ISC 双会话竞争仍可能出现；单模式已与生产"一路媒体源"一致。
- 待办：multi 也改成单媒体源/共享包队列，或恢复 single 作为唯一模式。

### KI-027 告警同框去重与录制配额未实现

- 现象：生产 `AlarmRecorder::isSameRectAlarm`/`lastAlarmTime` 对同框告警去重，并有
  `VideoRecordLimitMB=20` 配额；我们只有模型窗口 + cooldown，没有同框去重/录制配额。
- 影响：同一目标持续停留可能重复产生告警；长时间告警录制可能占盘。
- 待办：按生产补同框去重、`VideoRecordSec/LimitMB/Path/Prefix` 配置。
### KI-028 sail::Decoder::dump 在 30 路/1440p 下会崩溃

- 现象：单模式开启 dump 后，告警视频正常生成且图片居中，但进程在告警上报后
  `free(): invalid pointer`（libavutil `av_buffer_unref`）崩溃，容器重启、30 个任务全丢。
- 影响：dump 不能作为生产告警视频主路径；当前显式 `AIDETECT_NO_DUMP=1`。
- 绕过：单模式用 ADR-026 的"网关 + 时间戳包环"；multi 用包环/worker。
- 待办：向 SAIL/算能反馈复现；在 SDK 修复前不要在生产打开 dump。
### KI-029 改通道地址做 MP4 回放后容器 PID1 变 zombie、9000 端口残留

- 现象：把 b062 的 `tb_channel.address` 临时改成 `/data/.../media/*.mp4`，重启
  aidetect 后进程退出但 PID1 为 `[aidetect] <defunct>`，旧 socket 仍占
  `0.0.0.0:9000`；`docker restart`/`docker rm -f` 都会卡住，最后 `kill` shim
  + 删容器 + 用原 env/binds 重建才恢复。
- 影响：期间 AI 服务不可用；需要 reboot 才能彻底清掉孤儿 netns/socket。
- 绕过：不要用“改线上通道地址 + restart”做回放；要回放就建独立临时通道/任务，
  或直接用离线片段验证。若已卡住：`ps -o pid,ppid,stat -p <容器PID>` 看到
  `Zsl`，`kill -9 <containerd-shim PID>`，`docker rm -f`，再按
  `.work/dev/_recover_aidetect.sh` 的 env/binds 重建。
- 待办：把回放做成独立的 `AIDETECT_DEBUG` 测试任务，不碰线上通道。

### KI-030 合流瓦片偶发一帧彩色花屏（已加丢弃/保留上一帧防护，根因未定位）

- 现象：2026-11-12 抓 all_1 合流时，9 格中的测试_6 格出现过一帧彩虹色块（同一时刻
  测试_6 的单通道预览正常、无花屏）；随后 24 帧复抓正常。
- 复现：`node .work/browser/cap_long.js all_1 .work/m1_now 40`，约 60 帧里 1 帧。
- 影响：合流画面偶尔闪一帧彩色块，不影响检测/告警；单通道不受影响。
- 猜想：合流 composer 每 100ms 取 9 个瓦片 JPEG 解码后 `image_copy_to` 到画布，
  如果某一格拿到的是刚被替换/正在编码的瓦片会解出坏图；也可能是该路解码线程
  偶发丢包（4K/4096 源，UDP 缓冲已 8MB）。
- 待办：给瓦片 JPEG 加完整性校验（SOI/EOI + 解码失败重试一次），或 composer 在
  解码失败时保留上一帧缓存而不是贴黑/贴坏图；需要时单独压测复现。
- 2026-11-12（迭代 41）：已加防护——瓦片编码后校验 SOI/EOI（`jpeg_markers_ok`），
  不合格直接丢弃；composer 解码失败时保留上一帧瓦片而不是贴黑/纯色。
  但**根因仍未定位**（仍可能是该路解码丢包），需要时单独压测复现。
- 相关文件：`src/server.hpp`（`Aggregator::loop`）、`src/task.hpp`（`render_tile_fast`）

### KI-031 33 个任务并行时预览约 2fps（盒子容量上限，不是推理调度）

- 现象（2026-11-13 现场，33 个任务同时 running）：测试_2 单通道 2.1fps、
  all_1 合流 2.3~2.6fps；测试_2（4K）单轮检测 199~836ms（15 任务时 22~32ms）。
- 实测排除项：`infer stats` 5s 内约 400 次推理，`wait_avg=0`、`peak_pending=5~9`，
  即推理准入队列**没有排队**；并发 2 与 4 的帧率/检测轮完全一样。
- 结论：33 路解码 + VPP/JPU 共享硬件 + 预览跟踪的 CPU 开销（aidetect 约 258% CPU，
  CPU 空闲 39%）才是瓶颈。15 任务以内表现正常（同一测试_2 5.8fps）。
- 处理：默认并发保持 2；30+ 任务场景要么减少每任务算法数（4~5 个 → 2~3 个）、
  要么降低预览订阅数量，或换更高算力盒子。需要时可做“按总任务数自适应降检测节奏”。
- 相关文件：`src/model.hpp`（闸门）、`src/task.hpp`（检测节奏/预览）、
  `docs/assets/infer_scheduler_measure_20261112.txt`
### KI-032 合流预览帧率仍略低于生产（33 任务 6.5~7.1fps vs 生产 22 任务 8.7~9.2fps）

- 2026-11-13（迭代 44/45，均按“启动后 ≥3 分钟、连测 3 次”口径）：每算法节奏 +
  预览缩放一次把合流从 2.8 提到约 3.0；再让**检测裁剪也用缩放帧**后
  合流 **6.5~7.1fps**、all_2 5.2fps、2.5K 单通道 6.3~8.1fps（生产 720p 单通道 4.9fps）、
  4K 测试源单通道 2.5~3.2fps。生产当前只配置 22 个任务，合流 8.7~9.2fps。
  现场 33 个任务里只有 21 个 health=ok（6 connecting、6 stalled，项目部网络/ISC 侧），
  所以是“21 路有图 vs 生产 22 路”的同量级对比，合流仍差约 25%；
  剩余因素主要是 12 路 4K 测试源和合流瓦片的渲染预算。

- 现象（2026-11-13，网页只读实测）：
  - 生产 `10.10.207.46`：22 个任务，合流 1920×1080 **8.7~9.2fps**，
    单通道 **1280×720 4.9fps**；
  - 我们 `.25`：33 个任务（多 11 个），合流 **2.5~3.0fps**、单通道 **2.0~2.5fps**。
- 已排除：推理闸门 cap=4 时 `wait_avg=0`、`peak_pending=8~10`，
  单轮检测 72~399ms；把预览降到 720p、加积压降节奏都只带来小幅提升。
  因此瓶颈是 **33 路解码 + 每任务 4~5 个算法的 VPP 预处理 + 预览/合流编码
  抢同一套 VPP/JPU**，不是调度排队。
- 下一步（按生产结构重构）：`ImageInference` 每算法独立实例与节奏、按算法的
  frame-skip 令牌（生产 `onAllAlgoSkipped`/`algoSubmitted` 的语义）、把 33 任务
  的推理需求从 ~104 次/s 压到 ~50 次/s；另需对比同任务数（22）下的帧率。
- 相关文件：`src/model.hpp`、`src/task.hpp`、`docs/assets/prod_preview_channels_20261113.png`

### KI-034 ISC 凤凰咀两路 + 乙烯南路2/排水渠1 仍无画面（ISC 侧）

- 2026-11-13（迭代 46）：ISC 改走子码流后，11 路里 7 路恢复。剩余：
  - 广航局-三公司-凤凰咀-游客中心 / 博物馆：gateway `method DESCRIBE failed:
    500 Internal Server Error`（ISC 侧拒绝；生产预览页游客中心同样 0fps）；
  - 疏浚公司-泉惠项目-乙烯南路2：connecting；排水渠1：stalled（2560×1440）。
- 这些不是本机取流代码问题（`isc.hpp` 三个快照逐字节相同），属于 ISC/相机侧；
  生产侧同时段也只有部分通道有画面。需要项目部在 ISC 上确认这几路的状态。
### KI-034（更新 2026-11-13 迭代 47）ISC 仅剩凤凰咀两路 + 排水渠1

- 改回“按通道 streamType 取流 + HEVC 直通”后，八所_预制区等已恢复；
  剩余：凤凰咀-游客中心/博物馆（gateway `DESCRIBE failed: 500`，生产侧游客中心同样 0fps）、
  泉惠-排水渠1（会反复重连）。均属 ISC/相机侧。

### KI-035 4K 片源按原尺寸预览的帧率代价（迭代 48）

- 2026-11-13：预览改为按源尺寸后，12 路 4K 测试片源（3840×2160/4096×2160）也按 4K 编码，
  33 任务下测试_2 1.6fps、合流 2.0fps；真实相机 ≤2560×1440 不受影响。
- 需要 4K 压测时可设 `AIDETECT_PREVIEW_WIDTH=1920 AIDETECT_PREVIEW_HEIGHT=1080`（迭代 46 的档位）。

### KI-036（已修复）VPP 非整数倍缩放导致预览花屏

- 2026-11-13 迭代 50：`bmcv.vpp_resize` 在 2560×1440→1920×1080（×0.75）等非整数倍下
  输出彩色噪声。已改为整数倍才用 VPP，其余用 CPU `cv::resize`；跟踪缩放同样改为整数倍。
  若今后有人改回固定 960 宽跟踪或加新的缩放点，需注意这条限制。

### KI-037（已澄清）RSS 增长实为启动预热，非泄漏

- 2026-11-14 迭代 57：预热后 4.5 分钟 RSS 仅 +0.5MB（0.1MB/min），dmabuf 持平、
  dmabuf 总大小恒 21.5MB；此前记录的 4MB/min 来自启动阶段池化缓冲/arena 建立。
  若未来再现持续增长，再用 `AIDETECT_NO_INFER` / `AIDETECT_NO_PREVIEW` 二分定位。

<!-- pm:meta
iteration: 104
updated: 2026-11-19
commit: -
-->

# AI 盒子 C++ 检测服务端（gh-aibox-dev）

一句话定位：把原先 Python 写的 BM1684X AI 检测服务端用 C++ 重写并 Docker 化；当前已是**可运行的 MVP+**：5 类模型、ISC、告警 5+5 视频、多实例调度、36 路压测、硬件加速合流、Docker/Compose 均已跑通；实时预览已按生产方案重构为**按需渲染 + 真内容 10fps + 1080p 只缩不放**。

## 当前状态

- **2026-11-19 迭代 104（停机/门控/时延）**：注册 PID1 `SIGTERM/SIGINT → _exit(0)`，
  `docker stop -t 5` 从 **6871ms/exit 137（SIGKILL）** 变为 **1447ms/exit 0**，不再产生僵尸容器；
  告警上报门控按生产 `table_alarm_interval` 语义**落盘持久化**（重启后不重复放行，日志
  `alarm interval state loaded: 16 entries`）；`/task/list` 新增 `firstFrameMs`（1.3~5.2s）
  与 `alarmLatencyMs`（6~45ms）。预览按 ADR-097 对齐生产：**默认 5fps、按源尺寸编码**
  （实测 4K 2560×1440/3.81fps、704×576/5.04fps，ADR-097 取代 ADR-023/088）。
  详见 PROGRESS 迭代 104。

- **2026-11-18 迭代 103（预览全灭根因修复 + 生产对齐第一批）**：预览消失是 `Task::stop_gateway()` 被任务线程与 watchdog 线程**并发 join** → SIGABRT →
  PID1 僵尸容器（`docker ps` Up 但不可 exec，只能重启盒子）。已用 recursive mutex + recorder
  `life_mu_` 修复，并修掉解码器重建时的设备内存 churn（失败也释放、frame unref、先释放后创建、
  `extra_frame_buffer_num=1`、200ms 节流）：dma-buf 对象稳定在 1288~1342、`bm_live_mb` 559~726、
  RSS 1.01GB→trim 后 0.73~0.81GB、restarts=0。同时落地：设备侧 JPEG（`hw_jpeg_ok=17/0`）、告警视频
  H.264/8.98s/≤20MB（pre=0.5×duration）、遥测 jpu/vpu/bm_live、子码流改显式开关、
  预览 cap 只缩不放。详见 PROGRESS 迭代 103。

- **2026-11-18 迭代 101**：修好 ≤1080p 通道预览空白（planar 拷贝）、标签/任务编号彩虹乱码（按行上传）、告警图掉色（planar→packed 读回）、告警视频窗口（现 10.83s，含前后各 5s）；设备 handle 改进程级共享后崩溃消失。SDK 已切 0.5.1+mw0.14 编译通过，但运行时混挂库会 abort，待整镜像替换。

- **2026-11-18 预览/合流修复（迭代 100）**：packed BGR 的 stride 按 w*3 修正后，合流墙与单通道预览恢复出图；ROI 区域线改 CPU 绘制 + 按行写回设备图，斜边/多边形正确。并新增 docs/PRODUCTION_MATRIX.md（与生产盒子四维对比）。

- **2026-11-18 路线 B 已落地（迭代 99）**：新增 `src/routeb/compat.hpp`，用
  `bmcv_*` + `bmrt_*` + sophon-ffmpeg **替换掉 sail**（`ldd` 无 `libsail.so`，调用点未改）。
  解码改 VPU `h264_bm`/`hevc_bm`、推理改 `bmrt_launch_tensor_ex`、图像改 bmcv。
  实测 36 路：CPU **146~173%**（旧实现 266~294%）、dmabuf **290**（旧 500+）、
  30 秒 394~425 条 detect round、告警图片/MP4 正常（ffprobe H.264/1280×720/7.02s）、
  合流墙与单通道预览有画面。旧二进制备份在 `bin/aidetect.pre_routeb_20261118`。

- **2026-11-18 路线 B PoC（迭代 98）**：`tools/routeb_poc/` 用 **sophon-ffmpeg VPU 硬解
  （`h264_bm`/`hevc_bm`）+ `bmcv_*` + `bmrt_*`** 跑通「解码→缩放/归一化→推理→画框→JPEG」，
  **不链接 libsail**。实测单路 2560×1440：软件解码 **35.59 ms CPU/帧** vs VPU **3.25 ms**
  （**10.9 倍**）；全链路 13.24 ms/帧、推理 0.13 ms/帧、120 帧 dmabuf 零增长。
  这把「4K 解码烧 6 核」的根因（`sail::Decoder` 其实是软件解码）量化并有解了。
  详见 `tools/routeb_poc/README.md`。

- **2026-11-16 修复（四十三，单体架构）**：新增 `src/ingest_bridge.hpp`，用 **libavformat 在进程内**
  完成“拉流 → mpegts 封装 → 本地 UDP”，**替换掉每路 ffmpeg 子进程**；同一份 TS 扇出到解码与告警环
  端口。关键坑：mpegts 一次吐出最多 64KB，直接 `send()` 会被 IP 分片、而解码端按 1316 字节收 → 
  **必须按 1316 切片发送**。实测：**ffmpeg 进程 0**、线程 221、RSS 657MB、`RestartCount=0`、
  36 路 **34 ok / 2 connecting**、ISC 25K 与龙港-钢栈桥抓图正常。`AIDETECT_INPROC_INGEST=0` 可回退网关。
- **2026-11-14 修复（四十二）**：逆生产确认它**没有重启机制**（app 连续 39.5h、NRestarts=0、
  dmabuf 恒 133，靠每通道池化复用帧）；我们的增长来自连不上相机的**高频重连**。
  已给解码失败加重连退避，`dmabuf` 从 +1.3/分钟变为稳定 604~618。
- **2026-11-14 修复（四十一）**：ISC 再次全部无画面，根因仍是 BM 缓冲累积（15h 后 dmabuf 1359，
  稳态 610~660），先把 ISC 解码器拖死。看门狗阈值收紧到 **dmabuf>1000 / RSS>2000MB**；
  并加 **ISC 候选轮换**（解码器打不开编码时换下一个 streamType/协议，不再死循环）。
  重启进程后 29 ok，ISC 全部恢复（25K 704×576、预制场 1280×720）。
- **2026-11-13 修复（四十）**：大面积“没画面”的根因是 **BM(ion) 缓冲池耗尽**（故障时 dmabuf 925，
  正常 610~660），ISC 重连风暴叠加长时间预览把池子顶满，重启任务无效、重启进程立即恢复。
  已加资源看门狗：dmabuf>1500 或 RSS>2600MB 时记日志并自重启进程（阈值可配）；
  缩放算法本身无问题（1280×720 输出干净、dmabuf 不随缩放增长）。
- **2026-11-13 修复（三十九）**：预览缩放**全走 VPP**，按整数倍缩到 1080p 以下
  （2560×1440→1280×720、3840×2160→1920×1080、4096×2160→1024×540）。
  实测整数倍 VPP **比不缩放更省**：4K 单帧 render 160~288ms→42~79ms、合流 3.4→7.0~7.3fps、
  内存 1527→1473MB，CPU/TPU 不变。CPU 缩放仅作非整数尺寸的兜底。
- **2026-11-13 修复（三十八）**：2560×1440 通道预览花屏，根因是 `bmcv.vpp_resize` 只对
  **整数倍**缩小正确（×0.75 会读出彩色噪声）。现在只在整数倍时用 VPP，其余保留源帧由
  `cv::resize`（CPU）缩放；跟踪缩放改为“减半到 ≤1280”。花屏消失，合流 2.9→4.9fps。
- **2026-11-13 修复（三十七）**：按房规把单通道预览上限恢复为 1920×1080（源 ≤1080p 不缩放）。
  A/B 结论：4K 帧 JPEG 2.50MB→0.79MB（−68%）、渲染耗时约减半，但 **CPU/内存无变化**
  （瓶颈在解码+推理）；检测与告警仍用解码原帧，不受影响。生产是原尺寸出图，
  这里是与生产的一处知情差异，`AIDETECT_PREVIEW_WIDTH/HEIGHT` 可随时切回。
- **2026-11-13 修复（三十六）**：生产实测 >1080p 预览**不缩放**——生产新加的 2560×1440 通道
  预览页是 2560×1440 @ 5.00fps；反编译 `JpegEncoderInstance::onImagePoped` 只有
  `create_jpeg_encode(w,h,q)` + `frameToJpegByCpu`，没有 resize。我方 `preview_target` 已去掉
  1080p 上限按源尺寸出图（同一相机：生产 5.00fps / 我方 5.0fps @2560×1440）。
  4K 测试片源需要降档时用 `AIDETECT_PREVIEW_WIDTH/HEIGHT`。
- **2026-11-13 修复（三十五）**：按生产反编译结果对齐取流——① ISC **不自动切主/子码流**，
  按通道配置的 `streamType` 取（生产 `table_media.MediaUrl` 逐路写死，25K=1、预制场=0）；
  ② 补 HEVC 直通（识别到 `Video: hevc` 时 `-bsf:v hevc_mp4toannexb`），主码流 H.265 可解码；
  ③ 预览不放大、>1080p 才缩到 1080p（生产实测 704×576/352×288/1280×720 均为码流原尺寸）。
  25K 现在走主码流 704×576，与生产一致。
- **2026-11-13 修复（三十四）**：ISC 流无画面的根因是**主码流 H.265 解不出来**
  （`frame watchdog: no first frame`），与取流代码无关（`isc.hpp` 三个快照逐字节相同）；
  ISC 候选改为**子码流优先**（生产预览也是 704×576/720p 子码流），11 路 ISC 恢复 7 路，
  整机 21→28 路有图。单通道预览上限恢复规格 **1920×1080**（上一轮误降到 720p）。
  1080p 预览下合流 2.8~3.0fps、2.5K 单通道 6.3fps、ISC 720p 单通道 6.2~8.3fps。
- **2026-11-13 修复（三十三）**：检测裁剪也用缩放帧（有人可裁时每轮 4K→720 缩放一次，
  人形框换算后从缩放帧裁剪，人形检测仍用原图）——33 任务稳定后合流 **3.0→6.5~7.1fps**、
  all_2 5.2fps，2.5K 单通道 6.3~8.1fps（生产 720p 单通道 4.9fps、合流 8.7~9.2fps/22 任务）。
  同时纠正测量口径：任务刚重启 30~60s 内只有部分任务上量，读数偏高；之后统一
  “启动后 ≥3 分钟、连测 3 次”。生产侧核对：当前只配置 **22 个任务**（不是 36）。
- **2026-11-13 修复（三十二）**：按生产结构继续压缩预览开销——①**每算法独立节奏**
  （`AIDETECT_ALG_INTERVAL_MS` 默认 400ms，boat×1.25/behavior×1.5，基础人员模型
  每轮），测试_2 单轮从 103~617ms 降到 56~345ms，推理需求降约 20%；
  ②**每帧只缩放一次**（对齐生产 `DecodeInstance::requireScaled`）：4K 源缩到
  1280×720 后，跟踪/单通道/合流瓦片共用，`render_ms` 从 30~268ms 降到 20~67ms。
  33 任务实测：**合流 2.8→5.6~7.0fps**、测试_2（4K）2.4→3.2~3.7fps、
  2.5K 单通道 6.9~7.9fps（生产 720p 单通道 4.9fps、合流 9fps/22 任务）。
- **2026-11-13 修复（三十一）**：①标签之间的空行是**行为模型用了 1.1 倍扩边裁剪框**
  当上报框（比其他算法的人形框高 6px），已改回人形框，两栈合拢；②标签文案按任务
  实际开启的行为开关生成（只开抽烟 → `抽烟`，多开用 `/` 连成一个标签），底板改为
  **构建期预渲染**：`tools/gen_label_plates.cpp` 生成 `docker/labels/plate_*.png` +
  `labels.json`（16 个标签），运行时只读清单，**不再在任务启动时画字**；
  ③对照生产实测（网页只读）：生产 22 任务、合流 8.7~9.2fps、单通道 1280×720/4.9fps；
  我们 33 任务合流 3.0fps/单通道 2.5fps。推理闸门 cap=4 时 `wait_avg=0`（cap=2 有
  13~19ms 排队），故并发默认改为 **4**；同时把单通道预览上限对齐生产 1280×720，
  并加“积压时自动降检测节奏”。生产合流仍快我们 3 倍，瓶颈在 VPP/JPU/解码共享，
  下一轮按生产每算法独立流水线继续重构（KI-032）。
- **2026-11-13 修复（三十）**：标签底板改为“先画后量”（`putText` 后用
  `inRange+boundingRect` 取真实字形框），修掉 `抽烟/打电话` 末字被裁（底板从
  197x34 变成 182x39），三行标签完整可读。另实测 33 任务现场：推理闸门
  `wait_avg=0`/`peak_pending≤9`（不是调度瓶颈），但 33 路解码+VPP/JPU 把盒子打满——
  测试_2 检测轮 199~836ms、预览 2.1fps；并发 4 与 2 无差别，默认保持 2。
  15 任务以内正常（预览 5.8fps）。见 KI-031。
- **2026-11-12 修复（二十九）**：“检测标签无字”已定位并修复：标签底板原来只有 6 张
  PNG，`抽烟/打电话`、`人员聚集` 等算法上报文本没有底板，走 FreeType 兜底时又把
  原点当成左下角（实际是左上角），文字被画到底板外 → 只剩纯红条；快速预览路径更是
  直接不画。现在任务启动/热更新时用 FreeType 为**本任务所有算法标签**现渲染红底白字
  底板，快速路径直接贴；兜底 badge 原点修正。同时把告警目标 TTL 1s→2s、预览轨迹
  关联改成只比 label（行为模型每轮跳类），抽烟标签才终于能确认并显示；同一目标多
  算法命中时只保留一种状态（不再蓝框+红标签+多个倒计时叠在一起）。
  实证：`preview_test2_smoking_label_20261112.jpg`（抽烟/打电话、未穿救生衣、
  未穿反光衣三行都有字、贴同一个框）、`preview_test2_pending_countdown_20261112.jpg`。
- **2026-11-12 修复（二十八）**：推理调度改为**FIFO 公平闸门 + 有界并发**（默认 2，
  `AIDETECT_INFER_CONCURRENCY` 可调；实测 2 与 4 在 15 任务下无差别，需求远低于容量），
  去掉“所有算法共用一把全局锁”。改造前 15 任务并行时单次推理 187ms 里 172ms 在等锁、
  测试_2（4K/5 算法）一轮检测 1.7s，预览因快照超过 1.5s 而**完全不画框**；改造后
  等锁 0ms、测试_2 一轮 22~32ms，15 任务下正常出框并跟人。同时加了生产
  `onAllAlgoSkipped` 等价的需求上限（每任务检测节奏、裁剪只取最大 4 人）、
  多算法同目标**合成一个框 + 竖排标签**（消除嵌套/偏移框），
  漏检一次不再立刻清框（600ms 人形锚点宽限）。数字与图：
  [调优前后实测](docs/assets/infer_scheduler_measure_20261112.txt)、
  [生产调度符号证据](docs/assets/prod_infer_scheduler_syms_20261112.txt)。
- **2026-11-12 修复（二十七）**：预览框改为**按检测帧绑定 + 人员锚点**：
  轨迹保存 `(帧时间, 框)` 历史，新检测只把“检测框 − 该帧轨迹姿态”的
  增量修正到当前帧，不再把旧检测框重置到新帧；LK 失败时用内框模板匹配
  兜底。属性算法漏检时用全帧人员框把标签重新锚到人身上；框中心离开 ROI
  当帧删除，属性框在最新快照中无匹配且无人员锚点也删除。旧的 1s
  `active` 边缘框缓存已移除。单通道/合流约 8.7fps（36 路）。
- **2026-11-12 修复（二十六）**：预览框逐帧光流跟踪（LK，640 宽灰度），
  检测间隔内框跟着人走；ROI 边缘最后一个确认框保留 5s→1s，人出线后
  很快消失。检测本身仍是 AI 推理频率。
- **2026-11-12 修复（二十五）**：检测框按帧时间对齐（`dets_history_`
  选择不晚于预览帧的最新检测，再速度外推）；通道新增 MP4 协议：上传
  MP4 后任务循环播放，预处理/检测/预览与直播流同路径，但不上报告警。
- **2026-11-12 修复（二十四）**：钢筋厂2/3/4 的 ISC `cameraIndexCode`
  在 ISC 侧返回 `Camera not found`（生产日志也用同码重试），分组黑屏
  状态改为显示 `Camera not found`，不再只显示“已断开 6705s”；需要更换
  正确 cameraIndexCode/直连 URL 才能恢复画面。gateway UDP 接收缓冲
  1MB→8MB，修复 4K 单通道底部彩色撕裂。
- **2026-11-11 修复（二十三）**：无任务通道也能显示视频状态/分辨率。
  生产是 MediaChannel（视频通道）与 TaskChannel（检测任务）分离；我们
  不重构 39 路常驻媒体层（会额外占 ISC/VPU），改为 AI 按需
  `/channel/probe`：`ffmpeg -c copy` 只读流头、6s 超时、60s 缓存、2 并发，
  不进 BPU。通道页未配置任务的直连通道实测 `正常 2560x1440`；任务页
  按生产显示 `未运行/运行中/Input/output error`。
- **2026-11-11 修复（二十二）**：通道/任务列表显示真实视频状态。通道页
  新增“通道状态”，按 AI 帧状态显示 `正常 + 分辨率` / `正在连接` /
  `已断开 Ns` / `Input/output error` / `Camera not found`；任务页状态列
  覆盖后端“运行中”，显示真实健康。`/task/list` 新增 taskName、
  channelAddress、resolution、health、lastError。实测 14K `正常
  1280x720`、25K `正常 704x576`，失败任务显示 I/O 错误。
- **2026-11-11 修复（二十一）**：视频流卡死自动重连。根因是 gateway
  被杀后 `Decoder::read()` 仍阻塞，Task 永远 running 但无帧；现在
  `TaskManager` 对**曾经出过帧**且 2 分钟无帧的 Task 逐个 stop/start
  （30s 间隔、10min 退避），并把 `lastFrameAgeMs/everFrame` 暴露到
  `/task/list`。5 路重点 ISC 通道实测全部恢复；从未出帧的 ISC
  通道不再自动重启，避免抢会话。ISC previewURL 缓存 30min、失败回退
  旧 URL、请求间隔 120ms。
- **2026-11-10 修复（二十）**：ROI 外的检测目标不再画框/告警。原过滤只
  作用于基础人形检测，V3/vest/boat/behavior 的 crop 结果没有过 ROI；
  现在 `update_alarm` 入口和 `dets_` 写回前都按框中心过滤，算法自己的
  region 为空时回退任务级 ROI。实测 7c 低阈值下区域外目标被过滤，6488
  近全屏 ROI 内目标仍保留。
- **2026-11-10 修复（十九）**：运行中任务实时同步 ROI 和算法参数。原
  `main.cpp` 只在启动时调用一次 `reconcile`，且配置签名不含 ROI 和
  `threshold_for_people`；现改为**保存成功后前端直连
  `/task/reload_config` 触发**（生产 `handleRemoteMessage → onTaskArrived
  → archiveUpdated` 的同类事件驱动），推理线程帧间原地热更新
  `models_`/ROI，不释放槽位；2s 轮询保留为兜底，仅源地址/任务名变化才
  stop/start。实测 16:27:49 保存 ROI 后 `direct hot update` +
  `hot config applied models=3 rois=1`、无 task started；移除反光衣后
  models 由 3 个变 2 个，预览不再出旧标签；14K 保存 ROI 后单/合流均出现
  绿色区域线。
- **2026-11-10 修复（十八）**：合流框不跟人已修：检测框时间改为推理帧
  时间、推理队列只留最新帧、预览按帧龄外推；配置区域改取无框原图；
  单通道/合流/告警图按生产样式绘制绿色 ROI 区域线（ROI 保存后前端直连
  AI 触发热更新，2s 轮询兜底，14K 绿线已复验）；第 37 个任务被
  前端 36 路上限和 AI HTTP 409 双重拦截，不再出现“运行中但配置区域
  服务器错误”。
- **2026-11-10 修复（十七）**：新增任务页勾选算法后参数区空白已修复。
  根因是新增模式 `taskDetail` 为 null 时 `idx !== -1` 仍为真，解引用
  `taskAlgoResult` 抛异常；改为仅当 `idx >= 0` 且有记录时读已保存值。
  实测新增页 3 个算法 → 3 张配置卡/20 个输入。
- **2026-11-10 修复（十六）**：合流预览慢动作根因是解码线程内同步做 9 路
  JPEG；改为每任务 preview worker + latest-frame（丢过期帧），实测 64s
  窗口内合流 OSD 也走 64s，all_1 约 9.2fps。重连/未连接通道 1Hz 重发
  状态画面（时钟 + 已断开秒数），单通道不再只有 spinner。合流每格右上
  新增生产式黑底白字任务编号条，重连格也有、未配置格没有。合流槽位改为
  首空顺序分配，停止只留空不搬其它格；`VideoRoute=36` + 前端兜底使
  通道4 出现。上报门控 `max(任务3分钟, 算法86400s)` 已实测生效。
- **2026-11-10 修复（十五）**：多标签改回生产式垂直堆叠；删除新目标按
  中心距离继承红框的兜底，第二目标重新走 5s 蓝框倒数；6488 持续时长
  恢复 5s。
- **2026-11-10 修复（十四）**：通道新增重名自动改 `_2`；标签统一无置信度；
  预览 dets 保留 5s，消除推理空窗导致的 3s 无框。
- **2026-11-09 修复（十三）**：告警视频 ring 落盘失败回退
  `dump_in_worker`，并修复 pending 期间新告警丢视频；告警图标签固定
  贴框；`crowd_head` 隐藏，只保留生产同款人员聚集/人数监控；接入
  “人员检测基础配置” 0.85/deep=1（6488 演示任务 later 调回 0.3/0.0
  以恢复检测）；告警视频 watchdog 15s→45s，15:46 三亚新告警视频均
  True；17:49 视觉验证标签底边贴红框上沿、无间隙，多标签仅在重叠时向上叠；deep=1
  已切换生产 `people_big`（832×480/[1,8190,9]）；Server 2s 轮询配置
  签名，参数修改会自动热更新 task；抽烟按生产阈值调度，无额外几何模型；新增
  [算法逆向对照](docs/ALGORITHM_PARITY.md)、
  [告警视频链路对比](docs/VIDEO_PIPELINE.md)。
- **2026-11-09 修复（十二）**：按用户决定接受 ISC 并发配额限制，保持
  36 路同等取流；取消优先级、previewURL 10 分钟复用、1.2s 错峰。
  直连 RTSP 可绕过 ISC 配额，DecodePool/HighResLease 均 36 路；但 36 路
  同时 4K 仍受网络带宽/VPU 限制。
- **2026-11-09 修复（十一）**：修复任务编辑“服务器错误”——批量脚本插入的
  6 条 `tb_task_algo.created` 是 Unix 浮点数，API 期望 DateTime 字符串；
  改回日期字符串后 UpdateTask 返回 200。V3/vest 双告警均已落库。
  蓝色 pending 只显示倒计时不显示标签；上报间隔取任务 3 分钟与算法间隔
  的较大值，3 分钟内只抑制新上传，预览仍保留红框/标签。已确认目标标签
  一直显示，多算法同一目标标签向上错位；6488 恢复 helmet_v3+vest+boat
  三算法。所有通道同等启动，ISC previewURL 10 分钟复用、每 1.2s 错峰启动；
  仍只有 11 个 decoder、64 次 ISC 500，证明是 ISC 会话硬配额约 11，
  不是本地 36 路 decoder 不足。
- **2026-11-09 修复（十）**：恢复数智技术中心的 `vest`，与 `helmet_v3`
  并行；已实测同一任务 09:44:49 出“未戴安全帽 0.99”、09:45:16 出
  “未穿反光衣 0.98”，两条告警图/视频均落库。看门狗杀 gateway 后额外向
  decoder UDP 端口注入垃圾 TS 包唤醒阻塞 read；五个点名通道均恢复连续帧。
- **2026-11-09 修复（九）**：Task 增加 frame watchdog（15s 无帧/60s 无首帧
  杀 gateway 重连），泉惠排水渠1/凤凰咀博物馆/乙烯南路2/龙港/凤凰咀游客
  均已拿到 decoder 且连续两帧不同；V3 改为独立阈值，办公室白衬衫人员
  已输出“未戴安全帽 0.94”标签；告警图标签改板端直接绘制，不再依赖 Pillow。
- **2026-11-09 修复（八）**：办公室 `数智技术中心_test` 任务去掉 `vest`，
  只保留 `helmet_v3`，消除“未穿反光衣”误报红框；合流通道黑格是
  ISC 500/会话上限导致的无 decoder，不是前端问题。
- **2026-11-08 修复（七）**：behavior 按 YOLOv10 5 类 + people 人形框解析，
  近景吸烟者 class0 命中；救生衣确认 minor152 走 major1 人形框，和生产
  可见逻辑一致；凤凰咀强制触发告警已验证 HEVC→H.264 视频落库，
  DB `video_path` 存在、HTTP 200 `video/mp4`、1920×1080 H.264。
- **2026-11-08 修复（六）**：30 个任务已从 `helmet` 全量切到 `helmet_v3`；
  按生产 `minorId=195/majorId=1` 用人形框顶部 70% 裁剪，class0=未戴、
  class1=安全帽、阈值 0.5/0.7，206.25 日志已出现 `helmet_v3` 实际推理。
  behavior 模型按生产 YOLOv10 5 类解析并用 people 人形框（不再用 head），
  按 cigarette/call/phone 分类阈值判断；参考视频能框住近景吸烟者。
  HEVC 告警视频改为软件 HEVC 解码 + h264_bm 编码，并给 ring 文件加 3s 重试。
- **2026-11-08 修复（五）**：救生衣改为**人形框裁剪**推理（不再全帧扫水面），
  抽烟/打电话改为**始终扩成上半身**再用 0.5 阈值、吸烟 1s、只留最可信的
  一个候选人；14K/25K 预览已无救生衣误检框。V3 修正了类别顺序并改用 head
  供框，但黄帽工人仍被判“未戴”，暂不全量替换。
- **2026-11-08 修复（四）**：实时预览不再显示误检框。根因是 `boat_life_jacket`/
  `people_gather` 在无 ROI 河道场景大量误检，且预览把非目标检测画成绿框。
  已从 14K/25K/28K 移除这两个算法（八所中/北移除救生衣），预览/告警渲染只画
  `target/pending/confirmed`；14K/25K 预览已恢复干净画面。
- **2026-11-08 修复（三）**：告警媒体 URL 改走 dotnet 静态目录 `/aibox/<save相对路径>`
  （`/data/...` 被 nginx SPA fallback 成 index.html，是图片/视频全挂的根因）；
  告警视频写完检测 HEVC 并用板载 `h264_bm` 转 H.264，浏览器可播。
- **五通道取流根因**：ffmpeg 网关继承 AI 的 UDP socket，后启动的网关占用前一个
  decoder 的端口，decoder 报 `Address already in use`。已改 `fork` + 子进程关闭
  fd 3..1023 + 唯一端口分配 + 启动清理残留网关；ISC 按 RTSP/RTMP × streamType
  0/1 回退；reconcile 优先启动五个重点通道。实测 30/30 running、11 路 decoder，
  14K/八所预制区/凤凰咀游客中心/乙烯南路2/龙港钢栈桥全部有画面。
- **生产参数/V3**：`helmet_v3` 双阈值 + 上部 35% person crop + 持续时长/重复间隔，
  参考视频实测只报 2 次；`tb_algo_config` 已同步 8 种算法的生产独有参数
  （数值/开关/下拉），任务编辑页按算法渲染。
- 可运行性：C++ 服务 `build/aidetect/aidetect` 可启动；HTTP/WS 9000、任务启停、occupancy、area frame、任务/合流 WS、5 模型推理、告警图/视频/上报、模型多实例、4 路硬件合流均已验证；Docker 镜像 `aidetect:dev` 902 MB，Compose 已安装并部署。
- **实时预览（2026-11-05 重构）**：对齐生产盒子的"按需缩放 + 每任务 JPEG 编码 + 合流实例"方案：
  - 仅在有人订阅单通道 / `all_N` 时编码；无人观看时只保留 1fps 的 area 图；
  - 单通道 2560×1440 → 1920×1080，1920×1080/1280×720/704×576 原样输出（只缩不放）；
  - 源 >10fps 用令牌桶抽到 **真内容 10fps**，≤10fps 不补帧不抽帧；实测 11fps→10.0、
    9fps→9.0、25fps→10.0，重复帧 0；
  - 3×3 合流 1920×1080，按内容更新发送（实测 9.0~9.8fps，无重复帧）；
  - 告警视频仍为环形分片 `-c copy`，实测 2560×1440@25 源生成
    `2560×1440, 24.92fps, 10.03s` 片段；预览重构不影响告警画质；
  - 详见 [docs/PREVIEW_PIPELINE.md](docs/PREVIEW_PIPELINE.md) 与
    [docs/assets/preview_verify.txt](docs/assets/preview_verify.txt)。
- **告警视频（2026-11-05 重构）**：按生产 `libdump4` 的"回溯比例 0.5"思路，单模式新增**单媒体源网关**（每路一条上游 → 两路本地 MPEG-TS，解码/包环共用同一 PTS），告警包环取前后 5s；实测图片 OSD 19:59:43、视频 9.96s/250 帧，第 5 秒帧 OSD 19:59:42（同场景），图片居中。旧 ring（图片 18:57:24、视频 18:57:10–20）已复现并替换；dump 方案因 30 路下崩溃弃用。详见 [docs/ALARM_VIDEO.md](docs/ALARM_VIDEO.md)。
- **ISC（2026-11-05）**：逆向发现生产 URL 请求 `protocol=rtmp&transcode=1&videotype=h264`，旧代码却写死 `protocol=rtsp`；已改为解析并转发这些参数、断流重新 resolve；单模式由网关只保留一条上游连接，解码/包环读本地流。实测 ISC 单通道 1280×720/9.88fps；30 路容器曾实测 **7 路出图 = 6 路 ISC + 1 路直连 RTSP**；2026-11-06 迁移任务后实测仅 3 路 `decoder opened`，大量 `gateway no video stream`，受 `.25` 代理/ISC 会话限制（生产 .46 为 25 路直连）。206.68 到 ISC 的 83/443/554/1935 均被 ACL 拒绝（HLS 的 83 也不通），.25 已增加 `10445 → 1935`，RTMP 单路 1280×720/8.5fps 验证通过；30 路 RTMP 受 ISC 会话容量限制，当前部署用 RTSP 为主（见 ISC_PROXY.md）。详见 [docs/ISC_STREAM.md](docs/ISC_STREAM.md)。
- **算法插件/五种算法（2026-11-06）**：逆向生产 `libalg_plugin_*.so` 的 `get_plugin/get_params/get_desc/get_extra/get_extern` 自描述插件 ABI，提取 head/crowd/helmet/behavior/suit 六个 bmodel 并验证；已接入五算法并验证：head 2 个 Head 框、helmet 未戴安全帽、vest 未穿反光衣、people_gather/head_count 计数、smoking_call 抽烟告警；插件启动加载器可从 `libalg_plugin_*.so` 自动提取 bmodel；前端源码在 `web/`，已把五算法写入任务配置 DB 并验证显示/勾选；29 个工地任务已迁到新算法，办公室 6488 保留旧 `_our`；停止按钮实测 PATCH 200。新增 `GET /algorithm/list`。详见 [docs/ALGORITHM_PLUGINS.md](docs/ALGORITHM_PLUGINS.md)。
- **前端/任务迁移（2026-11-06）**：前端源码在 `web/`，算法配置由 dotnet `data.db` 驱动；已加入五算法与 `*_our` 旧算法，29 个工地任务迁到新算法，办公室 6488 保留旧算法；任务停止按钮根因 `n-data-table :loading` 覆盖层拦截点击，已改为仅空表时 loading，`pnpm build` 重新部署后停止/启动可点击（PATCH 200）；预览实现蓝框（未确认）→红框（确认）并把检测间隔降到 100ms；已用 NoVest 视频截到蓝框，红框旧位置问题通过 300ms 新鲜度丢弃 + vx/vy 速度外推（800ms）修复，5s 外推过冲已回退，并加同标签 IoU>0.5 去重。详见 [docs/PRODUCTION_ALARM_LOGIC.md](docs/PRODUCTION_ALARM_LOGIC.md)、[docs/PRODUCTION_PARITY.md](docs/PRODUCTION_PARITY.md)。
- **任务参数/告警状态（2026-11-06）**：任务编辑页移除间隔帧数/触发帧数/触发区间/上报间隔/视频时长，只留算法配置；全局上报间隔（分钟）和视频时长（秒）移到参数配置页；检测内部为 3s 蓝框倒计时→红框常驻，目标消失 1s 后重置，上报按全局分钟间隔。前端已重新构建部署，30/30 running。详见 [docs/ALGORITHM_PLUGINS.md](docs/ALGORITHM_PLUGINS.md) 第 9 节。
- **206.25 覆盖部署（2026-11-06）**：aidetect 镜像、805MB 模型、data.db、web/dist 已同步到 10.10.206.25；.25 上 30/30 任务 running、约 6~8/30 路 decoder（生产同样只有 6~8 路有画面，属项目部网络问题）；.68 aidetect 已停以释放 ISC；代理代码复制到 `/data/CT/project/ai/aibox-proxy-68`。详见 [docs/DEPLOY_25.md](docs/DEPLOY_25.md)。
- **2026-11-08 修复（二）**：停止任务 500 的根因是 decoder 读阻塞导致 `thread_.join()` 卡死；停止改为 detached cleanup，先杀 gateway，再异步 join；`.25` 实测 14K、入口2 stop/start 都返回 200；停止时清空 Aggregator 槽位，避免预览串框/旧相机构图。
- **2026-11-08 修复**：任务开始/停止幂等（AI 重启后按 DB status=1 自动 reconcile 29 个任务），告警图片/视频 URL 不再错误加 `/aibox/` 前缀（直接 `/data/...`），补齐生产阈值参数（未戴安全帽/反光衣/抽烟/打电话/人员阈值等）并新增 `helmet_v3`（`minor_helmet.bmodel`，YOLOv10 3 类）。
- **2026-11-07 修复**：`.25` 的 `AIDETECT_ISC_PROTOCOL` 被留在 `rtsp`，已切回 `rtmp`，30 任务恢复 6~8 路 ISC 出图；任务管理开始/停止被 `SecretModal` 的空遮罩挡住（外部图片加载失败、不可见），已移除并验证 PATCH 200；告警状态改为**每目标独立**（IoU/中心距匹配，新目标重新 3s 倒计时）；5 个“救生衣检测_our”任务已切到 `boat_life_jacket`；crowd 按生产 `CrowdMethod::process_people_head` 实现 `crowd_head`（96×160 双类热力图+offsets，softmax 阈值）。
- **船舶救生衣/ crowd 结论（2026-11-06）**：boat 按 YOLOv8（`VERSION_V8`）接入，
  无 ROI 71 框、`model.taskArea` ROI 后 2 框；生产源码/头文件全盘搜索未找到
  `board_algo_sdk.cpp`/`CrowdMethod*`/`HeatMap*`，按用户决定不接 crowd 密度后处理，
  人员聚集继续用 people 计数。
- 最近验证（2026-11-03，AI 盒子）：
  - `cmake --build build/aidetect -j8` → `BUILD_OK`。
  - **Damo**：安全帽 `7~9`、人员 `3~6`、抽烟 `1`、救生衣参考视频三段 `2/4/1`；真实任务 `6488...` 同时跑反光衣+安全帽。
  - **告警视频**：RTSP 触发后生成 H.264 2560×1440、9.64 秒、1.44 MB 视频；后端 `videoPath` 已验证。
  - **ISC**：通过 `10.10.206.25` 的 TCP 代理，8 个真实通道中 **7 个出图**：25K 281,448、三亚入口 99,408、三亚入口2 618,568、凤凰咀博物馆 653,624、游客中心 326,920、宁波 Y2 202,608、龙港钢栈桥 353,344 字节；八所预制区 RTSP open 失败。视觉见 `docs/assets/isc_real_*.jpg`。
  - **Web UI 验收**：登录 `testuser`，任务管理 30 条分 3 页全部启动，每页 10 条均“运行中”；AI `/task/list` 30 running/0 error。截图见 `docs/assets/ui_tasks_page1/2/3.png`、`ui_home.png`、`ui_preview.png`、`ui_alarm.png`；详见 `docs/UI_ACCEPTANCE.md`。
  - **状态图**：640×360 黑底白字居中，长错误文本换行；完整 WenQuanYi 字体已进镜像；验证图 `docs/assets/ui_status_centered.jpg`、`ui_preview.png`。
  - **检测标签**：Pillow 标签服务（9070）按 `draw_bmcv_chinese` 逻辑绘制红底白字，告警图已实测带白字；C++ 通过 `AIDETECT_LABEL_URL` 调用。
  - **合流空位**：未分配任务的槽位用 `no_task.jpg` 显示“任务未配置”，不再纯黑。
  - **当前部署**：`multi` 模式、30 个任务 running、`AIDETECT_NO_DUMP` 已移除；主机标签服务 9070、视频 worker 9071 运行。
  - **告警视频**：主机 `video_server.py` 调 `--dump-only`；单任务生成 500341 字节视频。
  - **首页温度**：AI `/get_occupancy` 返回 `chip_temp=59`、`board_temp=51`；首页首屏有短暂 51/51 的旧值，约 5-9 秒后刷新为 59/51，截图 `ui_home.png`。
  - **帧率/分辨率**（2026-11-05 起）：单通道按源决定——2560×1440→1920×1080，≤1080p 原样；内容帧率 ≤10fps（25fps 源抽到 10，9fps 源保持 9）；合流 1920×1080（640×360/格），按内容更新；告警图片同单通道规则；告警视频单路 2560×1440/25fps（`-c copy`）。
  - **预览心跳**：Aggregator 每 500ms 推送一次 `all_1..all_4`；Task 1s 无帧刷新状态图；状态图时钟在顶部，前端 blob 持续刷新。
  - **预览单通道限制**：前端源码已确认 `sendKey(taskId)` 存在；九宫格/空槽文字由 AI 服务端生成。206.25 的 `params_init.py` 实际硬编码的是 `rtsp://...207.241`，不是 ISC；206.68 的 `isc://` 仍因 ISC `missing data.url` 无画面。
  - **重连**：`Task::run` 改为外层重连循环；网络/解码失败只回到“正在重连”，任务保持 running，不因临时无流变 error。
  - **ISC 可用通道**：14K 实测解析成功；约 30 路中仍有部分 `missing data.url`，属于远程监控网络/ISC 预览 URL 问题，不作为任务失败。
  - **实时预览**：`StreamHub` 按 taskId/`all_N` 统计订阅者；Task 仅在有观看者时编码（单通道 1080p 上限、合流 640×360），Aggregator 仅在有订阅者时硬件拼图+JPU 编码并按内容发送；容器 30 任务下实测 6488 摄像机 2560×1440→1920×1080 单通道 **9.88fps**、其合流组 **9.75fps**，重复帧 0。
  - **告警视频上报**：最新 58427 告警详情 `videoPath` 非空；主机 video worker 9071 生成 500341~2461996 字节 MP4。
  - **父/worker**：父进程 multi 30 路 + `aibox-label.service`/`aibox-video.service` 独立 worker，systemd 已启用。
  - **worker 持久化**：`aibox-label.service`、`aibox-video.service` 已 `systemctl enable --now`，重启自动拉起。
  - **告警图片 404 修复**：nginx 容器补 `/data:/data:ro` 并加 `/aibox/data/ -> /data/` alias；告警卡片图片恢复，告警上报失败状态属于后端外部上报链路。
  - **ISC 网络诊断**：对比 `.25`（DHCP、可访问）与 `.68`（static、被 RST）；本机无 iptables/rp_filter 差异，判断为 ISC/安全设备按源 IP/MAC 的 ACL 差异。报告 `docs/NETWORK_REPORT_25_vs_68.md`，需要网络/安全运维放行。
  - **36 路 640×360**：36 个任务全部 running，stop 后 `running=[]`，`all_1..all_4` 正常。
  - **32 路 1080p/10fps**：`multi` 模式 + 640 预览，32 个任务全部 running、stop 后 `running=[]`，CPU 44%、TPU 6%、内存 888 MB。
  - **断流状态/重连**：坏流任务保持 running，单通道/合流预览显示黑底白字 `监控视频流状态` + `正在连接/正在重连`，实测 53,000 字节 JPEG：`docs/assets/status_reconnect.jpg`。
  - **ISC multi**：`loop=-1` + 阻塞 read 后，ISC 解析出的 RTSP 可在 multi 模式读帧（25K area 285,480）；但 multi 模式 RTSP 告警视频 `Decoder::dump` 崩溃，ISC 生产仍用 single。
  - **36×1080**：`AIDETECT_DECODE_MODE=multi` + 640 预览，36 个任务全部 running、stop 后 `running=[]`，服务存活；4 组解码纯压测 150 帧/通道 8.24 秒、RSS 129 MB。多模式对本地文件可生成 1.07 MB 告警视频。
  - **硬件合流**：`bmcv.imdecode` + `bmcv.resize` + `bmcv.image_copy_to` + `bmcv.imencode`，3×3 九宫格正确。
  - **Docker/Compose**：902 MB 镜像；容器内任务/area/WS、真实 ISC 通过；Compose v2.29.7 `config/up/ps` 通过。
  - 项目记忆检查：`8 通过 / 0 警告 / 0 失败`。
- 已知限制（重要）：
  - 开发盒到 ISC 需经 `10.10.206.25` 的 10443/10444 TCP 代理（见 `docs/ISC_PROXY.md`）；代理重启后要重跑，建议正式网络放行。
  - 默认 `single` 保留 ISC/RTSP 告警视频；`multi` 可跑 32/36 路 1080p 和 ISC 读帧，但 RTSP 告警视频仍崩溃，暂不作为 ISC 默认。
  - 救生衣参考视频已验证 `2/4/1`；更多现场视频可补。
  - `ModelScheduler` 已支持按 busy 扩容 + 空闲实例回收（`AIDETECT_MODEL_IDLE_SECONDS`，默认 120s）。
  - 告警视频只支持 H.264/H.265 输入；mpeg4 本地视频无法 dump。
  - HTTP/WS 仍是线程/连接同步 Beast，不是最终 async 方案。
  - 曾因频繁强杀进程/驱动异常导致 JPU 卡死；重启盒子后恢复。生产要优雅停服务。
  - 镜像 `aidetect:routeb051d` ≈1.44 GB（同时保留 0.5.1/0.14 与 0.5.3/0.15 两套 SDK）；
    基于本地 `aibox-api:latest`（Debian 12）+ 拷贝 OpenSSL 1.1。
  - 0.5.1 运行时必须 `-v /models:/models:rw`（bmrt 0.5.1 无法从只读 bind 打开 bmodel）、
    必须 `--privileged`（`/dev/jpu`、`/dev/bmdev-ctl`）；`/opt/sophon/*-current|latest` 三个指针
    必须一起指向 0.5.1/0.14，否则 dlopen 会混入 0.5.3/0.15。

## 下一步

0. **0.5.1 运行时 soak（当前最高优先级）**：`aidetect` 已跑在
   `libsophon 0.5.1(SoC)+mw0.14`（镜像 `aidetect:routeb051d`），需连续观察 ≥24h：
   restarts/崩溃、RSS 与 dmabuf 斜率、JPU `/proc/jpuinfo`、ISC 重连次数、告警视频产量与时长。
   同时把本轮「协议钉死 + previewURL 失败失效」的二进制部署后复测 36 路。

0. **ISC 剩余问题（外部依赖）**：龙港-预制场用**新解析**的 RTSP URL 仍 `Server returned 5XX`
   （0.5s 即返），乙烯南路2 新 URL 可出流——前者需 ISC/相机侧排查，后者已由缓存失效逻辑覆盖。
   本机 1935 可达但按相机 RTMP 转码池很小（fresh RTMP 18s 未出流），故钉 `AIDETECT_ISC_PROTOCOL=rtsp`；
   若 ISC 侧放开 RTMP 会话/转码配额，去掉该 env 即回到生产同款 RTMP。

0. **路线 B 全量替换**：已完成（迭代 99，`ldd` 无 libsail）；PoC 见 `tools/routeb_poc/`。
   设备侧 JPEG（`bmcv_image_jpeg_enc`，只吃 NV12）仍可作 CPU 优化项（当前 `cv::imencode` 走 CPU）。

0. **单体架构后续**：观察 24h 的 `dmabuf`/RSS 曲线（桥接后 847 且有缓升）；若继续增长，
   优先检查 `IngestBridge` 的 avio/AVPacket 释放与 `MultiDecoder`/`Decoder` 的重建频率；
   另外可把桥接的重连策略按候选（streamType/协议）轮换，进一步减少 ISC 5XX 窗口。
1. 推理闸门并发默认 2：15 任务下 2 与 4 实测无差别（`wait_avg=0`、`peak_pending≤4`），
   30 路以上再复测；
2. 底层属性算法仍约 1s 一次、远小目标的人形框本身偏大；下一步用更紧的人形框裁剪或
   生产同款 10fps 基础检测做对比，区分“跟踪误差”与“检测框误差”；
3. 合流瓦片偶发一帧彩色花屏（单通道正常），先加瓦片 JPEG 完整性校验/重解码保护；
4. 历史告警里的 HEVC 视频按需批量转 H.264（当前只对新告警转码）；
5. “人员检测基础配置”已接入；deep=1 时加载生产 `people_big`
   （832×480/[1,8190,9]），不再是固定 384×640；
6. 30 路并发下 ISC 仍只稳定约 11 路；已改为所有通道同等、previewURL 复用、
   1.2s 错峰，剩余需 ISC 会话/项目部网络放行；
7. 告警同框去重（`isSameRectAlarm`）与 `VideoRecordLimitMB` 配额；冷启动预录预热提示；
8. V3 需按摄像头做 helmet/no-helmet 阈值与裁剪标定；当前黄帽样本仍会误判，
   不全量替换。`sensitivity_helmet_v3` 也未在 ability schema 暴露；
9. 抽烟/打电话已按生产 `people_*_threshold` 调度；生产没有独立
   hand/mouth/cigarette 行为模型，`CigaretteBoxMethod` 是路面烟头检测；
10. 增加预览 JPEG 质量/带宽档位（对齐生产 `QualityOfPreview`）；
11. 合流任务编号条当前在 Task 构造时取 `taskName` 并缓存，改名需重启任务；
    86400s 上报门控只在进程内保存，容器重启后首次仍会放行一次，后续可
    落盘到 `/data/.../save/.state` 做跨重启保持。

## 5 分钟跑起来

以下命令在 AI 盒子（`ssh linaro@10.10.206.68`）上执行：

```bash
cd /data/CT/project/ai/gh-aibox-dev
source env/env.sh

# 编译服务（首次或改代码后）
cmake -S . -B build/aidetect -DCMAKE_BUILD_TYPE=Release
cmake --build build/aidetect -j8

# 启动（9000；JWT secret 从环境变量注入，不要写进仓库）
export AIDETECT_JWT_SECRET='<from team secret store>'
export AIDETECT_CONFIG_BASE='http://10.10.206.68'
export AIDETECT_REPORT_BASE='http://127.0.0.1'
./build/aidetect/aidetect

# 另开 shell 验证
curl -s -H "Authorization: Bearer <token>" http://127.0.0.1:9000/get_occupancy
```

Docker（推荐 Compose；v2.29.7 已安装）：

```bash
cd /data/CT/project/ai/gh-aibox-dev
export AIDETECT_JWT_SECRET='<secret>'
docker compose -f docker/docker-compose.yml up -d
docker compose -f docker/docker-compose.yml ps
```

等价 `docker run`：

```bash
docker build -t aidetect:dev -f docker/Dockerfile .
docker run -d --name aidetect --privileged --network host \
  -e AIDETECT_JWT_SECRET='<secret>' \
  -v /data/CT/project/ai/models:/models:ro \
  -v /data/CT/project/ai/inference/save:/data/CT/project/ai/inference/save \
  -v /data/CT/project/ai/inference/video_transtation:/data/CT/project/ai/inference/video_transtation \
  aidetect:dev
```

> `env/packages/`、`env/sdk/`、`env/src/`、`env/build/`、`build/`、`tests/**/models/`、`tests/**/bin/`、`tests/**/results/` 是生成物，不提交仓库。

## 文档地图

| 文件 | 何时读 |
|---|---|
| [docs/PROGRESS.md](docs/PROGRESS.md) | 接手时先读最后 3 条 |
| [docs/PREVIEW_PIPELINE.md](docs/PREVIEW_PIPELINE.md) | 生产预览方案逆向、我们的重构、10fps/1080p 规则 |
| [docs/assets/preview_verify.txt](docs/assets/preview_verify.txt) | 预览帧率/分辨率/重复帧实测数据 |
| [docs/ALARM_VIDEO.md](docs/ALARM_VIDEO.md) | 生产 libdump4 回溯方案、告警图/视频对齐、旧 ring 问题 |
| [docs/ISC_STREAM.md](docs/ISC_STREAM.md) | 生产 ISC RTMP 协议、我们的解析重构与网络结论 |
| [docs/ALGORITHM_PLUGINS.md](docs/ALGORITHM_PLUGINS.md) | 生产算法插件 ABI、五种算法模型/阈值与集成进度 |
| [docs/ALGORITHM_VALIDATION.md](docs/ALGORITHM_VALIDATION.md) | 五算法生产告警样本验证报告 |
| [docs/NETWORK_POLICY_68_25_46.md](docs/NETWORK_POLICY_68_25_46.md) | .68/.25/.46 三机网络策略差异 |
| [docs/PRODUCTION_ALARM_LOGIC.md](docs/PRODUCTION_ALARM_LOGIC.md) | 生产蓝框→红框告警逻辑与我们的实现 |
| [docs/PRODUCTION_PARITY.md](docs/PRODUCTION_PARITY.md) | 与生产插件/编解码/预览/告警机制对齐度 |
| [docs/PROD_STREAM_MODEL_SCHEDULER.md](docs/PROD_STREAM_MODEL_SCHEDULER.md) | 生产取流/任务启停/模型调度机制 |
| [docs/DEPLOY_25.md](docs/DEPLOY_25.md) | 206.25 覆盖部署记录 |
| [docs/assets/alarm_isc_verify.txt](docs/assets/alarm_isc_verify.txt) | 告警/ISC 对齐与网络实测数据 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 看当前代码目录、数据流、SDK 布局 |
| [docs/INTERFACES.md](docs/INTERFACES.md) | 对接前端/后端、改接口、查 token/WS/配置字段 |
| [docs/SERVICE_TASKS.md](docs/SERVICE_TASKS.md) | 理解旧线程/进程、拆新模块、查旧缺陷 |
| [docs/SOLUTION.md](docs/SOLUTION.md) | 目标架构、模型调度、合流、Docker |
| [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md) | 判断"算不算做完"、看 FR/AC |
| [docs/CONVENTIONS.md](docs/CONVENTIONS.md) | 写代码前（命令、路径、端口、禁区） |
| [docs/DECISIONS.md](docs/DECISIONS.md) | 想"优化"现有设计前 |
| [docs/DOMAIN.md](docs/DOMAIN.md) | 业务术语 |
| [docs/ISC_PROXY.md](docs/ISC_PROXY.md) | ISC 网络代理与真实通道验证 |
| [docs/UI_ACCEPTANCE.md](docs/UI_ACCEPTANCE.md) | Web UI 截图与接口验收、告警图片修复 |
| [docs/NETWORK_REPORT_25_vs_68.md](docs/NETWORK_REPORT_25_vs_68.md) | .25/.68 ISC 网络差异与运维建议 |
| [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md) | 已知缺陷/MVP 限制 |

## 事实

- 技术栈：C++17、g++ 9.4.0、CMake 3.16.3、Boost 1.71（Beast/Asio）、OpenSSL 1.1、nlohmann/json。
- SDK：libsophon 0.5.3、sophon-mw 0.15.0、sophon-sail 3.11.0，位于 `env/sdk/`。
- 运行环境：AI 盒子 Ubuntu 20.04 LTS aarch64，BM1684X SoC，8 核，6.1 GiB 内存。
- 模型：反光衣 YOLOv11 FP16；DamoYOLO FP16：`safetyhelmet`、`lifejacket`、`cigarette`、`person`、`reflectivejacket_sky`。注册表在 `src/model.hpp`，输入归一化/类别/目标/告警文案见 `docs/INTERFACES.md` §5.2。
- 主要代码：`src/common.hpp`、`src/model.hpp`、`src/isc.hpp`、`src/task.hpp`、`src/server.hpp`、`src/main.cpp`；`CMakeLists.txt`；`docker/`。
- 测试证据：`docs/assets/damo_*.jpg`、`service_aggregate.jpg`（36 路 3×3）、`alarm_video_frame.jpg`、`service_video.log`、`service_load.log`、`isc_mock.log`。
- 负责人 / 验收人：项目负责人。

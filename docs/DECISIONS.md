<!-- pm:meta
iteration: 56
updated: 2026-11-12
commit: -
-->

# 决策记录（ADR）

> 唯一作用：阻止后来的 AI 把**刻意的设计**当成垃圾清理掉。
> 规则：只增不改；标题写结论；必须写代价；被推翻时新增一条并标注取代关系。

## 索引

| 编号 | 结论 | 日期 | 状态 |
|---|---|---|---|
| ADR-001 | SDK 自包含在 `env/sdk`，不改 `/opt/sophon` | 2026-11-02 | 生效 |
| ADR-002 | 在盒子上原生编译 sail，不做 x86 交叉编译 | 2026-11-02 | 生效 |
| ADR-003 | native 构建显式传 `-Dthe_libbmcv.so` | 2026-11-02 | 生效 |
| ADR-004 | 步骤 2 用 FP16 YOLOv11 做冒烟测试 | 2026-11-02 | 生效 |
| ADR-005 | 官方 demo 源码保持原样，差异放脚本 | 2026-11-02 | 生效 |
| ADR-006 | 项目记忆文档放 `docs/` | 2026-11-02 | 生效 |
| ADR-007 | 新服务用单进程 + 线程流水线 | 2026-11-02 | 生效 |
| ADR-008 | 4 个解码/合流组 ×9 路 | 2026-11-02 | 生效 |
| ADR-009 | 反光衣先用 FP16，INT8 暂缓 | 2026-11-02 | 生效 |
| ADR-010 | 控制面用 Boost.Beast + OpenSSL | 2026-11-02 | 生效 |
| ADR-011 | 任务固定映射到合流槽位 | 2026-11-02 | 生效 |
| ADR-012 | 接口契约以线上源码 + 负责人确认为准 | 2026-11-02 | 生效 |
| ADR-013 | MVP 先用一个共享反光衣 `ModelService` 串行推理 | 2026-11-02 | 生效 |
| ADR-014 | MVP 的 HTTP/WS 用线程/连接同步 Beast | 2026-11-02 | 生效 |
| ADR-015 | Docker 基于本地 `aibox-api:latest` + 拷贝 OpenSSL 1.1 | 2026-11-02 | 生效 |
| ADR-016 | DamoYOLO 后处理直接移植旧 Python 的分数/框两输出逻辑 | 2026-11-02 | 生效 |
| ADR-017 | 合流改 `bmcv` 硬件 decode/resize/copy/encode + dirty 组 | 2026-11-02 | 生效 |
| ADR-018 | ISC 用旧 HMAC/HTTPS 协议解析，证书忽略校验 | 2026-11-02 | 生效 |
| ADR-019 | 36 路压测先按 640×360/1fps 检测/2fps 预览定基线 | 2026-11-02 | 生效 |
| ADR-020 | 安装 Docker Compose v2 静态二进制到 CLI 插件目录 | 2026-11-02 | 生效 |
| ADR-021 | JPEG/JPU 操作用全局互斥串行化 | 2026-11-02 | 生效 |
| ADR-022 | 高分辨率任务默认最多 12 路，超出快速失败 | 2026-11-02 | 生效 |
| ADR-027 | 网关 ffmpeg 必须 fork/exec 并关闭继承 fd，不能用 std::system/posix_spawn | 2026-11-08 | 生效 |
| ADR-028 | 告警媒体 URL 使用 `/aibox/<save相对路径>`，不再直接用 `/data/...` | 2026-11-08 | 生效 |
| ADR-029 | 告警视频落盘后检测 HEVC 并转 H.264 | 2026-11-08 | 生效 |
| ADR-064 | 预览编码改 worker + latest-frame，不做 FIFO | 2026-11-10 | 生效 |
| ADR-065 | 合流槽位按首空顺序分配，停止只留空 | 2026-11-10 | 生效 |
| ADR-066 | 告警上报 `max(task, algo)` 硬门控 + next_report | 2026-11-10 | 生效 |
| ADR-067 | 检测框绑定推理帧时间，推理队列 latest-only | 2026-11-10 | 生效 |
| ADR-068 | 配置区域取原图；单/合流/告警图绘 ROI 绿线 | 2026-11-10 | 生效 |
| ADR-069 | 无合流槽位时 AI 返回 409，前端 36 路预检 | 2026-11-10 | 生效 |
| ADR-070 | 保存后直连 AI 触发热更新，2s 轮询兜底 | 2026-11-10 | 生效 |
| ADR-071 | 所有算法/告警统一按框中心过滤 ROI | 2026-11-10 | 生效 |
| ADR-072 | 仅曾出帧任务 2min 无帧时逐个重启，避免 ISC 会话风暴 | 2026-11-11 | 生效 |
| ADR-073 | 通道/任务状态改用 AI 实时帧健康，不信任后端在线状态 | 2026-11-11 | 生效 |
| ADR-074 | 无任务通道用按需 ffmpeg 探测，不重构常驻 MediaChannel | 2026-11-11 | 生效 |
| ADR-075 | ISC Camera not found 状态直显；UDP 缓冲 8MB 防 4K 花屏 | 2026-11-12 | 生效 |
| ADR-076 | 预览框按帧时间快照对齐；MP4 文件源循环且不上报告警 | 2026-11-12 | 生效 |
| ADR-077 | 预览框逐帧 LK 光流；确认目标保留 5s→1s 消除 ROI 残框 | 2026-11-12 | 生效 |
| ADR-078 | 预览框按检测帧绑定，不再用“最新检测 + 外推”；取消 1s 边缘框缓存 | 2026-11-12 | 生效 |
| ADR-079 | 推理改 FIFO 公平闸门 + 有界并发 + 每任务需求上限，替代全局串行锁 | 2026-11-12 | 生效 |
| ADR-080 | 每个算法标签现渲染底板；告警目标 TTL 2s；轨迹按 label 关联 | 2026-11-12 | 生效 |
| ADR-081 | 标签底板“先画后量”；30+ 任务按容量上限处理，并发保持 2 | 2026-11-13 | 生效 |
| ADR-082 | 标签按开关动态拼接、底板构建期预渲染；预览 720p；并发默认 4 | 2026-11-13 | 生效 |
| ADR-083 | 每算法独立节奏 + 每预览帧只缩放一次（对齐 requireScaled） | 2026-11-13 | 生效 |
| ADR-084 | 检测裁剪改用缩放帧；性能对比必须在任务上量完成后测量 | 2026-11-13 | 生效 |
| ADR-097 | 预览对齐生产：默认 5fps + 按源尺寸编码（取代 ADR-023 的 10fps、ADR-088 的 1080p 上限） | 2026-11-19 | 生效 |
| ADR-096 | HEVC bsf 按候选临时启用，不得跨候选粘连 | 2026-11-14 | 生效 |
| ADR-095 | 跟踪中间图/合流瓦片也池化，热路径零按帧分配 | 2026-11-14 | 生效 |
| ADR-094 | 推理输入与预览帧改为每任务池化复用（对齐生产） | 2026-11-14 | 生效 |
| ADR-093 | 失败重连按退避重试（不再立即重试），抑制 BM 缓冲增长 | 2026-11-14 | 生效 |
| ADR-092 | 看门狗阈值 dmabuf>1000/RSS>2000MB；ISC 解码失败时轮换候选 | 2026-11-14 | 生效 |
| ADR-091 | 资源看门狗：BM 缓冲/RSS 超限自重启进程 | 2026-11-13 | 生效 |
| ADR-090 | 预览只按整数倍 VPP 缩放（省 VPP/JPU），CPU 缩放仅兜底 | 2026-11-13 | 生效 |
| ADR-089 | 预览缩放：整数倍用 VPP，非整数倍用 CPU cv::resize | 2026-11-13 | 生效 |
| ADR-088 | 预览默认 1080p 上限（房规优先于生产原尺寸），A/B 数据见迭代 49 | 2026-11-13 | 生效 |
| ADR-087 | 预览按源尺寸编码（取消 1080p 上限），与生产 JpegEncoderInstance 一致 | 2026-11-13 | 生效 |
| ADR-086 | ISC 按通道 streamType 取流（不自适应）+ HEVC Annex-B 直通 | 2026-11-13 | 生效 |
| ADR-085 | ISC 预览走子码流（streamType=0）；单通道预览上限 1920×1080 | 2026-11-13 | 生效 |
| ADR-030 | 安全帽 V3 用 person 上部 35% crop + 生产双阈值，不再二次 sigmoid | 2026-11-08 | 生效 |
| ADR-031 | 算法独有参数存 `tb_algo_config`，类型/范围/选项放 `remark` JSON | 2026-11-08 | 生效 |
| ADR-032 | 无 ROI 的 `boat_life_jacket`/`people_gather` 任务先停用，预览只画目标框 | 2026-11-08 | 生效 |
| ADR-033 | 救生衣只在人形裁剪内推理，禁止全帧扫水面 | 2026-11-08 | 生效 |
| ADR-034 | 抽烟/打电话固定上半身裁剪 + 单候选人 + 分类别持续时长 | 2026-11-08 | 生效 |
| ADR-035 | V3 暂不全量替换，需按摄像头标定 | 2026-11-08 | 生效 |
| ADR-036 | V3 按生产 majorId=1 人形框顶部 70% 裁剪，全量替换 helmet | 2026-11-08 | 生效 |
| ADR-037 | behavior 按生产 5 类 + 分类阈值，不虚构 cigarette 模型 | 2026-11-08 | 生效 |
| ADR-038 | HEVC 告警视频用软件解码 + h264_bm 编码 | 2026-11-08 | 生效 |
| ADR-039 | behavior 按 YOLOv10 5 类 + people 框解析 | 2026-11-08 | 生效 |
| ADR-040 | minor152 救生衣按 major1 人形框，不接独立 boat.bmodel | 2026-11-08 | 生效 |
| ADR-041 | 办公室演示任务去掉 vest，只保留 helmet_v3 | 2026-11-09 | 生效 |
| ADR-042 | Task 增加 frame watchdog，卡流杀 gateway 重连 | 2026-11-09 | 生效 |
| ADR-043 | V3 独立阈值；告警标签板端绘制，不再依赖 Pillow | 2026-11-09 | 生效 |
| ADR-044 | 恢复 6488 vest，V3+vest 双算法并行 | 2026-11-09 | 生效 |
| ADR-045 | 卡流 watchdog 杀 gateway 后注入 UDP 包唤醒 decoder | 2026-11-09 | 生效 |
| ADR-046 | 批量脚本写 `tb_task_algo.created` 必须用 DateTime 字符串 | 2026-11-09 | 生效 |
| ADR-047 | 预览 pending 不画标签；上报间隔取任务与算法较大值 | 2026-11-09 | 生效 |
| ADR-048 | 已确认目标标签常驻；多算法标签按 slot 向上错位 | 2026-11-09 | 生效 |
| ADR-049 | 7 路重点通道优先启动，ISC 被挤后仍需重启恢复 | 2026-11-09 | 生效 |
| ADR-050 | 已确认目标保留 5s 参与预览合成，跨算法叠标签 | 2026-11-09 | 生效 |
| ADR-051 | ISC 优先使用频道 protocol=rtmp，与生产 MediaUrl 对齐 | 2026-11-09 | 生效 |
| ADR-052 | ISC previewURL 10 分钟缓存，减少重连会话泄漏 | 2026-11-09 | 生效 |
| ADR-053 | 取消 7 路优先级，所有通道同等启动；瓶颈是 ISC 会话配额 | 2026-11-09 | 生效 |
| ADR-054 | 错峰 1.2s 启动仍受 ISC 硬配额 11 路限制 | 2026-11-09 | 生效 |
| ADR-055 | 接受 ISC 配额，保持 36 路同等；直连 RTSP 可绕过配额 | 2026-11-09 | 生效 |
| ADR-056 | 告警图标签固定贴框，预览才做多标签 slot 错位 | 2026-11-09 | 生效 |
| ADR-057 | ring 片段落盘失败时回退外部 dump_in_worker 生成告警视频 | 2026-11-09 | 生效 |
| ADR-058 | 接入 threshold_for_people=0.85 与 enable_people_deep_mode | 2026-11-09 | 生效 |
| ADR-059 | pending 期间新告警先 finalize 旧片段，不再丢视频 | 2026-11-09 | 生效 |
| ADR-060 | 看门狗无帧阈值 15s→45s，避免杀告警 packet ring | 2026-11-09 | 生效 |
| ADR-061 | 通道重名由前端自动改 `_2`，避免后端 500 | 2026-11-10 | 生效 |
| ADR-062 | 预览 dets 保留 5s；标签同行贴框且不显示置信度 | 2026-11-10 | 生效 |
| ADR-063 | 标签垂直堆叠；删除中心距离兜底，第二目标重新 5s 倒数 | 2026-11-10 | 生效 |

---

### ADR-001 SDK 自包含在 `env/sdk`，不改 `/opt/sophon`

- 日期：2026-11-02（迭代 1）
- 背景：盒子原厂运行时是 libsophon 0.5.1 + sophon-mw 0.12.0，正在被旧 Python 服务使用；参考包是 0.5.3 + 0.15.0；根分区只剩约 2.8 GiB，`/data` 有 45 GiB；最终还要 Docker 化。
- 候选：A. 直接升级 `/opt/sophon`；B. 解压到 `/data/.../env/sdk` 并靠 `env.sh` 切换；C. 只在 Docker 镜像里装，盒子不装。
- 决定：选 B。SDK 全部落在 `env/sdk/`，`env.sh` 把新库放到 `LD_LIBRARY_PATH` 最前。
- 代价：每次新 shell 必须 `source env/env.sh`；二进制要带 rpath 或依赖该环境；`/opt/sophon` 与新 SDK 并存，排查问题时要确认实际加载的是哪套库。
- 影响文件：`env/env.sh`, `env/setup_sdk.sh`, `env/test_sail.sh`, `tests/yolov11_official_demo/build.sh`
- 取代：无

---

### ADR-002 在盒子上原生编译 sail，不做 x86 交叉编译

- 日期：2026-11-02（迭代 1）
- 背景：算能官方 SoC 文档默认在 x86 上交叉编译；当前工作区没有可用的 x86 Linux + 交叉工具链，盒子本身有 g++ 9.4、cmake 3.16、8 核。
- 候选：A. 搭 x86 交叉编译环境；B. 盒子原生编译（`BUILD_TYPE=pcie` 在上游就是 native 分支）；C. 用官方预编译 wheel（只有 Python，没有 C++ 库）。
- 决定：选 B。`cmake -DBUILD_TYPE=pcie` + 盒子 g++，编译产物是 aarch64 `.so`。
- 代价：占用盒子 CPU/内存数分钟；构建不可在 CI 的 x86 机器上直接复现；`BUILD_TYPE=pcie` 这个名字容易让人误以为在编 PCIe 卡版本。
- 影响文件：`env/setup_sdk.sh`, `docs/CONVENTIONS.md`
- 取代：无

---

### ADR-003 native 构建显式传 `-Dthe_libbmcv.so`

- 日期：2026-11-02（迭代 1）
- 背景：sail 的 native 分支用 `${the_libbmcv.so}` 追加 bmcv；这个变量本应由 `find_package(libsophon)` 的 config 设置。我们传了 `LIBSOPHON_BASIC_PATH` 绕过 find_package，变量为空，`libsail.so` 里留下 44 个未定义 bmcv 符号，`NEEDED` 也没有 `libbmcv.so.0`。
- 候选：A. 改 sail 源码/CMake 加 bmcv；B. 在 cmake 命令行补 `-Dthe_libbmcv.so=<libbmcv.so 绝对路径>`；C. 运行期靠可执行文件链接 bmcv 兜底。
- 决定：选 B。不改上游源码，构建参数一次说清。
- 代价：构建命令多一个不直观的变量；换 SDK 路径时要同步改；后来者如果删掉它，链接仍能过，但运行期会缺符号。
- 影响文件：`env/setup_sdk.sh`, `docs/CONVENTIONS.md`
- 取代：无

---

### ADR-004 步骤 2 用 FP16 YOLOv11 做冒烟测试

- 日期：2026-11-02（迭代 1）
- 背景：盒子上的生产候选 INT8 反光衣模型（`yolov11s_int8_1b_back_reflectivejacket_1.bmodel`）在旧 0.5.1 + Python sail 和新 0.5.3 + C++ bmrt 下都输出分类列全 0；同结构的 FP16 `yolov11s_fp16_1b_back.bmodel` 在同样预处理下能正常输出 0.89+ 的分类分数。`text_1` 项目里的 `yolov11s_int8_1b_{source,nopre,back}.bmodel` 也全部是分类列全 0，说明不是单个文件损坏。
- 候选：A. 继续死磕 INT8，环境搭建卡住；B. 用 FP16 同结构模型完成步骤 2，同时记录 INT8 问题；C. 用 DamoYOLO 模型绕开 YOLO11。
- 决定：选 B。步骤 2 的目标是验证 C++ SDK 能跑通 Bmodel，FP16 足以证明；INT8 作为已知问题单独跟踪。
- 代价：验证的性能数字（inference 约 5.2 ms/帧）不代表最终 INT8 指标；生产选型仍要解决 INT8 重新导出/量化问题；文档必须明确写"本轮验证用 FP16"，不能让人误以为 INT8 已通过。
- 影响文件：`tests/yolov11_official_demo/run.sh`, `docs/KNOWN-ISSUES.md` (KI-001), `docs/PROGRESS.md`
- 取代：无

---

### ADR-005 官方 demo 源码保持原样，差异放脚本

- 日期：2026-11-02（迭代 1）
- 背景：步骤 2 要"用算能官方模型运行 demo"；官方 `YOLOv8_plus_det/cpp/yolov8_bmcv` 的 CMakeLists 只支持原生 `/opt/sophon` 路径或 aarch64 交叉编译器，和我们的自包含 SDK 不兼容。
- 候选：A. 改官方 CMakeLists/源码适配；B. 复制源码进仓库，用自定义 `build.sh` 的 g++ 命令编译；C. 给 `/opt/sophon` 做软链让官方 CMake 直接跑。
- 决定：选 B。源码目录原样复制，不改；编译/运行/断言都在外层脚本。
- 代价：没有走官方 CMake，上游更新时要手动同步源码；编译参数（include/lib/rpath）要自己维护；后续服务端不会直接复用这个脚本。
- 影响文件：`tests/yolov11_official_demo/cpp/yolov8_bmcv/**`, `build.sh`, `run.sh`
- 取代：无

---

### ADR-006 项目记忆文档放 `docs/`

- 日期：2026-11-02（迭代 1）
- 背景：用户要求根目录放基础 `README.md`，其他 md 放 `docs/`；project-memory skill 默认用 `docs/`。
- 候选：A. 用默认 `docs/`；B. 用 `docs/` 并在 AGENTS/校验命令里显式传 `-DocsRoot docs`；C. 放 `docs/ai/`。
- 决定：选 B。6 份固定文档 + DOMAIN/KNOWN-ISSUES + AGENTS 全在根/`docs/`。
- 代价：所有 project-memory 脚本调用都要带 `-DocsRoot docs`；新 AI 若只按 skill 默认路径找 `docs/` 会找不到。
- 影响文件：`AGENTS.md`, `README.md`, `docs/**`
- 取代：无

---

### ADR-007 新服务用单进程 + 线程流水线

- 日期：2026-11-02（迭代 2）
- 背景：旧方案一任务一 Python 进程，36 路时进程/线程爆炸、模型重复加载、跨进程传图；C++ 可以在同一地址空间共享帧和模型。
- 候选：A. 继续保持一任务一进程；B. 一个 C++ 进程 + 线程流水线；C. 4 个进程（每组合流一个）。
- 决定：选 B。控制面、解码组、模型实例、合流、告警都在一个进程内，用有界队列和 `shared_ptr` 帧池连接。
- 代价：一个未捕获异常可能影响全部任务；需要严格的异常边界、看门狗和 Docker 重启策略；线程安全/生命周期管理比多进程更考验代码质量。
- 影响文件：`docs/SOLUTION.md` §3/§5/§9，未来 `src/main.cpp`、`src/task/`、`src/model/`
- 取代：无

---

### ADR-008 4 个解码/合流组 ×9 路

- 日期：2026-11-02（迭代 2）
- 背景：36 路 = 4 个合流 × 9 路；`sail::MultiDecoder` 单实例适合多通道；实测单路 1080p 解码约 241 fps，36×25fps=900fps，需要多解码器分摊。
- 候选：A. 一个 36/40 通道大池；B. 4 个 `MultiDecoder(16)`，每组 9 路；C. 每任务一个 `Decoder`。
- 决定：选 B。每组一个解码线程 + 一个合流线程，组内 9 路固定；若 M2 实测 4 个 MultiDecoder 资源冲突，再退化为 1 个 40 通道实例（不改上层接口）。
- 代价：任务必须归属组；组间负载可能不均；4 个解码实例的 VPU 资源需要实测。
- 影响文件：`docs/SOLUTION.md` §3/§6，未来 `src/video/`、`src/aggregate/`
- 取代：无

---

### ADR-009 反光衣先用 FP16，INT8 暂缓

- 日期：2026-11-02（迭代 2）
- 背景：INT8 反光衣及 `text_1` 里多个 INT8 YOLO 模型分类列全 0；FP16 新权重正常（`class_max=0.913574`，14 框/3 图，inference 5.214ms）。36 路常态 1fps 只需约 0.19 s/s TPU，FP16 可支撑；全速 burst 受限。
- 候选：A. 先解决 INT8 导出再开发；B. 先用 FP16 推进服务端；C. 反光衣改用 DamoYOLO。
- 决定：选 B。服务端按 FP16 设计；INT8 作为后续优化项，不阻塞主线。
- 代价：burst 全速能力有限（8 路 25fps 就超过单 TPU）；性能数字不是最终 INT8 指标；模型调度必须做准入控制。
- 影响文件：`docs/SOLUTION.md` §5/§11，`docs/KNOWN-ISSUES.md` KI-001
- 取代：无

---

### ADR-010 控制面用 Boost.Beast + OpenSSL

- 日期：2026-11-02（迭代 2）
- 背景：需要 HTTP + WebSocket + JWT；盒子已有 Boost 1.71 和 OpenSSL 1.1；Docker 需离线构建，尽量少新增依赖。
- 候选：A. Boost.Beast/Asio；B. Crow（单头文件）；C. Drogon；D. 自己写 socket/WS。
- 决定：选 A。Beast 提供 HTTP/WS，Asio 提供异步 I/O，OpenSSL 做 JWT HMAC；JSON 用已存在的 `nlohmann/json` 单头文件。
- 代价：Beast 样板代码多，WebSocket 分帧/背压要自己处理；开发量比 Crow 大；需要 C++17 和 Boost 链接。
- 影响文件：`docs/SOLUTION.md` §8，未来 `src/app/`、`src/auth/`
- 取代：无

---

### ADR-011 任务固定映射到合流槽位

- 日期：2026-11-02（迭代 2）
- 背景：旧聚合按 `frames` 字典插入顺序拼图，任务启停会导致 3×3 位置跳变；前端预览页期望稳定通道。
- 候选：A. 保持动态顺序；B. 按任务 hash 固定 `(group, slot)`；C. 后端下发通道序号。
- 决定：选 B。`group = hash(taskId)%4`，`slot = (hash/4)%9`，冲突线性探测；9 槽满则任务 pending；合流 id **固定 `all_1..all_4`**。
- 代价：任务集合变化时映射可能变化（同一 taskId 稳定）；hash 冲突要处理。
- 影响文件：`docs/SOLUTION.md` §6，未来 `src/task/`、`src/aggregate/`
- 取代：无

---

### ADR-012 接口契约以线上源码 + 负责人确认为准

- 日期：2026-11-02（迭代 3）
- 背景：设计文档里的 token 签发、WS 8000、`aggregate_i`、相对告警路径、`vpp_3` 等假设与线上实际不符；负责人已逐项确认。
- 候选：A. 按需求文档的理想契约；B. 按线上最新源码 `app.py + main.py + ai_package` + 负责人确认；C. 双套兼容。
- 决定：选 B，并在必要处做最小兼容：
  - token 由前端/后端签发，AI **只验证**；出站 `create_jwt("admin")`。
  - WS **9000** + `all_1..all_4`；header 同时写 `imageType`/`ImageType`。
  - 告警图片/视频 **绝对路径** `/data/CT/project/ai/inference/save/...`。
  - `/get_occupancy` 返回 `vpu_1..3` + `vpp_1..2`，**没有 `vpp_3`**。
  - `/task/start` 保留 `pid` 字段，单进程返回服务 PID。
- 代价：需求文档里的旧表述会显得过时；`docs/INTERFACES.md` 必须作为唯一接口真相源；后续改契约要同步改文档和前端。
- 影响文件：`docs/INTERFACES.md`, `docs/SOLUTION.md`, `docs/CONVENTIONS.md`, `README.md`
- 取代：无

---

### ADR-013 MVP 先用一个共享反光衣 `ModelService` 串行推理

- 日期：2026-11-02（迭代 4）
- 背景：目标是 `ModelScheduler` + 多 `ModelInstance`，按队列水位动态加载/回收；但第一步要先把 HTTP/WS/任务/RTSP/推理/合流跑通，且当前只移植了反光衣 YOLOv11 后处理。
- 候选：A. 直接实现完整调度器（多实例、队列、回收）；B. 先做一个全局 `ModelService`，`infer()` 用互斥串行；C. 每任务一个 Engine。
- 决定：选 B。`ModelService` 启动时加载一个 FP16 反光衣 Engine；所有任务共享，`infer()` 串行。等 Damo 后处理和 36 路压测时再替换为调度器。
- 代价：当前只有一个模型实例，TPU 利用率上不去；多任务串行会互相等待；`ModelService` 的 API 可能与最终调度器不同，后续要改调用点。
- 影响文件：`src/model.hpp`, `src/task.hpp`
- 取代：无（实现 ADR-007/008 的阶段性简化，不推翻目标）

---

### ADR-014 MVP 的 HTTP/WS 用线程/连接同步 Beast

- 日期：2026-11-02（迭代 4）
- 背景：目标方案是 Boost.Beast 异步 + 固定 I/O 线程；但 MVP 要快速跑通 HTTP/WS/鉴权/任务接口，异步 `async_accept`/`async_read` 的状态机容易拖慢首个可运行版本。
- 候选：A. 直接写 async Beast；B. 线程/连接同步 `http::read` + `websocket::stream`；C. 引入第三方 HTTP 库。
- 决定：选 B。accept 后每连接一个线程；WS 每条连接一个读线程 + 一个发送线程；发送队列最多 2 帧，旧帧丢弃。
- 代价：连接数多时线程多；36 路 WS 客户端会接近线程/内存上限；缺少统一超时/背压；后续要换 async 或线程池。
- 影响文件：`src/server.hpp`
- 取代：无（目标架构仍按 ADR-010）

---

### ADR-015 Docker 基于本地 `aibox-api:latest` + 拷贝 OpenSSL 1.1

- 日期：2026-11-02（迭代 4）
- 背景：盒子不能访问外网，本地没有 `ubuntu:20.04` 镜像；可用的 arm64 基础镜像只有 `aibox-api:latest`（Debian 12）和 nginx/dotnet。服务二进制在 Ubuntu 20.04 上链接了 `libcrypto.so.1.1`，Debian 12 只有 `libcrypto.so.3`。
- 候选：A. 拉取/导入 ubuntu:20.04 根文件系统；B. 用 `aibox-api:latest` + 拷贝 `libcrypto.so.1.1`/`libssl.so.1.1`；C. 静态链接 OpenSSL 后从 `scratch`/最小镜像跑。
- 决定：选 B。`docker/Dockerfile` 以 `aibox-api:latest` 为基础，`COPY env/sdk` 和 `docker/lib/libcrypto.so.1.1`、`libssl.so.1.1`，设置 `LD_LIBRARY_PATH`；`--privileged --network host` 访问设备/后端。
- 代价：镜像 901 MB；依赖 `aibox-api` 基础镜像存在；基础镜像升级可能改变 glibc/依赖；OpenSSL 1.1 是手工拷贝，需要在 Dockerfile 里保留来源说明。
- 影响文件：`docker/Dockerfile`, `docker/docker-compose.yml`, `docker/lib/*`
- 取代：无

---

### ADR-016 DamoYOLO 后处理直接移植旧 Python 的分数/框两输出逻辑

- 日期：2026-11-02（迭代 5）
- 背景：4 个 Damo 模型输出是 `[1,8400,C]` 分数 + `[1,8400,4]` 框，旧 Python `Damo_Yolo.py` 有现成的多类 NMS/反映射逻辑；重新设计容易与旧行为漂移。
- 候选：A. 只用 YOLOv11；B. 移植旧 Python 的 `multiclass_nms`/`resize_numpy_with_padding`；C. 用第三方检测库。
- 决定：选 B。C++ 按旧逻辑实现逐类阈值/NMS、`x1y1x2y2` letterbox 反映射、ROI 过滤；类名/目标/文案放 `ModelSpec` 注册表。
- 代价：C++ 与 Python 数值细节可能有微小差异；需要逐个模型实测；Damo 输出布局变更时要改代码。
- 影响文件：`src/model.hpp`, `src/task.hpp`
- 取代：无

---

### ADR-017 合流改 `bmcv` 硬件 decode/resize/copy/encode + dirty 组

- 日期：2026-11-02（迭代 5）
- 背景：旧合流每 tile `cv::imdecode`/`resize`/`imencode`，CPU 热点；sail 提供 `bmcv.imdecode`、`resize`、`image_copy_to`、`imencode`，且旧 Python `aggregate.py` 已用 `image_copy_to`。
- 候选：A. 继续 CPU JPEG 拼图；B. 硬件解码/缩放/拷贝/编码，仅 dirty 组处理；C. 直接由 Task 推送 BMImage（共享设备内存，线程/句柄复杂）。
- 决定：选 B。Aggregator 每组的 `sail::Bmcv` 做硬件链路；`image_copy_to` 前统一格式；`Group::dirty` 避免 4 个空组持续占用 JPU。
- 代价：JPU 仍被 Task 预览编码和聚合解码/编码共享；强杀进程可能锁死 JPU（需重启）；`image_copy_to` 要求格式/尺寸一致，必须显式转换。
- 影响文件：`src/server.hpp`
- 取代：无

---

### ADR-018 ISC 用旧 HMAC/HTTPS 协议解析，证书忽略校验

- 日期：2026-11-02（迭代 5）
- 背景：线上除一个 RTSP 外都是 `isc://`；旧 `video_analysis.py` 的 ISC 流程是 HMAC-SHA256 签名 + HTTPS `/artemis/api/video/v2/cameras/previewURLs` + `data.url`。盒子到真实 `10.10.69.250:443` 连接被拒绝。
- 候选：A. 不支持 ISC；B. 按旧协议实现，证书 `verify_none`；C. 等网络/VPN 再实现。
- 决定：选 B。`IscResolver` 复刻签名字符串/headers/body，HTTPS 不校验证书（与旧 `verify=False` 一致）；用本地 HTTPS mock 验证签名与解析，真实网络问题单列。
- 代价：忽略证书有 MITM 风险（内网/旧系统兼容）；真实 ISC 未联通前只能证明协议/签名/mock，不证明生产可用。
- 影响文件：`src/isc.hpp`, `src/task.hpp`
- 取代：无

---

### ADR-019 36 路压测先按 640×360/1fps 检测/2fps 预览定基线

- 日期：2026-11-02（迭代 5）
- 背景：36 路 1080p/10fps 需要 360 路 1080p 预览编码 + 36 路解码，6 GiB/单 TPU 不现实；需要先证明 36 任务生命周期/调度/合流稳定，再逐级升规格。
- 候选：A. 直接 36×1080p/10fps（很可能 OOM/JPU 锁死）；B. 640×360、1fps 检测、2fps 预览做基线；C. 只跑 4 路高规格。
- 决定：选 B。36 个任务 30 秒稳定，RSS 375 MB、CPU 331%、TPU 24%、2 个模型实例；4 路 `all_i` 均能输出。高规格容量单列待测。
- 代价：不能把该结果当作 36 路 1080p/10fps 验收；需要下一轮准入控制/规格测试。
- 影响文件：`docs/PROGRESS.md`, `docs/REQUIREMENTS.md`
- 取代：无

---

### ADR-020 安装 Docker Compose v2 静态二进制到 CLI 插件目录

- 日期：2026-11-02（迭代 5）
- 背景：盒子 Docker 20.10 无 compose 插件；apt 源证书/镜像包校验失败，无法在线安装；官方 Compose v2 是静态 Go aarch64 二进制，可从工作站下载后上传。
- 候选：A. 继续 `docker run`；B. 下载 Compose v2.29.7 静态二进制，装到 `/usr/local/bin/docker-compose` 和 `/usr/local/lib/docker/cli-plugins/docker-compose`；C. 自己写 compose 兼容脚本。
- 决定：选 B。`docker compose version` 返回 v2.29.7；`config/up/ps` 和任务启动已实测。
- 代价：Compose 二进制 62 MB；系统级安装；`docker-compose.yml` 仍需 `AIDETECT_JWT_SECRET` 环境变量。
- 影响文件：`docker/docker-compose.yml`, `README.md`, `docs/CONVENTIONS.md`
- 取代：无

---
### ADR-021 JPEG/JPU 操作用全局互斥串行化

- 日期：2026-11-02（迭代 5）
- 背景：36 个 1080p 任务同时首帧预览时，多个 `cv::imencode` 与 Aggregator 的 `bmcv.imdecode/imencode` 并发进入 JPU，曾出现 segfault/驱动锁死；JPU 是共享单引擎。
- 决定：全局 `jpeg_hw_mutex()` 串行化 Task `cv::imencode` 与 Aggregator 的 JPEG decode/encode；`common.hpp` 提供 static mutex，resize/copy 仍在锁外。
- 代价：JPEG 吞吐被串行化，36 路 10fps 预览会被限速；但比驱动崩溃好。36×1080 在停止阶段仍会崩，说明这是**容量/驱动上限**，不是并发锁能完全解决。

---

### ADR-022 高分辨率任务默认最多 12 路，超出快速失败

- 日期：2026-11-02（迭代 5）
- 背景：36×1080 在启动/停止阶段多次导致 VPU/JPU 驱动崩溃；12×1080 实测可稳定启动/停止。硬上 36 路 1080 会把整个服务带崩。
- 决定：`HighResGate` 对 `frame_h>=1000` 的 Task 限流，`AIDETECT_MAX_HIGHRES_TASKS` 默认 12；超出任务状态 `error`，错误文案 `high-resolution task capacity reached`；低分辨率 36 路不受限。
- 代价：36 路 1080p 不能全部同时跑；需要后续 4 组 MultiDecoder/准入策略才能提高。36×640 仍是全量 36 路通过。

---

### ADR-023 实时预览改为"按需 + 硬件缩放 + 真内容 10fps"

- 日期：2026-11-05（迭代 6）
- 背景：原实现每个任务无论有没有观看者，都按固定 1920×1080 每 100ms 重新编码；30 路时
  JPU/CPU 被"无人看"的编码占满，真正被观看的通道掉到 2~5fps，且合流发送的是重复帧。
  生产盒子 `10.10.207.46` 在同样硬件上可稳定预览，逆向证据见 `docs/PREVIEW_PIPELINE.md`：
  `DecodeInstance::requireScaled/revokeScaled`（按需缩放）、每任务 `JpegEncoderInstance`、
  `MergeInstance` 合流 JPEG 输出。
- 候选：
  A. 继续提高 JPEG 并发/降分辨率；
  B. 照生产改成按需 + 硬件缩放/编码 + 内容驱动的 10fps 门控；
  C. 预览改 WebRTC/H.264（前端不支持）。
- 决定：选 B。`StreamHub` 统计 taskId/`all_N` 订阅需求；Task 仅在有人观看时生成单通道
  1080p 预览或 640×360 合流瓦片，无人观看时只保留 1fps area 图；`preview_target` 只缩
  不放；`preview_due` 令牌桶输出真 10fps（≤10fps 源不抽不补）；Aggregator 仅在有订阅
  者时用 VPP+JPU 硬件拼图/编码，按内容更新发送。
- 代价：无观看者时 `latest_jpeg` 只有 1fps（area 取图可接受）；预览质量暂无档位；
  multi 解码组在大量失败源下的隔离仍需单独修复（KI-020）。
- 影响文件：`src/server.hpp`, `src/task.hpp`, `docs/PREVIEW_PIPELINE.md`,
  `docs/assets/preview_verify.txt`
- 取代：ADR-021 仍保留（JPU 全局串行），但 36 路"每路都编码"的假设不再成立。
---

### ADR-024 告警视频改为"同一路 decoder 的 dump(5,5)"，按触发时刻对齐

- 日期：2026-11-05（迭代 7）
- 背景：旧 ring 实现按文件 mtime 取"最新 5 个 2s 分片"，RTSP 分片落盘滞后且
  `-reset_timestamps` 丢失 PTS，实测告警图片 OSD 18:57:24、视频 18:57:10–20（视频早于
  图片 4s 结束）；异步 `dump()` 又被立即 finalize，导致dump 文件被判 missing。
  生产用 libdump4 FrameBuffering + ratio 0.5（前 5s/后 5s），告警图片位于视频中间。
- 决定：单解码模式只用 `sail::Decoder::dump(5, post)`，与预览/检测共用一条网络连接；
  触发时刻在渲染告警图片之前记录；dump 异步，继续读 post 秒后再 finalize；
  多解码模式保留时间戳包环/worker 回退。
- 代价：单模式每条 ISC 只允许一条流，和生产的"一路媒体源"一致；multi 模式仍需两条
  （包环），后续要按生产式单连接重构。
- 影响文件：`src/task.hpp`, `src/alarm_recorder.hpp`, `docs/ALARM_VIDEO.md`
- 取代：迭代 6 的"环形 ffmpeg 分片优先"方案。

---

### ADR-025 ISC 解析改为按后端 URL 的 protocol/transcode 请求 Artemis

- 日期：2026-11-05（迭代 7）
- 背景：生产 DB URL 是 `protocol=rtmp&transcode=1&videotype=h264&https=1`，代码按
  `"protocol":"%s"` 转发；我们写死 `protocol=rtsp`，且忽略 transcode/videotype/https，
  连接成功率和出图率低于生产。206.68 到 ISC 的 443/554/1935 又被 ACL RST。
- 决定：解析并转发全部参数；`AIDETECT_ISC_PROTOCOL` 可强制协议；
  `AIDETECT_ISC_RTMP_PROXY` 支持 1935 转发；`AIDETECT_ISC_ALLOW_DIRECT_RTMP` 允许
  直连网络；断流重连重新 resolve；单模式不再起第二个 ffmpeg 录制连接。
- 代价：206.68 当前仍需 `.25` 的 443/554 转发；切 RTMP 需要网络放行 1935 或 1935 转发。
- 影响文件：`src/isc.hpp`, `src/task.hpp`, `docs/ISC_STREAM.md`
- 取代：迭代 5 的"只请求 rtsp、只重写 RTSP host"方案。
---

### ADR-026 告警视频最终采用"单媒体源网关 + 时间戳包环"，取代 ADR-024 的 dump 方案

- 日期：2026-11-05（迭代 7）
- 背景：ADR-024 的单路 `decoder.dump(5,5)` 在单任务下对齐，但在 30 路 + 2560×1440
  部署中触发 `free(): invalid pointer`（libavutil `av_buffer_unref`）导致容器重启、
  任务全丢；旧 ring 的两条连接又有约 2s 时钟偏移。
- 决定：每个单模式任务启动一个 ffmpeg 网关，只拉一条上游 RTSP/ISC，`-c copy` 复制为
  两路本地 MPEG-TS/UDP；`sail::Decoder` 读一路，`AlarmRingRecorder` 读另一路，告警
  取前后 5s。两路来自同一 ffmpeg 解复用/PTS，无连接间偏移；上游只占一个会话。
  包环写完后直接 rename 成 MP4，不再 ffmpeg `-c copy` 二次封装。
- 代价：每任务多一个 ffmpeg 进程（copy 模式，CPU 低）；端口按 (group,slot) 分配；
  网关断流时需重启 ffmpeg 并重连 decoder（已在重连逻辑处理）。
- 影响文件：`src/task.hpp`, `src/alarm_recorder.hpp`, `docs/ALARM_VIDEO.md`
- 取代：ADR-024（dump 方案）；ADR-024 保留为历史记录，不再作为当前实现。

---

### ADR-027 网关 ffmpeg 必须 fork/exec 并关闭继承 fd，不能用 `std::system`/posix_spawn

- 日期：2026-11-08（迭代 15）
- 背景：30 路任务只能打开 6~8 路；日志里 decoder 反复
  `bind failed: Address already in use`。排查发现后启动的 ffmpeg 进程继承了 AI
  进程的 UDP socket，一直占着前一个任务的 decoder 端口。`std::system` 通过 shell
  启动后 ffmpeg 还会被 init 收养，任务停止时无法可靠回收；`posix_spawn` 的
  `POSIX_SPAWN_CLOEXEC_DEFAULT` 在该 glibc/文件动作组合下也未生效。
- 候选：A. 保留 std::system，靠 pkill；B. posix_spawn + CLOEXEC；C. fork 后子进程
  显式关闭 fd 3..1023 再 exec。
- 决定：选 C。子进程 `setsid`、重定向日志、循环 `close(fd)` 后 `execvp("ffmpeg")`；
  父进程记录 pid，stop 时 `kill(-pid)` 并 `waitpid`；UDP 端口用进程内唯一分配，
  启动时清理匹配 `ffmpeg .*udp://127.0.0.1:22` 的残留网关。
- 代价：fork 在 multithread 进程里调用，子进程只能做 async-signal-safe 操作；
  启动清理会短暂终止所有旧网关（AI 重启时正是期望行为）。
- 影响文件：`src/task.hpp`, `src/main.cpp`
- 取代：迭代 7 的 `std::system(ffmpeg ... &)` 网关启动方式。

---

### ADR-028 告警图片/视频 URL 使用 dotnet 静态目录 `/aibox/<save相对路径>`

- 日期：2026-11-08（迭代 15）
- 背景：AI 返回 `/data/CT/project/ai/inference/save/...`，nginx 的 SPA fallback
  把它当路由返回 `index.html`（HTTP 200 text/html），所以告警图片/视频全部
  “加载失败”。dotnet API 容器已把 `/data/CT/project/ai/inference/save` 挂到
  `/app/wwwroot`，静态文件中间件可用 `/aibox/<相对路径>` 访问。
- 候选：A. 给 nginx 加 `/data` 挂载 + alias；B. 前端把 save 前缀映射到
  `VITE_BASE_API`；C. 改 AI/后端返回绝对 URL。
- 决定：选 B（前端 `media.ts`），列表和详情共用；不依赖 nginx 容器新增挂载。
  历史数据仍是 `/data/...`，前端统一转换。
- 代价：媒体 URL 依赖 `VITE_BASE_API=/aibox/`；若以后换 nginx 路径需同步改
  `media.ts`。
- 影响文件：`web/src/views/bi/alarm-preview/media.ts`,
  `web/src/views/bi/alarm-preview/Index.vue`,
  `web/src/views/bi/alarm-preview/components/TableModal.vue`
- 取代：迭代 13 的“直接使用 `/data/...`”判断。

---

### ADR-029 告警视频在落盘后检测 HEVC 并转 H.264

- 日期：2026-11-08（迭代 15）
- 背景：包环 `-c copy` 保留相机编码；HEVC 通道的 MP4 在 Chrome/Edge 的
  `<video>` 里报 `MEDIA_ELEMENT_ERROR: Format error`。板载 `h264_bm` 需要
  `-is_dma_buffer 0` 才能编码普通内存帧。
- 候选：A. 浏览器端用 WASM HEVC 解码；B. 网关实时转码；C. 告警落盘后仅对 HEVC
  片段按需转 H.264。
- 决定：选 C。扫 MP4 的 `hvcC/hev1/hvc1`（moov 可能在文件尾，需全文件扫），
  命中则 `ffmpeg -c:v h264_bm -is_dma_buffer 0 -pix_fmt nv12` 重编码并替换。
- 代价：每个 HEVC 告警多约 6 秒板载编码，告警上报会延后；历史告警未批量转码。
- 影响文件：`src/task.hpp`
- 取代：无

---

### ADR-030 安全帽 V3 用 person 上部 35% crop + 生产双阈值，不再二次 sigmoid

- 日期：2026-11-08（迭代 15）
- 背景：V3 模型输入 256×256，直接跑 4K 全帧时人头太小、误检多；原实现又对
  已经是概率的 class 输出再做一次 sigmoid，且只用一个 `attr_thresh`。
- 决定：只对 person base 的上部 35%（宽 1.05×）crop 推理；class0=安全帽、
  class1=未戴安全帽、class2=人；`helmet_det_threshold` 判有帽并抑制、
  `no_helmet_det_threshold` 判无帽；`helmet_v3_keep_no_sec` 控制持续时长；
  `helmet_v3_repeat_sec=0` 同一目标只报一次；不再二次 sigmoid。
- 代价：person 检测漏人时 V3 也漏；上部 35% 对坐姿/低头目标需要继续用现场
  视频调参；参考视频仍可能有少量误报。
- 影响文件：`src/task.hpp`, `src/common.hpp`
- 取代：迭代 13 的 V3“全帧 + 单阈值 + sigmoid”实现。

---

### ADR-031 算法独有参数存 `tb_algo_config`，类型/范围/选项放 `remark` JSON

- 日期：2026-11-08（迭代 15）
- 背景：生产 Web 每种算法有不同参数（安全帽 V3 4 项、抽烟 11 项、反光衣 4 项、
  人员聚集 2 项等），我们原来每种算法只有 conf/nms/exceed 三项；且 dotnet
  `JsonSerializer.Deserialize` 区分大小写，任务 `value_config` 存 camelCase 时
  反序列化成 null，AI 拉不到参数。
- 决定：从生产 `alg_ability_fetch` 提取 name/code/default/type 写入
  `tb_algo_config`；数值范围/选项以 JSON 存 `remark`；`value_type` 用
  `input-number|switch|select`；前端 `AddOrEdit` 解析 `remark` 渲染，并把
  任务已保存值与默认值按 code 合并；任务 `value_config` 统一存 PascalCase。
- 代价：`tb_algo_config` 行数增加；`remark` 是轻量 schema，不是强类型；新增
  算法时要同步维护参数表。
- 影响文件：`web/src/views/bi/task-management/AddOrEdit.vue`,
  `web/src/views/bi/other-config/Index.vue`, `web/src/services/api/typings.d.ts`,
  `src/common.hpp`, 206.25 `data.db`
- 取代：迭代 8 的“每个算法固定 3 个参数”方案。

---

### ADR-032 无 ROI 的 `boat_life_jacket`/`people_gather` 先停用，预览只画目标框

- 日期：2026-11-08（迭代 16）
- 背景：14K/25K 河道画面被红/蓝框铺满，单帧 70~90 个框；`boat_life_jacket` 与
  `people_gather` 在没有生产 ROI 的情况下把水面纹理误检成人员/救生衣。预览渲染
  还会把所有非目标检测画成绿框，进一步放大噪音。
- 候选：A. 继续调低/调高置信度；B. 复制生产 ROI 后保留算法；C. 先停用这两个算法，
  等 ROI 配置好再启用；D. 只改前端隐藏框。
- 决定：选 C + 渲染过滤。14K/25K/28K 移除 `boat_life_jacket`/`people_gather`，
  八所中/北移除 `boat_life_jacket`；`render_jpeg`/`render_preview_fast`/
  `render_tile_fast` 只画 `target || pending || confirmed`。算法仍保留在
  `tb_algo_mode`，配好 ROI 后可重新勾选。
- 代价：这五个任务暂时没有救生衣/人员聚集告警；生产同款能力需要后续把
  `RuleProperty` ROI 迁到 `tb_task_area` 再启用。
- 影响文件：`src/task.hpp`, 206.25 `tb_task_algo`/`tb_alarm`
- 取代：迭代 15 的“保留 boat/people_gather 全帧检测”配置。

---

### ADR-033 救生衣只在人形裁剪内推理

- 日期：2026-11-08（迭代 17）
- 背景：`boat_life_jacket.bmodel` 全帧推理时把水面纹理分成 70+ 个“未穿救生衣”；
  生产 `MinorBoatLifeJacket` 的 `boxesCropPolicy` 走 `MinorContainer`，是在人/船
  裁剪内运行。
- 决定：新增 `ModelType::BoatLifeJacket`，只消费 `people` 人形框；人形框外扩
  10% 后跑 256×256；class3 最大概率必须 ≥ `safety_jacket_threshold` 且大于
  三个“救生衣”类最大概率才上报。
- 验证：`未穿救生衣_2.mp4` 输出 1 框标签“未穿救生衣”；14K/25K 空水面 0 框。
- 影响文件：`src/model.hpp`, `src/task.hpp`, 206.25 `tb_task_algo`
- 取代：ADR-032 中“移除救生衣算法”的临时做法。

### ADR-034 抽烟/打电话固定上半身裁剪 + 单候选人

- 日期：2026-11-08（迭代 17）
- 背景：原实现只在小头时才扩展，近景吸烟者只截到头，模型把背景人员判成抽烟；
  且每个 head 都上报，容易多框。
- 决定：head 框始终扩成 3×宽、2.5×高；阈值取 `people_ciga_threshold`；
  吸烟类持续 1s、打电话/玩手机按生产 10s；多个候选人只保留
  `score×sqrt(area)` 最大的一个，告警框用扩展后的人体框。
- 验证：4K 参考视频能出“抽烟/打电话”告警图；背景候选人仍可能出现，需后续
  `cigarette/hand/mouth` 特征二次确认。
- 影响文件：`src/task.hpp`, `src/common.hpp`
- 取代：迭代 15 的“按 head 原框上报、最多 3 类各自计时”。

### ADR-035 V3 暂不全量替换安全帽算法

- 日期：2026-11-08（迭代 17）
- 背景：V3 之前类别顺序反了（class0=未戴，class1=安全帽），且用 people 框在
  4K 上检测不到小头。修正后改用 head 供框，4K 可出候选。
- 标定结果：戴黄色安全帽的工人 class0=0.78~1.0（误判未戴）；真正未戴的工人
  class0=0.60，低于 0.7 阈值；阈值无法分开。
- 决定：不把 30 个任务全量切到 V3，继续用 `helmet`；V3 保留为可选算法，
  需按摄像头重新标定裁剪/阈值。
- 影响文件：`src/model.hpp`, `src/task.hpp`
- 取代：迭代 15 的“V3 已优化、可替换”判断。

---

### ADR-036 V3 全量替换：按生产 majorId=1 + 人形框顶部 70%

- 日期：2026-11-08（迭代 18）
- 背景：生产 `BaseAlgItem` 的 `minorId=195` 是 `majorId=1`（PeopleMethod），
  不是 head；生产样本裁剪实测 class0=未戴 0.833/0.879、class1≈0.003。
- 决定：V3 用 `people` 基座、人形框顶部 70%（宽 1.05×）裁剪；class0=未戴、
  class1=安全帽、class2=人员；阈值 `helmet_det_threshold=0.5`、
  `no_helmet_det_threshold=0.7`；30 个 `helmet` 任务全部替换为 `helmet_v3`。
- 影响文件：`src/model.hpp`, `src/task.hpp`, 206.25 `tb_task_algo`
- 取代：ADR-035 的“暂不全量替换”。

### ADR-037 behavior 按生产 5 类 + 分类阈值，不虚构 cigarette 模型

- 日期：2026-11-08（迭代 18）
- 背景：`libalg_plugin_behavior.so` `classes_num=5`，描述“抽烟打电话玩手机”；
  生产没有独立 cigarette/hand/mouth bmodel，`feature_cigarette.db` 无表。
- 决定：behavior 按生产 **YOLOv10 4 box + 5 class** 解析（旧 YOLOv5
  objectness 解析会整体错位）；裁剪用生产 majorId=1 的 people 人形框，
  不再用 head；按 `people_ciga_threshold`、`people_call_threshold`、
  `people_phone_threshold` 分别判定 class0/1/2；吸烟 1s、打电话/玩手机 10s；
  仍只保留最可信候选人。
- 影响文件：`src/model.hpp`, `src/common.hpp`, `src/task.hpp`
- 说明：背景误选仍未根除，生产实际还依赖 `PeopleAttributeMethod` 的
  几何/特征逻辑，当前无对应可提取模型。

### ADR-038 HEVC 告警视频用软件解码 + h264_bm 编码

- 日期：2026-11-08（迭代 18）
- 背景：30 路 decoder 占满 VPU，`ffmpeg` 自动选硬件 HEVC 解码时报
  `VPU_DecIssueSeqInit`/`vpu_EncInit`，转码输出为空，`video_path` 为空。
- 决定：`ensure_browser_mp4` 固定 `-c:v hevc` 软件解码，再用 `h264_bm`
  编码；`finalize_video` 对 ring 输出文件重试 3s，避免 race。
- 验证：2560×1440 HEVC 片段容器内实测 rc=0、输出 751KB。

---

### ADR-039 behavior 按 YOLOv10 5 类 + people 框解析

- 日期：2026-11-08（迭代 19）
- 背景：`behavior.bmodel` 输出 `1×1344×9`，是 4 box + 5 class，没有
  objectness；旧 YOLOv5 解析把 class0 当 obj，类别整体错位。生产
  `minorId=4` 父级为 `majorId=1`，使用 people 框。
- 决定：新增 `postprocess_behavior`；behavior 只吃 people 框；按
  `people_ciga_threshold`/`people_call_threshold`/`people_phone_threshold`
  分类判定；不再用 head 框。
- 验证：4K 参考视频 class0=0.539~1.0 命中近景吸烟者。
- 影响文件：`src/task.hpp`
- 说明：生产没有独立 cigarette/hand/mouth bmodel，`feature_cigarette.db`
  为空库，无法再接入额外特征模型。

### ADR-040 minor152 救生衣按 major1 人形框，不接独立 boat.bmodel

- 日期：2026-11-08（迭代 19）
- 背景：生产 `BaseAlgItem` 中 `minorId=152` 的父级是 `majorId=1`；
  `MinorBoatLifeJacket` 走 `MinorContainer::boxesCropPolicy`，即人形框。
  独立 `libalg_plugin_boat.so`/`boat.bmodel` 是“船舶检测”算法，不是
  minor152 的父模型。
- 决定：保持 people 框外扩 10% → `boat_life_jacket` → class3 阈值；
  不接 `boat.bmodel` 船框，避免把岸边人员漏掉。
- 验证：生产 BoatNoLifeJacket 告警样本是岸边人员；本地
  `未穿救生衣_2.mp4` 出 1 框。
- 影响文件：`src/task.hpp`

### ADR-041 办公室演示任务去掉 vest，只保留 helmet_v3

- 日期：2026-11-09（迭代 20）
- 背景：`数智技术中心_test`（6488bb6c）是办公室画面，却同时挂了
  `helmet_v3` 和 `vest`；办公室人员不穿反光衣，`vest` 必然把 3 个人
  标成“未穿反光衣”。
- 决定：删除该任务的 `vest`，保留 `helmet_v3`；V3 的 no/helmet 同时
  命中时按现有规则跳过，不再出红框。
- 影响：该任务不再有反光衣告警；若以后要演示反光衣，需要换到工地画面
  或单独建任务。
- 影响文件：206.25 `tb_task_algo`

### ADR-042 Task 增加 frame watchdog，卡流杀 gateway 重连

- 日期：2026-11-09（迭代 21）
- 背景：部分通道 `decoder opened` 后 `sail::Decoder::read()` 永久阻塞，
  没有新帧也没有错误，预览停在最后一帧；另一些通道 gateway 已死但一直
  “正在连接”。
- 决定：Task status 线程加看门狗：
  - 已有帧但 15s 无新帧 → `stop_gateway()`，让 read 返回并走重连；
  - 无首帧 60s → 杀 gateway 重试；
  - gateway 启动时重置 `started_ms_`，任务重启重置 `ever_frame_`；
  - 两次强杀间隔 30s，避免抖动。
- 验证：泉惠排水渠1/凤凰咀博物馆/乙烯南路2/龙港/凤凰咀游客均恢复
  `decoder opened`，连续两帧 SHA256 不同。
- 影响文件：`src/task.hpp`

### ADR-043 V3 独立阈值；告警标签板端绘制

- 日期：2026-11-09（迭代 21）
- 背景：V3 之前“helmet 类也高就跳过”，办公室白衬衫人员
  `no=0.809/helmet=1.0` 被判为已戴；告警图标签依赖 Pillow，worker 不在时
  只有红框没有标签。
- 决定：V3 只看 `no_helmet_det_threshold`，独立阈值；`render_jpeg` 不再
  `continue` 等 Pillow，直接在板端用 FreeType/预渲染牌画标签。
- 验证：办公室告警图显示“未戴安全帽 0.94”；14K/25K 救生衣图标签正常。
- 影响文件：`src/task.hpp`

### ADR-044 恢复 6488 vest，V3+vest 双算法并行

- 日期：2026-11-09（迭代 22）
- 背景：ADR-041 为避免办公室误报删除了 6488 的 `vest`；用户明确要求
  “未戴安全帽”和“未穿反光衣”同时检测。
- 决定：恢复 `vest`，保留 `helmet_v3`；V3 独立阈值已能正确出
  “未戴安全帽”，vest 继续出“未穿反光衣”。
- 验证：09:44:49 helmet_v3 告警 `未戴安全帽 0.99`；09:45:16 vest 告警
  `未穿反光衣 0.98`，两条图/视频均落库。
- 影响：6488 恢复双算法告警；取代 ADR-041。
- 影响文件：206.25 `tb_task_algo`

### ADR-045 卡流 watchdog 杀 gateway 后注入 UDP 包唤醒 decoder

- 日期：2026-11-09（迭代 22）
- 背景：`Decoder::read()` 在空 UDP socket 上没有超时；只杀 gateway 不会
  让已经阻塞的 `read()` 返回，看门狗仍无法触发重连。
- 决定：看门狗 `stop_gateway()` 后调用 `wake_decoder_udp()`，向
  `port_dec_` 发 6 个 188 字节垃圾 TS 包，强制 recv 返回，让 decode loop
  进入 fail_count 重连路径。
- 验证：乙烯南路2/凤凰咀博物馆等卡流通道恢复 decoder opened 与连续帧。
- 影响文件：`src/task.hpp`

### ADR-046 批量脚本写 `tb_task_algo.created` 必须用 DateTime 字符串

- 日期：2026-11-09（迭代 23）
- 背景：为恢复救生衣/反光衣插入的 6 条 `tb_task_algo` 记录把
  `created` 写成 `str(time.time())`（Unix 浮点数）。任务编辑点“更新”时
  API 反序列化 `DateTime` 失败，返回 500“服务器错误，请重试”。
- 决定：所有直接写 `tb_task_algo` 的脚本必须写
  `YYYY-MM-DD HH:MM:SS.ffffff`；已有 6 条历史脏数据已修正。
- 验证：6488 与 14K 的 UpdateTask 均返回
  `{"code":200,"message":"调用成功"}`。
- 影响文件：206.25 `tb_task_algo`，`.work/dev/*.sh` 批量脚本

### ADR-047 预览 pending 不画标签；上报间隔取任务与算法较大值

- 日期：2026-11-09（迭代 23）
- 背景：蓝色 pending 框上沿提前出现告警标签；vest 参数
  `helmet_3cls_no_vest_alarm_sec=30` 覆盖了任务“告警上报间隔 3 分钟”，
  同一场景 1 分钟内报了 3 次。
- 决定：`render_jpeg` 在 pending 倒计时后直接跳过标签；`update_alarm`
  的上报间隔取 `max(task cooldown, algorithm repeat)`；间隔内只抑制新上传，
  预览仍保留红框/标签，避免出现裸红框。
- 验证：6488 在 10:42:33 后 3 分钟内无第二条 vest；10:43:03 V3 正常；
  pending 代码路径只画 `3s/2s/1s`。
- 影响文件：`src/task.hpp`

### ADR-048 已确认目标标签常驻；多算法标签按 slot 向上错位

- 日期：2026-11-09（迭代 23）
- 背景：为抑制 3 分钟内的重复标签曾设置 `suppress_label`，导致预览出现
  裸红框；用户要求平时也显示标签，只在多算法同一目标时处理叠加。
- 决定：已确认目标的标签一直显示；`render_jpeg`/`render_preview_fast`
  用递增 `label_slot` 把同帧多个标签向上错位，避免互相覆盖；上传间隔
  只控制是否产生新告警，不再隐藏标签。
- 验证：11:19 办公室预览同时显示“未穿戴救生衣 1.00 / 未穿反光衣 0.96”，
  V3 为蓝色 5s pending，确认后第三条出现。
- 影响文件：`src/task.hpp`

### ADR-049 7 路重点通道优先启动，ISC 被挤后仍需重启恢复

- 日期：2026-11-09（迭代 23）
- 背景：30 个 task 同时抢 ISC 预览会话，7 路重点通道经常被其它任务挤掉；
  本地 DecodePool 有 36 路，不是 decoder 数量问题。
- 决定：`is_priority` 扩展为 14K/25K/28K/八所/博物馆/游客/乙烯南2/
  泉惠排水渠1/龙港，reconcile 时优先启动；看门狗继续负责卡流重连。
- 限制：ISC 返回 500 后任务会 0 decoder，watchdog 只能重试，不能保证
  永久抢回；要永久 7 路需降低/停非重点任务并发。
- 验证：重启后 7 路均取到帧；博物馆仍需单独重启一次恢复。
- 影响文件：`src/server.hpp`

### ADR-050 已确认目标保留 5s 参与预览合成，跨算法叠标签

- 日期：2026-11-09（迭代 23）
- 背景：三个算法在不同帧确认，单帧 `dets_` 往往只包含一个算法的目标，
  预览只能看到一条标签；用户要求多算法标签同时显示。
- 决定：`ModelRuntime` 增加 `active`/`active_ms`，保存最近 5s 的已确认
  目标；`detect()` 末尾按 class/label+IoU 去重后并入当前帧。标签继续用
  `label_slot` 向上错位。
- 限制：若 ISC 流花屏/卡死，合成帧仍会花屏，这是取流问题，不是标签逻辑。
- 影响文件：`src/task.hpp`

### ADR-051 ISC 优先使用频道 protocol=rtmp，与生产 MediaUrl 对齐

- 日期：2026-11-09（迭代 23）
- 背景：生产告警 MediaUrl 是 `protocol=rtmp&https=1&videotype=h264&transcode=1`；
  容器环境 `AIDETECT_ISC_PROTOCOL=rtsp` 强制我们先用 RTSP，很多相机
  `DESCRIBE 500`/`Invalid data`。
- 决定：`start_isc_gateway` 优先读取频道地址中的 `protocol=rtmp`；环境变量
  只在地址没有 protocol 时生效。候选仍保留 RTSP/streamType 0/1 回退。
- 验证：7 路复验 6 路有帧（14K/25K/28K/博物馆/游客/龙港）；泉惠排水渠1
  仍偶发 RTSP 2560 打开后无帧，属 ISC/相机流问题。
- 影响文件：`src/task.hpp`

### ADR-052 ISC previewURL 10 分钟缓存，减少重连会话泄漏

- 日期：2026-11-09（迭代 23）
- 背景：每个 task 重连都重新请求 ISC previewURL，30 个 task 反复刷新会
  耗尽 ISC 预览会话，出现 `DESCRIBE 500`。
- 决定：`IscResolver::resolve` 按 `isc_url|protocol|streamType` 缓存成功
  URL 10 分钟，重连复用同一 URL；失败仍走候选回退。
- 验证：缓存后仍只有 11 个 decoder，说明初始并发 30 已超过 ISC 配额，
  缓存只能减少重连泄漏。

### ADR-053 取消 7 路优先级，所有通道同等启动；瓶颈是 ISC 会话配额

- 日期：2026-11-09（迭代 23）
- 背景：用户要求 36 路同等重要，不应只保 7 路；生产盒子 24 个 task
  全部有画面，而 206.25 30 个 task 只有 11 个 decoder。
- 决定：移除 `is_priority` 特判，所有通道按 DB 顺序启动；不再人为偏好
  7 路。本地 DecodePool 仍为 36 路。
- 结论：11 个 decoder + 31 次 ISC 500 证明瓶颈是 ISC 侧并发预览会话
  配额（约 11），不是本地 decoder 数量或优先级。要 36 路需 ISC 提升
  quota、改用直连 RTSP，或生产侧不同的取流账号/白名单。
- 影响文件：`src/server.hpp`, `src/isc.hpp`

### ADR-054 错峰 1.2s 启动仍受 ISC 硬配额 11 路限制

- 日期：2026-11-09（迭代 23）
- 背景：怀疑 30 task 同时请求 previewURL 触发 ISC 突发限流，改为每 1.2s
  启动一个。
- 结果：仍只有 11 个 decoder、64 次 `DESCRIBE 500`，错峰无效。
- 结论：ISC 对该客户端是**硬并发配额约 11**，与本地 DecodePool 36 路、
  task 优先级、URL 缓存、启动节奏均无关。生产能 36 路说明其 ISC 账号/
  白名单/直连方式不同；本地不修改生产配置无法突破。
- 影响文件：`src/server.hpp`

### ADR-055 接受 ISC 配额，保持 36 路同等

- 日期：2026-11-09（迭代 24）
- 用户决定：ISC 并发配额限制不再处理，保持 36 路同等取流。
- 决定：保留取消优先级、previewURL 10 分钟复用、1.2s 错峰；不再尝试
  突破 ISC 11 路并发。
- 直连 RTSP：本地 DecodePool 4×9=36、HighResLease 36，若 36 路均为
  直连 RTSP/RTMP，ISC 会话限制消失；但 36 路同时 4K 仍受网络带宽/
  VPU 解码能力限制，不能保证任意分辨率稳定。
- 影响文件：`src/server.hpp`, `src/isc.hpp`

### ADR-056 告警图标签固定贴框，预览才做多标签 slot 错位

- 日期：2026-11-09（迭代 25）
- 背景：`render_jpeg` 同时服务预览和告警图，预览的 `label_slot` 多标签
  错位被带进告警图，标签浮在红框上方。
- 决定：`alarm_render_` 为 true 时 `label_slot` 固定 0，标签画在框顶；
  只有实时预览继续向上错位。
- 验证：58427 新告警图标签贴在红框上边沿。

### ADR-057 ring 片段落盘失败时回退 decoder dump 生成告警视频

- 日期：2026-11-09（迭代 25）
- 背景：HEVC 通道 `packet ring clip` 返回成功但实际没有文件，
  `finalize_video` 报 `dump file missing`，video_path 为空；只有 6488
  （H.264/本地）有视频。
- 决定：`clip_recorder` 在 `request_clip` 后校验 out 文件存在且 >1KB，
  不满足返回 -1；容器有 `AIDETECT_NO_DUMP=1`，因此 `save_alarm` 改走
  外部 video service `dump_in_worker` 回退。
- 验证：58427 新告警 15:00/15:01、三亚 f079e823 15:21:19 的
  `video_path` 均为 True，三亚文件 543KB。
- 影响文件：`src/task.hpp`

### ADR-058 接入 threshold_for_people=0.85 与 enable_people_deep_mode

- 日期：2026-11-09（迭代 25）
- 背景：生产任务 UserData 有 `threshold_for_people=0.85`、
  `enable_people_deep_mode=true`；206.25 people base 固定 0.3，V3/vest
  误检分布与生产不同。
- 决定：V3/vest/boat 的算法配置新增两项；`parse_task_config` 解析为
  `ModelConfig.people_conf/people_deep`；Task 构造 people base 时取最大
  `people_conf` 作为 base conf。
- 结果：日志 `people base conf=0.850000 deep=1`；V3 所有任务
  `no_helmet_det_threshold` 对齐 0.7、`keep_no_sec=5`。
- 限制：`enable_people_deep_mode` 只解析/记录，people 模型输入尺寸固定，
  未真正放大分辨率。
- 影响文件：`src/common.hpp`, `src/task.hpp`, 206.25 `tb_algo_config`

### ADR-059 pending 期间新告警先 finalize 旧片段，不再丢视频

- 日期：2026-11-09（迭代 25）
- 背景：`save_alarm` 在 `pending_.active` 时直接返回，导致上一个片段
  未完成期间的新告警 `video=""`。
- 决定：先 `finalize_video(true)` 收尾旧片段，再继续新告警的视频生成。
- 验证：三亚 15:28:30 连续触发的新告警 `video_path=True`。
- 影响文件：`src/task.hpp`

### ADR-060 看门狗无帧阈值 15s→45s，避免杀告警 packet ring

- 日期：2026-11-09（迭代 25）
- 背景：三亚 15:28:30 告警片段还在写，15:28:45 看门狗因 15s 无帧
  重启网关，15:28:51 新告警 clip timeout（rc=134），视频为空。
- 决定：无帧阈值改为 45s，最少重启间隔 60s；给 packet ring 留出
  告警 pre/post 窗口。真正死流仍会重连。
- 影响文件：`src/task.hpp`

### ADR-061 通道重名由前端自动改 `_2`，避免后端 500

- 日期：2026-11-10（迭代 26）
- 背景：`AddChannel` 对重复通道名称抛未处理异常，HTTP 500，前端只显示
  “服务器错误，请重试”。
- 决定：`TableModal.vue` 保存前拉通道列表，重名自动改成 `名称_2` 并提示；
  后端接口未改（当前工作区无 API 源码）。
- 影响文件：`web/src/views/bi/channel-management/components/TableModal.vue`

### ADR-062 预览 dets 保留 5s；标签同行贴框且不显示置信度

- 日期：2026-11-10（迭代 26）
- 背景：推理繁忙时 `dets_` 超过 800ms 即清空，预览红框消失约 3s 后
  又回来；标签还有时带置信度、且多标签向上堆叠离框越来越远。
- 决定：`dets_` 有效期改为 5000ms，期间按速度外推；`render_jpeg`
  不再拼 score；多标签同一行横向排，超出画面宽度才向上换行。
- 验证：09:30:53 预览标签无置信度、贴框，红框不再空窗。
- 影响文件：`src/task.hpp`

### ADR-063 标签垂直堆叠；删除中心距离兜底，第二目标重新 5s 倒数

- 日期：2026-11-10（迭代 27）
- 背景：横向标签不符合生产；已有红框时新目标靠近它会被中心距离匹配，
  继承 confirmed，直接变红，跳过 5s 蓝框倒数。
- 决定：标签第一块底边贴框顶、其余垂直向上；目标匹配只保留
  IoU>0.15，删除中心距离兜底；6488 持续时长恢复 5s。
- 验证：09:52:57 预览“未穿救生衣/未穿反光衣”垂直贴框；新目标需重建
  TargetState，重新 5s 倒数。
- 影响文件：`src/task.hpp`

### ADR-064 预览编码改 worker + latest-frame，不做 FIFO

- 日期：2026-11-10（迭代 28）
- 背景：9 路 tile 在解码线程里同步 `bmcv.imencode`，JPU 互斥串行导致解码
  等待，SAIL 解码器持续交付旧包，合流时间流速越来越慢。
- 决定：每 Task 一个 preview worker；解码线程只在 worker 空闲时拷贝当前帧，
  worker 完成即丢弃过期帧；不建预览队列，保证合流永远是最新内容。
- 代价：JPU 繁忙时单通道可能低于 10fps（丢帧而不是积压）；预览不再与
  解码严格一一对应；告警视频链路不受影响。
- 验证：64s 窗口内 tile OSD 也走 64s；all_1 600 帧/64.1s≈9.36fps。
- 影响文件：`src/task.hpp`

### ADR-065 合流槽位按首空顺序分配，停止只留空

- 日期：2026-11-10（迭代 28）
- 背景：hash 散列把任务打散到 4 组，停止后槽位空洞位置不可预期；生产是
  先启动先占前面的格子，停止只把该格变“任务未配置”，其它格不动。
- 决定：`assign_slot` 从 group0.slot0 开始找第一个空槽；`stop` 只
  `Aggregator::clear(group,slot)` 并释放该槽，不搬动其它任务；新任务填回
  第一个空槽。`VideoRoute=36` 让后端返回 4 个合流通道，前端兜底补 `all_4`。
- 代价：进程重启后任务会按启动顺序重新排布；客户端必须以 taskId 订阅，
  不能把 slot 当持久标识。
- 验证：35 task 时 group0/1/2 各 9 格、group3 0..7；停 group0 slot8 只有
  右下变未配置，重启回 slot8。
- 影响文件：`src/server.hpp`、`web/src/views/bi/current-preview/Index.vue`

### ADR-066 告警上报 `max(task, algo)` 硬门控 + next_report

- 日期：2026-11-10（迭代 28）
- 背景：任务级 3 分钟与算法级 86400s 并存时，旧门控在部分算法上仍按
  3 分钟重复上传。
- 决定：`report_gap=max(task_gap, algo_gap)`；正数 algo_gap 对所有目标生
  效，显式 0 保持“每目标一次 + task floor”；每次上报写 `next_report_ms`，
  下次只有 `now >= next_report_ms` 才允许。日志打印 gap 和剩余时间。
- 代价：门控只在进程内保存，容器重启后首次会放行一次；当前接受并记录。
- 验证：`alarm gate open ... gap_ms=86400000`；12:39:30 后 6488 同算法
  到 12:45+ 无重复。
- 影响文件：`src/task.hpp`、`src/common.hpp`

### ADR-067 检测框绑定推理帧时间，推理队列 latest-only

- 日期：2026-11-10（迭代 30）
- 背景：合流画面出现“框不跟人”。`dets_ms_` 原来记推理完成时间，而框属于
  更早的推理帧；同时推理 FIFO 保留 2 帧，推理慢时处理旧帧，框滞后更明显。
- 决定：`InferJob{BMImage, frame_ts}` 把采集时间带入推理；`dets_ms_ =
  frame_ts`；推理队列只保留最新一帧；预览按 `now - frame_ts` 外推，上限
  1.5s（超过 3s 清空）。生产 `FramePackage::setBoxes/getBoxes` 按帧号存框、
  `board::track::get_iou_tracker` 也印证框要与帧绑定。
- 代价：推理忙时丢中间帧，检测频率可能下降；外推是线性近似，急转向仍会
  有少量偏差（生产用 IoU tracker 进一步处理）。
- 验证：`preview_merge_box_sync_20261110.jpg` 办公室画面框落在人身上；
  单通道/告警图同步。
- 影响文件：`src/task.hpp`

### ADR-068 配置区域取原图；单/合流/告警图绘 ROI 绿线

- 日期：2026-11-10（迭代 30）
- 背景：配置区域取到带检测框/标签/任务条的 `latest_jpeg_`；生产配置页是
  干净原图，且三类预览/告警图都画区域线。
- 决定：新增 `area_jpeg_`，`force_area_` 时用空 dets、无任务条、无 ROI
  渲染原图；`taskArea` 归一化多边形去重后存 `roi_polygons_`，用
  `bmcv.polylines`/`cv::polylines` 绘绿色闭合线（单/合流/告警图），
  配置原图不画。
- 代价：每次配置页请求多一次 VPP+JPU 原图渲染（低频，可接受）；ROI 在
  任务构造时解析，改区域由 10s 配置轮询的 ROI 签名变化触发重启。
- 验证：`preview_area_raw_20261110.jpg` 无框；`preview_single_roi`/
  `merge_roi`/`alarm_roi` 均见 6488 绿色 ROI；14K 保存后
  `preview_14k_roi_20261110.jpg` 出现绿色多边形。
- 影响文件：`src/task.hpp`、`src/server.hpp`

### ADR-069 无合流槽位时 AI 返回 409，前端 36 路预检

- 日期：2026-11-10（迭代 30）
- 背景：AI 无槽位时仍返回 HTTP 200 `{"status":"error"}`，.NET
  `StartTaskAsync` 只看非空响应就 Success，DB 置 Running；第 37 个任务
  点配置区域才报“服务器错误”。
- 决定：AI `/task/start` 失败返回 HTTP 409；前端启动前统计 status=1
  任务，>=36 直接提示并不发 PATCH；后端仍保留最终门控。
- 代价：前端多一次任务列表请求；若 DB 状态与 AI 实际运行不一致，可能
  多拦一次（需先停止/修复异常任务）。
- 验证：AI 直接启动第 37 条 → `409 {"msg":"没有可用的合流槽位"}`；
  后端 PATCH 后 DB 仍 status=2；Web 只弹“合流槽位已满…”，PATCH=0。
- 影响文件：`src/server.hpp`、`web/src/views/bi/task-management/Index.vue`

### ADR-070 保存后直连 AI 触发热更新，2s 轮询兜底

- 日期：2026-11-10（迭代 31）
- 背景：`main.cpp` 只在启动时调用一次 `reconcile_from_backend()`，所谓
  “每 10s 检查配置”实际不会再次执行；签名也不含 ROI 与
  `threshold_for_people` 等参数，导致 Web UI 改 ROI/移除反光衣/调阈值后
  运行中的 Task 仍用旧配置，预览继续出旧标签。
- 生产证据（只读）：`handleRemoteMessage` 解析保存消息后调用
  `onTaskArrived`，再走 `TaskChannel::archiveUpdated` →
  `TaskInstance::taskArchiveApply`；没有构造新 TaskChannel/TaskInstance，
  app.log 17:26:02 仅 `archiveUpdated` + `HandleTaskChanged`，无
  `Task Building`。生产是保存请求直接触发，不是轮询。
- 决定：新增 `POST /task/reload_config`（JWT+CORS）；前端任务/ROI 保存
  成功后直连 9000 触发；`TaskManager` 拉新配置后调用 `Task::update_config`，
  推理线程帧间重建 `models_`/`roi_polygons_`，不释放槽位；2s 轮询保留为
  兜底，仅源地址/任务名变化才 stop/start。
- 代价：模型重建会清空该任务当前检测状态/告警目标；直连失败时最多等
  2s 轮询；2s 轮询对 36 个 task 有周期 HTTP 请求。
- 验证：Playwright 在 14K ROI 页面点保存，请求
  `9000/task/reload_config`；16:27:49 日志
  `direct hot update: 6488` + `hot config applied models=3 rois=1`，
  无 `task started`，响应 `{"status":"ok"}`；槽位不变，预览更新。
- 影响文件：`src/main.cpp`、`src/server.hpp`、`src/task.hpp`、
  `web/src/services/aiTask.ts`、`web/src/views/bi/task-management/AddOrEdit.vue`、
  `web/src/views/bi/task-management/Config.vue`

### ADR-071 所有算法/告警统一按框中心过滤 ROI

- 日期：2026-11-10（迭代 32）
- 背景：`filter_roi` 原来只作用于基础人形检测；V3/vest/boat/behavior 从
  `base` 人形框裁剪推理后没有再过滤，`update_alarm` 和最终 `dets_` 也没有
  ROI 检查，导致绿色区域外的目标仍画框、打标签、告警。
- 生产证据（只读）：`SystemAlarmFilter::BoxInRoiRegion`、
  `isBoxCenterInRegions`、`isBoxCornerPointInRegions`、
  `centerInRoiRegion`，说明生产按框中心/角点判断是否在 TaskRegion 内。
- 决定：统一 `filter_roi_polys`（点在多边形内，取框中心）；算法自己的
  `taskArea` 优先，为空回退任务级 ROI；`update_alarm` 入口过滤，
  `detect()` 写 `dets_` 前再兜底过滤，保证框/标签/告警三者一致。
- 代价：中心点在 ROI 外、但框部分压在 ROI 内的目标会被过滤；与生产
  `isBoxCenterInRegions` 的取舍一致。
- 验证：7c 低阈值下 ROI 外目标无框/无标签；6488 ROI 内目标仍保留；
  测试后已恢复两任务人员阈值 0.9。
- 影响文件：`src/task.hpp`

### ADR-072 仅曾出帧任务 2min 无帧时逐个重启，避免 ISC 会话风暴

- 日期：2026-11-11（迭代 33）
- 背景：gateway 被 watchdog 杀掉后，`Decoder::read()` 仍可能永久阻塞，
  Task 显示 running 但无帧；手动 stop/start 才能恢复。最初按 75s 批量
  重启，导致 ISC previewURL 会话风暴，反而有 23 路无帧。
- 生产证据（只读）：`MediaChannel::mediaReconnect()`、
  `MediaChannel/DecodeInstance/TaskChannel/TaskInstance::healthy()`、
  `TaskInstance::checkRunCondition`，以及 “Try reconnect media”
  “Automatically attempting to reconnect”；生产是按 media channel
  健康状态重连，并做节流。
- 决定：`TaskManager` 只接管 `everFrame=true` 且 2 分钟无帧的任务；
  一次只重启一个，间隔 30s，单任务 10 分钟退避。从未出帧的任务多为
  ISC `missing data.url`/源离线，不做任务重启，避免抢占健康通道会话。
- 代价：曾出帧但 decoder 永久阻塞的通道最多 2 分钟才恢复；不是秒级。
- 验证：5 路重点 ISC 通道全部恢复（age 2~546ms）；09:18 supervisor
  逐个重启长期无帧任务；8 路从未出帧的其它 ISC 通道日志为
  `ISC response missing data.url`，与用户列出的 5 路无关。
- 影响文件：`src/server.hpp`、`src/task.hpp`、`src/isc.hpp`、`src/main.cpp`

### ADR-073 通道/任务状态改用 AI 实时帧健康，不信任后端在线状态

- 日期：2026-11-11（迭代 34）
- 背景：通道页“在线”和任务页“运行中”来自后端配置状态；视频卡住/无帧
  时仍显示正常，无法看出 `Input/output error`/`Camera not found`，与生产
  通道页的“正常 + 分辨率/错误”不一致。
- 决定：`/task/list` 返回 `taskName/channelAddress/resolution/health/
  lastError`；前端通道页按 `channelAddress` 映射运行任务，任务页按
  `taskId` 映射；健康状态由 AI 帧时间判定（ok/connecting/stalled/error），
  并把长 stderr 归一为生产同款短状态。
- 代价：页面每 3~5s 需要直连 9000 拉 `/task/list`；AI 不可达时退化为
  “启动中/未接入”，不会误报“运行中”。
- 验证：Playwright 截图 `preview_ui_channel_health_20261111.png` 显示
  `正常 2560x1440`/`正常 1280x720`/`正常 704x576`；
  `preview_ui_task_health_20261111.png` 显示 `Input/output error`/
  `Camera not found`。
- 影响文件：`src/task.hpp`、`web/src/services/aiTask.ts`、
  `web/src/views/bi/channel-management/Index.vue`、
  `web/src/views/bi/task-management/Index.vue`

### ADR-074 无任务通道用按需 ffmpeg 探测，不重构常驻 MediaChannel

- 日期：2026-11-11（迭代 35）
- 背景：生产通道管理在无任务通道也显示状态/分辨率；生产代码有独立
  `mediaChannels`、`MediaChannel`、`MediaInstance`、`DecodeInstance` 和
  `storage::updateVideoStatus`，而 `TaskChannel` 只负责检测任务。我们
  当前只有 Task 才启动 gateway/decoder，无任务通道显示“未接入”。
- 选项对比：
  - 全量重构：为 39 路建立常驻 MediaChannel/decoder。状态最接近生产，
    但额外占用 39 路 ISC 会话/网络，并可能占用 VPU；当前 36 路 AI 已
    接近上限，容易引发 ISC 会话风暴。
  - 按需探测（采用）：对当前页无任务的在线通道调用
    `POST /channel/probe`，`ffmpeg -c copy` 只读流头 6s，解析分辨率，
    60s 缓存、2 并发、不进 BPU；有任务通道继续用 Task 的真实状态。
- 决定：暂不重构任务/媒体管线；当前共用 Task 内 gateway/decoder，没有
  重复取流，按需探测以极小资源补齐状态/分辨率。只有未来要求所有未配置
  通道持续实时预览时，再做共享 MediaChannel 重构。
- 代价：探测最长 6s，页面首次打开有“正在探测”；ISC 探测会短暂占用
  一个预览会话；每 30s 重新探测当前页。
- 验证：茶水间（无任务直连）`正常 2560x1440`；五公司-冲洗区1（无任务
  ISC）`Input/output error`；截图 `preview_ui_channel_probe_20261111.png`。
- 影响文件：`src/server.hpp`、`web/src/services/aiTask.ts`、
  `web/src/views/bi/channel-management/Index.vue`

### ADR-075 ISC Camera not found 状态直显；UDP 缓冲 8MB 防 4K 花屏

- 日期：2026-11-12（迭代 36）
- 背景：钢筋厂2/3/4 长期显示“已断开 6705s”，单通道 4K 预览底部彩色撕裂。
- 证据：ISC previewURLs 对 `b7cc9d...` 返回
  `Camera not found. cameraIndexCode not found`；生产 app.log 同样用该
  cameraIndexCode 每 5s 重试，说明是 ISC 源/编码问题，不是本地重连。
  4K 花屏是本地 gateway UDP 接收缓冲 1MB 不足导致丢包。
- 决定：Task 状态优先显示 `Camera not found`/`Input/output error`，
  不再用“已断开 Ns”掩盖根因；gateway UDP 接收缓冲 8MB +
  `overrun_nonfatal=1`；不尝试用重启修复不存在的相机。
- 代价：花屏只能降低概率，源端丢包/编码损坏仍可能发生；Camera not found
  需要用户更换正确 cameraIndexCode/直连 URL。
- 验证：group3 三块黑屏显示 Camera not found；6488 复抓无底部撕裂。
- 影响文件：`src/task.hpp`

### ADR-076 预览框按帧时间快照对齐；MP4 文件源循环且不上报告警

- 日期：2026-11-12（迭代 37）
- 背景：预览只用最新检测框，推理提前完成时会拿未来帧框套到旧预览帧，
  看起来框跳/滞后；同时需要支持本地 MP4 循环作为任务源。
- 生产证据（只读）：`FramePackage::setBoxes/getBoxes`、
  `board::track::get_iou_tracker`、`TrackBoxesByHandler`，框与帧绑定。
- 决定：`dets_history_` 保存最近 20 个 `(frame_ts, boxes)`，预览选
  `ts <= now` 的最新快照，再用 `vx/vy` 外推；MP4 通道上传后保存到
  `data_root/media`，Task 用 `-stream_loop -1 -re`，走同一条 gateway/
  decoder/检测/预览链路；`file_source_` 下跳过 `save_alarm` 和 alarm ring。
- 代价：框仍以检测频率更新，只做帧时间对齐+速度外推，不引入逐帧光流；
  MP4 首帧/循环点可能有极短停顿。
- 验证：H264 640x360 MP4 循环，`health=ok`、`resolution=640x360`，
  `tb_alarm` 记录 0；测试通道/任务已清理；前端 MP4 上传栏截图。
- 影响文件：`src/task.hpp`、`src/server.hpp`、`web/src/services/aiTask.ts`、
  `web/src/views/bi/channel-management/components/TableModal.vue`

### ADR-077 预览框逐帧 LK 光流；确认目标保留 5s→1s 消除 ROI 残框

- 日期：2026-11-12（迭代 38）
- 背景：检测频率低于预览帧率，框在检测间隔内不动；人离开 ROI 后最后一个
  边缘框/标签保留约 5s。
- 决定：预览线程对每个检测框在 640 宽灰度图上用
  `calcOpticalFlowPyrLK`（4×4 点阵、中值位移）逐帧移动；新检测框重置
  跟踪点。`rt.active` 确认目标保留 5s→1s。
- 代价：预览线程增加一次灰度转换+光流（仅观看的通道）；快速变向时
  光流可能跟不上，检测框仍由 AI 推理频率纠正。
- 验证：6488 单通道抓图框/人物/ROI 正常；无花屏；36 路检测主循环未受影响。
- 影响文件：`src/task.hpp`、`CMakeLists.txt`（opencv_video）
### ADR-078 预览框按检测帧绑定，不再用“最新检测 + 外推”；取消 1s 边缘框缓存

- 日期：2026-11-12（迭代 39）
- 背景：ADR-076/077 仍把“最新检测框”套到当前预览帧，再靠速度外推和
  640 宽 LK 跟帧；检测约 1s 一次、推理有延迟，框会慢半拍并向右/向上
  漂移；人出 ROI 后 `rt.active` 的 1s TTL 让边缘框继续停留。生产盒子
  用 `FramePackage::setBoxes(frameId, boxes)` / `getBoxes(frameId)`
  把框与检测帧绑定，并由 `IoUTracker` 维持目标身份。
- 决定：
  - `PreviewTrack` 保存 `(ts, box)` 历史；新检测到来时用 `dets_ts` 找
    轨迹在那一帧的姿态，只把“检测框 − 该帧姿态”的增量应用到当前姿态；
  - 帧间用 LK（960 宽、VPU 预缩放）推进，LK 失败时用内框
    `matchTemplate` 兜底；
  - 属性算法漏检时用全帧人员框把标签重锚到人员框上（不再用旧位置）；
  - 框中心离开 ROI 立即删除；新检测快照无匹配且无人员锚点的轨迹删除；
  - 删除 `rt.active` 1s 缓存，不使用“保留最后边缘框”的兜底。
- 代价：属性算法漏检时标签可能闪现/短暂消失；人员框本身偏大/偏移时，
  框会跟着人形框偏；检测仍约 1s 一次，极限快速变向仍可能有一帧误差。
- 验证：见 `docs/PROGRESS.md` 迭代 39；生产逆向证据见
  `docs/assets/prod_box_frame_binding_20261112.txt`；回放图
  `docs/assets/preview_track_follow_20261112.jpg`、
  `preview_track_two_people_20261112.jpg`、
  `preview_track_edge_gone_20261112.jpg`；最终现场边缘前后帧
  `preview_track_edge_last_box_20261112.jpg` →
  `preview_track_edge_cleared_20261112.jpg`。
- 影响文件：`src/task.hpp`（`PreviewTrack`、`track_preview_boxes`、
  `maybe_preview`、`dets_persons_`、`DetSnapshot`）


### ADR-079 推理改 FIFO 公平闸门 + 有界并发 + 每任务需求上限，替代全局串行锁

- 日期：2026-11-12（迭代 40）
- 背景：ADR-013 的“共享模型串行推理”在 15 个任务并行时变成全局锁：实测单次
  推理 187ms 里 172ms 在等锁，TPU 真正计算约 15ms；同时排队 15 个请求，
  4K/5 算法的任务一轮检测 1.7s，预览因快照超过 1.5s 而完全不画框（需求 3）。
  生产盒子不是这个结构：`ImageTaskInstance::workLoop` +
  `onInferenceFinished` 异步回调、`ThreadSafeLazyQueue<FramePackage>` 只留最新帧、
  `FramePackage::algoSubmitted` + `TaskInstance::onAllAlgoSkipped` 明确“跟不上就整帧跳过”。
- 决定：
  - `ModelScheduler::infer` 去掉全局 `infer_mu_`，改 FIFO 票据闸门（严格到达顺序准入），
    并发数 `AIDETECT_INFER_CONCURRENCY`（默认 2）；每模型实例池
    `max(1, 并发/2)`，避免同模型任务挤在一个 engine 锁上。
  - 每任务检测节奏上限 `AIDETECT_DETECT_INTERVAL_MS`（默认 300ms，4K 源 ×1.5）；
    裁剪算法只处理 `AIDETECT_CROP_PERSONS`（默认 4）个最大人形框；
    `AIDETECT_ATTR_MODELS_PER_ROUND` 可把裁剪算法改轮转（默认全部）。
  - 这是生产 `onAllAlgoSkipped` 的等价物：用**需求上限**代替“把请求堆进全局队列”，
    保证任何一个任务都不会被别人的推理饿死。
- 代价：
  - 检测节奏从 10Hz 降到约 3.3Hz（4K 约 2.2Hz），框间完全依赖预览 LK 跟踪；
  - 画面里同时超过 4 个目标时，最小目标这一轮不做属性判定（基础人员框仍在）；
  - 并发=2 是实测保守值，调大不一定更快（bmcv/JPU 是共享硬件）。
- 验证：见 `docs/PROGRESS.md` 迭代 40、`docs/assets/infer_scheduler_measure_20261112.txt`
  与 `docs/assets/preview_test2_{starved,boxes,follow,follow2}_*.jpg`；
  生产证据 `docs/assets/prod_infer_scheduler_syms_20261112.txt`。
- 影响文件：`src/model.hpp`（`enter_queue`/`leave_queue`、profile 计数）、
  `src/task.hpp`（检测节奏、裁剪人数、轨迹聚类、瓦片令牌桶）、`src/common.hpp`
  （`profiling()` 开关）。
- 关系：取代 ADR-013 中“串行推理”的部分；ADR-067 的 latest-only 推理队列保留。

### ADR-080 每个算法标签现渲染底板；告警目标 TTL 2s；预览轨迹按 label 关联

- 日期：2026-11-12（迭代 41）
- 背景：需求 2「检测标签乱飞、检测标签无字（应该是抽烟行为）」的实测根因不是
  绘制顺序，而是三件事叠加：
  1. `/app/labels/label_*.png` 只有 6 个历史名字，而算法上报的
     `report_content` 是 `抽烟/打电话`、`人员聚集`、`人数监控`，没有底板；
  2. FreeType 兜底 badge 用了 `Point(4, bh-4)`，而 OpenCV FreeType2 的
     `putText` 以左上角为原点（`task_bar_mat` 早前已修过同一个坑），文字画到
     底板外 → 纯红条；
  3. 快速预览路径 `render_preview_fast` 只贴底板，没有底板就完全不画标签。
  另外行为模型每轮在 吸烟/打电话/玩手机 之间跳类，`update_alarm` 1s 没命中就删目标、
  预览轨迹又要求 class_id 相同才匹配，导致抽烟目标永远到不了 5s 持续时长。
- 决定：
  - `Task::ensure_label_plate(text)` 在任务启动和热更新时，为本任务所有算法的
    `report_content` 用 FreeType 现渲染「红底白字」底板（左上角原点 `(6,3)`），
    放进 `label_plates_`；单通道/合流/告警图共用。缺底板时兜底 badge 原点改 `(4,2)`。
  - 告警目标存活 TTL：1s → `AIDETECT_TARGET_TTL_MS`（默认 2000ms），容忍间歇性
    分类器；预览幽灵框仍由跟踪器 600ms 人形锚点宽限控制，不受该 TTL 影响。
  - 预览轨迹关联只比 `label`，不比 `class_id`（行为模型 class 会跳）。
  - 同一目标多算法命中，聚类后只保留一种状态：有 confirmed 只画红色标签，
    否则只画一个蓝色倒计时。
- 代价：目标重现在 2s 内会继承 confirmed 状态（红色），不再重新走 3~5s 倒计时；
  标签底板是任务级缓存，算法热更新新增标签时才补渲染。
- 验证：`docs/PROGRESS.md` 迭代 41；图
  `docs/assets/preview_test2_smoking_label_20261112.jpg`、
  `docs/assets/preview_test2_pending_countdown_20261112.jpg`；
  日志 `label plate rendered: 抽烟/打电话`、`label plates=8`。
- 影响文件：`src/task.hpp`（`ensure_label_plate`、`label_draws` badge、
  `target_ttl_ms_`、`track_preview_boxes` 关联、聚类输出状态）、`src/common.hpp`
  （`jpeg_markers_ok`）、`src/server.hpp`（合流瓦片校验/保留上一帧）。

### ADR-081 标签底板“先画后量”；30+ 任务按容量上限处理，并发保持 2

- 日期：2026-11-13（迭代 42）
- 背景：
  1. `ensure_label_plate` 原来用 `getTextSize()` 估算底板尺寸，实测与真实字形不符
     （`抽烟/打电话` 底板 197x34，末字被裁），需要以实际绘制结果为准；
  2. 现场把 33 个任务全部开启后，测试_2 检测轮 199~836ms、预览 2.1fps。
     实测推理闸门 `wait_avg=0`、`peak_pending=5~9`，并发 2 与 4 结果一致，
     说明瓶颈是 33 路解码 + VPP/JPU + 预览跟踪 CPU，而不是推理准入。
- 决定：
  - 底板生成改为在固定红底画布上 `putText`，再用 `inRange+boundingRect` 取真实字形框，
    四周留 8/4 px 裁切；不再依赖 `getTextSize` 的估算值。
  - 推理并发默认保持 2（`AIDETECT_INFER_CONCURRENCY` 可调）；30+ 任务不再通过加并发
    解决，按容量上限处理，需要时减算法/减任务/换盒子。
- 代价：`getTextSize` 相关的旧假设作废；33 任务场景帧率仍低（约 2fps），
  属于硬件容量限制，不是软件缺陷。
- 验证：图 `docs/assets/preview_test2_label_full_text_20261113.jpg`（修复后）/
  `docs/assets/preview_test2_label_clipped_before_20261113.jpg`（修复前）；
  33 任务实测见 `docs/PROGRESS.md` 迭代 42 与 KNOWN-ISSUES KI-031。
- 影响文件：`src/task.hpp`（`ensure_label_plate`）、`docs/KNOWN-ISSUES.md`（KI-031）。

### ADR-082 标签按开关动态拼接、底板构建期预渲染；单通道预览 720p；并发默认 4

- 日期：2026-11-13（迭代 43）
- 背景：
  1. 标签之间有 6px 空行：行为模型把 1.10 倍扩边**裁剪框**当检测框上报，
     比其他算法的人形框高 6px，两段标签栈各自从不同 y 起算；
  2. 标签文案写死 `抽烟/打电话`，与实际开启的算法不符；
  3. 底板在任务启动时用 FreeType 现画，字体/字号虽有但每个任务都重复渲染，
     且生产是把底板和算法资源一起预置的；
  4. 生产实测（只读网页）：22 任务、合流 1920×1080 8.7~9.2fps、单通道 1280×720 4.9fps；
     我们 33 任务合流 3.0fps、单通道 2.5fps。cap=2 时推理 `wait_avg=13~19ms`，
     cap=4 时 `wait_avg=0`、测试_2 单轮 617→399ms。
- 决定：
  - 行为检测框改回人形框 `p`（扩边只用于模型输入裁剪），标签栈自然合拢；
  - `ModelRuntime::display_label` 按 `behavior_enable_smoking/calling/playing`
    拼标签（单词直接显示，多个用 `/` 连接）；
  - 新增 `tools/gen_label_plates.cpp`：构建期把全部标签渲染成
    `docker/labels/plate_*.png` + `labels.json`；运行时只读清单，缺标签才回退现画
    （并打 WARN 提示重新生成清单）；
  - 单通道预览上限 1920×1080 → **1280×720**（对齐生产，省 2.25× VPP/JPU）；
  - 推理并发默认 2 → **4**（`AIDETECT_INFER_CONCURRENCY` 可调）；并加
    `ModelScheduler::load()`，队列深度 > 2×cap 时任务按比例拉长检测间隔
    （最多 3×，即生产 `onAllAlgoSkipped` 的等价物）。
- 代价：
  - 单通道预览从 1080p 降到 720p（生产同款），细节略降；
  - 积压时检测节奏最多降到 1/3（33 任务下测试_2 约 0.7s 一轮）；
  - 生产合流 9fps 与我们 3fps 的差距**没有消除**，只是把推理排队去掉了。
- 验证：`docs/PROGRESS.md` 迭代 43；图
  `docs/assets/preview_test2_labels_flush_20261113.jpg`、
  `docs/assets/prod_preview_channels_20261113.png`、
  `docs/assets/prod_preview_single_20261113.png`、
  `docs/assets/labels_manifest_20261113.json`。
- 影响文件：`src/task.hpp`（行为框、display_label、预览 720p、自适应间隔、阶段计时）、
  `src/model.hpp`（默认并发 4、`load()`）、`tools/gen_label_plates.cpp`、
  `CMakeLists.txt`、`docker/labels/*`。

### ADR-083 每算法独立节奏 + 每预览帧只缩放一次（对齐生产 requireScaled）

- 日期：2026-11-13（迭代 44）
- 背景：cap=4 已把推理排队消掉，但 33 任务时合流仍 2.8fps。profile 显示一帧预览
  把 4K 源缩放了三次（跟踪 4K→960、单通道 4K→1280、瓦片 4K→640），
  `track_ms=109`、`render_ms=30~268`，全在抢同一套 VPP；同时每轮把所有裁剪
  算法都跑一遍（测试_2 单轮 103~617ms）。
- 决定：
  - `ModelRuntime::min_interval_ms/last_run_ms`：裁剪算法按各自间隔刷新
    （`AIDETECT_ALG_INTERVAL_MS` 默认 400ms，boat×1.25、behavior×1.5），
    基础人员模型仍每轮；跳过的算法靠预览跟踪的人形锚点维持标签
    （守护窗口 = max(600ms, 最大间隔×1.5)）。这是生产 `onAllAlgoSkipped`
    的等价物。
  - 预览改为**每帧只缩放一次**：`maybe_preview` 先把源帧缩到 `preview_target`
    （1280×720），跟踪、单通道渲染、合流瓦片共用；检测框在进入跟踪前统一换算到
    预览坐标系，渲染时 `sx=1`。对齐生产 `DecodeInstance::requireScaled`。
- 代价：
  - 裁剪算法刷新率降到 2~2.5Hz/算法（告警确认仍按 5s 持续，不受影响）；
  - 4K 源仍需一次 4K→720 的 VPP 缩放，33 任务时该路预览约 3.7fps；
  - 合流仍低于生产 9fps（我们 33 任务 vs 生产 22 任务）。
- 验证：`docs/PROGRESS.md` 迭代 44；
  `docs/assets/preview_test2_720p_20261113.jpg`、
  `preview_b062_720p_20261113.jpg`、`merge_all1_tiles_20261113.jpg`；
  实测合流 2.8→5.6~7.0fps、`render_ms` 30~268→20~67ms。
- 影响文件：`src/task.hpp`（`algorithm_interval_ms`、裁剪算法门控、`maybe_preview`
  缩放一次、坐标换算、`person_grace_ms_`）。

### ADR-084 检测裁剪改用缩放帧；性能对比必须在任务上量完成后测量

- 日期：2026-11-13（迭代 45）
- 背景：
  1. 33 任务时 4K 源每轮要做 1 次全帧缩放（人形检测）+ N 次人形裁剪（属性算法），
     每次裁剪都从 4K 原图取像素，VPP 压力大；
  2. 迭代 44 的“合流 5.6~7.0fps”是在容器重启后 30~60s 内测的——那时 33 个任务
     还没全部出图，读数虚高；稳定后同版本只有 3.0fps。**性能结论必须等任务上量完成。**
- 决定：
  - 当画面里有人可裁时，每轮先把源帧 `vpp_resize` 到 1280×720 一次（`det_src`），
    人形框按 `det_scale` 换算到该坐标系后从缩放帧裁剪；人形检测仍用原图，
    所以检测框坐标不变。无人可裁的任务不付这次缩放。
  - 性能对比统一口径：容器启动后 ≥3 分钟（33 任务全部 running）、连续测 3 次取范围；
    `count_ws.js` 单次 10s。
- 代价：有人可裁的 4K 任务每轮多一次 4K→720 缩放；实测合流 3.0→6.5~7.1fps、
  4K 单通道持平、2.5K 单通道略降（6.3~8.1 vs 8.2~8.5）。
- 验证：`docs/PROGRESS.md` 迭代 45；`docs/assets/preview_test2_scaled_crops_20261113.jpg`；
  生产侧同口径 22 任务合流 8.7~9.2fps。
- 影响文件：`src/task.hpp`（`det_scaled`/`det_src`/`det_scale`、`crop_norm`、
  四个裁剪点、`maybe_preview` 缩放一次）。

### ADR-085 ISC 预览走子码流；单通道预览上限 1920×1080

- 日期：2026-11-13（迭代 46）
- 背景：
  1. ISC 任务全部 connecting 无画面。排查确认 `isc.hpp` 未改动（三个快照 md5 相同），
     失败在解码环节：ISC 主码流是 H.265，`-c copy` 进 MPEG-TS 后本机解码链路
     `no first frame`；生产预览页看到的 25K 是 704×576、预制场 1280×720，
     即生产预览用的是**子码流**。
  2. 迭代 44/45 把单通道预览上限降到 1280×720，与“≤1080p 保持原分辨率、
     >1080p 缩到 1080p”的规格不符（生产样例 720p 是因为那一路源就是 720p）。
- 决定：
  - ISC 候选顺序：`(proto,0) → (proto,1) → (另一协议,0) → (另一协议,1)`，
    子码流优先；直连 RTSP 相机行为不变。
  - `preview_target` 上限恢复 1920×1080（`AIDETECT_PREVIEW_*` 环境变量仍可覆盖）。
- 代价：1080p 预览的缩放+JPEG 编码成本约为 720p 的 2 倍——29 路有图时
  合流 2.8~3.0fps（720p 上限时为 6.5~7.1fps）；2.5K/ISC 单通道 6.2~8.3fps。
  这是规格与帧率的取舍，按规格执行。
- 验证：`docs/PROGRESS.md` 迭代 46；`docs/assets/preview_test2_1080p_20261113.jpg`；
  ISC 11 路→7 路恢复出图，整机 28 路 ok。
- 影响文件：`src/task.hpp`（`start_isc_gateway` 候选顺序、`preview_target`）。
### ADR-086 ISC 按通道 streamType 取流（不自适应）+ HEVC Annex-B 直通

- 日期：2026-11-13（迭代 47）
- 背景与取证（生产只读）：
  - `libEdgeSystem.so` 的 `ISCStreamFactory::GetMediaUrl` 用 `"streamType": %d` 调
    `/artemis/api/video/v1/cameras/previewURLs`，URL 模板里 streamType 是参数；
  - 生产 `table_media.MediaUrl` 逐路写死 streamType（25K=1、预制场=0、八所_预制区=1），
    说明生产**没有**主/子码流自适应；
  - 生产预览尺寸 = 码流原始尺寸（704×576/352×288/1280×720），二进制中只做
    1920×1080/1920×1088/1280×720 的白名单判断，没有 >1080p 降档分支；
  - 每通道一个 `DecodeInstance`，检测用解码原帧，预览用 `requireScaled` 缩放帧。
- 决定：
  - ISC 候选顺序 = 配置 streamType 优先，另一 streamType 仅兜底；协议同生产（配置优先）；
  - gateway 首次连接从日志识别编码，HEVC 时加 `-bsf:v hevc_mp4toannexb`（位置：`-i` 之后、输出之前），
    识别结果缓存到任务，重连不再探测；
  - 预览 `preview_target` = min(源, 1920×1080)，不放大。
- 结果：25K 主码流 704×576 与生产一致；29/32 任务 ok（凤凰咀两路为 ISC 500）。
- 影响文件：`src/task.hpp`（`start_gateway` bsf、`start_isc_gateway` 候选顺序、`upstream_hevc_`）。

### ADR-087 预览按源尺寸编码（取消 1080p 上限）

- 日期：2026-11-13（迭代 48）
- 背景：用户在生产新加一条 2560×1440 通道；只读测量生产预览 = 2560×1440 @ 5.00fps，
  未缩到 1080p。反编译 `JpegEncoderInstance::onImagePoped` 确认只调用
  `board::create_jpeg_encode(w,h,q)` 与 `board::frameToJpegByCpu(frame,q)`，无缩放步骤；
  质量按 `cmp #2/#1 → csel 70/40` 选择。
- 决定：`preview_target()` 返回源尺寸，不再按 1920×1080 上限；
  `AIDETECT_PREVIEW_WIDTH/HEIGHT` 仍可显式指定上限（4K 压测机用）。
- 代价：4K 片源按原尺寸编码更贵（33 任务、12 路 4K 时测试_2 1.6fps、合流 2.0fps）；
  真实相机 ≤2560×1440，不受影响。
- 影响文件：`src/task.hpp`（`preview_target`）。

### ADR-088 预览默认 1080p 上限（房规优先于生产原尺寸）

- 日期：2026-11-13（迭代 49）
- 背景：迭代 48 实测生产预览按源尺寸出图（2560×1440 通道 5fps 原样输出）。
  用户决定采用房规：≤1080p 不缩放、>1080p 等比缩到 1080p。
- 决定：`preview_target()` 默认按房规；`AIDETECT_PREVIEW_WIDTH/HEIGHT` 覆盖。
- 量化（同机同批 30 任务 A/B）：4K 单帧 JPEG 2.50MB→0.79MB；渲染 160~288ms→39~215ms；
  CPU/内存无变化；4K 单通道 2.0→2.4fps。省的是预览编码/带宽/JPU，不是整机算力。
- 影响：单通道预览清晰度上限 1080p；检测、告警图片仍用解码帧（告警图片尺寸随该规则）。

### ADR-089 预览缩放：整数倍用 VPP，非整数倍用 CPU cv::resize

- 日期：2026-11-13（迭代 50）
- 背景：2560×1440 相机预览花屏（底部彩色噪声、瓦片几乎全花）。对照实验证明
  `bmcv.vpp_resize` 只对整数倍缩小正确：×0.5 正常，×0.75 花屏；预览跟踪的 ×0.375 同样被污染。
- 决定：`maybe_preview` 仅在 `w%tw==0 && h%th==0` 时走 VPP；否则保留源帧，
  由 `render_jpeg`/`render_preview_fast` 的 `cv::resize` 完成缩放。跟踪缩放改为减半到 ≤1280。
- 代价：非整数倍通道多一次 CPU 缩放（实测反而更快：合流 2.9→4.9fps）。
- 影响文件：`src/task.hpp`（`maybe_preview`、`track_preview_boxes`）。

### ADR-090 预览只按整数倍 VPP 缩放（省 VPP/JPU），CPU 缩放仅兜底

- 日期：2026-11-13（迭代 51）
- 背景：用户要求不用 CPU 缩放，并担心 VPP 资源。
- 决定：`preview_target()` 取最小整数 k 使两维整除且 ≤1920×1080（2560×1440→1280×720、
  3840×2160→1920×1080、4096×2160→1024×540、3200×1800→1600×900）；
  `maybe_preview` 因目标是源的整数分之一，恒走 VPP；CPU `cv::resize` 仅在
  `AIDETECT_PREVIEW_WIDTH/HEIGHT` 强制非整数尺寸时兜底。
- 关键结论：**“不缩放”不比“整数倍缩放”省资源**——不缩放仍要做 VPP 格式转换 + JPU 整帧编码，
  只是像素更多；整数倍缩放后这两步都在更小的图上做，省 VPP/JPU。
- 实测：4K 单帧 render 160~288ms→42~79ms；合流 3.4→7.0~7.3fps；2.5K 单通道 6.7→8.9fps；
  CPU/TPU 不变；内存 1527→1473MB。
- 影响文件：`src/task.hpp`（`preview_target`）。

### ADR-091 资源看门狗：BM 缓冲/RSS 超限自重启进程

- 日期：2026-11-13（迭代 52）
- 背景：32 路运行约 40 分钟后 19 路黑屏，dmabuf 925（正常 610~660），重启任务无效、
  重启进程立即恢复 —— BM(ion) 缓冲池耗尽，判断为 ISC 重连风暴 + 长时间预览叠加。
- 决定：`main.cpp` 起一个看门狗线程，每 30s 统计 dmabuf 句柄数与 VmRSS，
  超过阈值（默认 1500 / 2600MB，`AIDETECT_WATCHDOG_DMABUF`、`AIDETECT_WATCHDOG_RSS_MB` 可调）
  时告警并 `_exit(0)`，依赖 docker `--restart unless-stopped` 自动拉起。
- 代价：极端情况下会主动重启一次（约 40s 中断），换取不会长时间黑屏。
- 待办：定位 BM 缓冲泄漏点、给 ISC 重连限速。

### ADR-092 看门狗阈值收紧 + ISC 解码失败时轮换候选

- 日期：2026-11-14（迭代 53）
- 背景：连续运行 15h 后 dmabuf 1359、10 路 ISC 全部无画面；先前的 1500/2600MB 阈值高于实际故障点。
- 决定：
  1. 看门狗触发条件改为 dmabuf>1000 或 RSS>2000MB（可配），稳态 610~660；
  2. `start_isc_gateway` 支持候选轮换：解码器打开编码失败时 `++isc_rot_` 并 stop_gateway，
     下一轮从下一个候选开始，避免在同一条打不开的流上无限重试。
- 影响文件：`src/main.cpp`（阈值）、`src/task.hpp`（`isc_rot_`、候选旋转、失败处理）。

### ADR-093 失败重连按退避重试，抑制 BM 缓冲增长

- 日期：2026-11-14（迭代 54）
- 背景：生产 `app` 连续 39.5h 不重启、dmabuf 恒 133（池化复用，不累积）；
  我们稳态 610~660 且长时间运行会涨到 1359，拆解发现增长来自连不上的 ISC 相机的高频重连
  （每 10 分钟 28 次候选，每次失败泄漏少量 BM 缓冲）。
- 决定：解码器打开编码失败时 `++open_fail_count_` 并按 `open_backoff_ms()` 退避后再试；
  ISC 候选轮换保留。
- 效果：dmabuf 2.5 分钟内稳定 604~618 不增长；候选重试量降到 18 次/3min。
- 下一步：把预览/裁剪的 `vpp_resize` 改为每任务固定缓冲复用（对齐生产的池化做法）。

### ADR-094 推理输入与预览帧改为每任务池化复用

- 日期：2026-11-14（迭代 55）
- 背景：生产靠池化帧做到 39.5h 不重启、dmabuf 恒 133；我们每帧新建 BMImage，15h 涨到 1359。
- 决定：新增 `pooled_frame()` 按 (格式,宽,高) 缓存推理输入（u8 + float32），
  预览帧改为每任务一个 `preview_img_`，用 sail 的 dst 版 `vpp_resize` /
  `vpp_crop_and_resize_padding` 直接写入；工作线程用借用指针，不再转移所有权。
- 效果：dmabuf 稳定 700~730（不增长），RSS 1.0GB（原 1.9GB），检测/预览画面正确。
- 未完成：跟踪中间图与合流瓦片未池化；RSS 仍有 ~5MB/min 慢涨待查。

### ADR-095 跟踪中间图/合流瓦片也池化，热路径零按帧分配

- 日期：2026-11-14（迭代 56）
- 决定：`track_preview_boxes` 接收调用方提供的 scratch（`track_img_`），
  `render_tile_fast` 接收 handle 并使用 `tile_img_`；配合迭代 55 的推理/预览池化，
  热路径不再每帧分配 BMImage。
- 实测：dmabuf 713~744 持平、dmabuf 总大小恒 21.5MB、RSS 855MB（原 1.9GB）。
- 遗留：RSS 仍有 ~4MB/min 匿名内存增长（非 BM），由看门狗兜底，待定位。

### ADR-096 HEVC bsf 按候选临时启用，不得跨候选粘连

- 日期：2026-11-14（迭代 59）
- 背景：`gateway_bsf_hevc_` 曾是每任务粘性标志，一次 HEVC 判定会让后续 H.264 候选都带
  `-bsf:v hevc_mp4toannexb` 而立刻失败，导致多条 ISC 通道长期 connecting。
- 决定：每个候选开始前重置该标志；只在“该候选失败且日志确认 HEVC”时对同一 URL 重试一次并启用，
  重试后复位；同时 gateway 加 `-hide_banner`。
- 效果：预制场、排水渠1 恢复，connecting 从 5 降到 3。

### ADR-097 预览对齐生产：默认 5fps + 按源尺寸编码（取代 ADR-023/ADR-088）

- 背景：2026-11-19 用户要求"务必将我们盒子所有逻辑与生产盒子对齐"。生产
  `JpegEncoderInstance` 对单通道预览按**源尺寸**编码（2560×1440 仍是 2560×1440），
  实测 Web 播放约 3~5 fps；我方此前按 ADR-023 固定 10fps、按 ADR-088 默认 1080p 上限，
  属"知情差异"。
- 决策：默认 `AIDETECT_PREVIEW_INTERVAL_MS=200`（5fps）、预览目标=源尺寸（不缩放）；
  `AIDETECT_PREVIEW_WIDTH/HEIGHT` 仍可作上限（只缩不放）。
- 理由：设备侧 JPEG 已落地（`bmcv_image_jpeg_enc`），全尺寸编码不再走 CPU；
  预览只在有观看者时渲染，36 路同时观看不是常态，带宽影响可控。
- 取代：ADR-023（真内容 10fps）、ADR-088（默认 1080p 上限）。
- 代价：4K 单通道预览 JPEG 体积/带宽上升（10fps→5fps 抵消一半）；若前端反馈卡顿，
  设 `AIDETECT_PREVIEW_INTERVAL_MS=100` 或 `AIDETECT_PREVIEW_WIDTH/HEIGHT` 即可回退。

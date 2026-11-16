<!-- pm:meta
iteration: 39
updated: 2026-11-12
commit: -
-->

# 规范与本项目个性化配置

> 这里只写**和默认习惯不一样**的地方。默认习惯通常是对的，AI 出错大多错在"这个项目的特殊约定"上。

## 技术栈与版本

- 语言 / 标准：官方 demo 与 sail 源码按 C++14；**新服务端代码用 C++17**，不要为了复用 demo 把新代码降回 C++14。
- 编译器：g++ 9.4.0（盒子自带 `/usr/bin/g++`）。
- 构建：CMake 3.16.3 + GNU Make 4.2.1。
- SDK：libsophon 0.5.3、sophon-mw 0.15.0、sophon-sail 3.11.0（源码编译）。
- 控制面依赖：Boost 1.71（Beast/Asio，系统已装）、OpenSSL 1.1（JWT HMAC）、`nlohmann/json`（单头文件）。
- 硬件：BM1684X SoC，aarch64，Ubuntu 20.04 LTS，8 核 / 6.1 GiB。
- 模型：反光衣 `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel`，输入 640×640，2 类（穿反光衣 / 未穿反光衣），**输入 /255**；其余 5 个 Damo 模型 0-255。

## 环境搭建（步骤 1）

厂商包来自工作区 `reference/release/`，上传到盒子：

```
/data/CT/project/ai/gh-aibox-dev/env/packages/
  libsophon_soc_0.5.3_aarch64.tar.gz
  sophon-mw-soc_0.15.0_aarch64.tar.gz
  sophon-sail_3.11.0.tar.gz
```

`bash env/setup_sdk.sh` 做四件事（可重复执行）：

1. 解压 libsophon 到 `env/sdk/libsophon_soc_0.5.3_aarch64/`。
2. 解压 mw 到 `env/sdk/sophon-mw-soc_0.15.0_aarch64/`。
3. 解压 sail 源码到 `env/src/sophon-sail/`。
4. 原生编译并安装 sail：

```bash
cmake -S env/src/sophon-sail -B env/build/sail \
  -DBUILD_TYPE=pcie -DBUILD_PYSAIL=OFF -DCMAKE_BUILD_TYPE=Release \
  -DINSTALL_PREFIX=$PWD/env/sdk \
  -DLIBSOPHON_BASIC_PATH=$PWD/env/sdk/libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3 \
  -DFFMPEG_BASIC_PATH=$PWD/env/sdk/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-ffmpeg_0.15.0 \
  -DOPENCV_BASIC_PATH=$PWD/env/sdk/sophon-mw-soc_0.15.0_aarch64/opt/sophon/sophon-opencv_0.15.0 \
  -Dthe_libbmcv.so=$PWD/env/sdk/libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3/lib/libbmcv.so
cmake --build env/build/sail -j8
cmake --install env/build/sail
```

安装结果：`env/sdk/sophon-sail/{include/sail, lib/libsail.so, lib/cmake}`。

安装后脚本会修正 `SAILConfig.cmake` 的一处上游 bug：`.in` 模板里的 `${SAIL_LIB_DIRS}` 被 `configure_file()` 提前展开成空，导致 `find_package(SAIL)` 找不到 `libsail.so`。修正后下游 CMake 可以这样用：

```cmake
list(APPEND CMAKE_PREFIX_PATH "$ENV{SAIL_DIR}")  # 或 -DSAIL_DIR=...
find_package(SAIL REQUIRED)
target_include_directories(app PRIVATE ${SAIL_INCLUDE_DIRS} ${LIBSOPHON_HOME}/include)
target_link_directories(app PRIVATE ${SAIL_LIB_DIRS} ${LIBSOPHON_HOME}/lib)
target_link_libraries(app PRIVATE ${SAIL_LIBRARY} bmcv bmrt bmlib)
```

> `find_package(SAIL)` 只提供 sail 自己的路径；libsophon 的 include/lib 和 bmcv/bmrt/bmlib 仍需显式加，否则 `sail/engine.h` 会找不到 `bmlib_runtime.h`。

> `-Dthe_libbmcv.so=...` 不是可选优化。上游 native 分支漏了 bmcv 链接，不传会出现 44 个未定义 bmcv 符号且 `libsail.so` 的 `NEEDED` 里没有 `libbmcv.so.0`（见 ADR-003）。

验证步骤 1：

```bash
source env/env.sh
bash env/test_sail.sh
# 期望：SAIL_OK graph=yolov11s / input images shape=1,3,640,640 / output ... shape=1,6,8400
```

## 命令

| 用途 | 命令（在盒子 `/data/CT/project/ai/gh-aibox-dev` 下） |
|---|---|
| 首次安装 SDK | `bash env/setup_sdk.sh` |
| 加载环境 | `source env/env.sh` |
| sail 冒烟测试 | `bash env/test_sail.sh [bmodel]` |
| 编译官方 demo | `bash tests/yolov11_official_demo/build.sh` |
| 运行官方 demo | `bash tests/yolov11_official_demo/run.sh [图片目录或视频]` |
| 端到端验证 | `bash tests/yolov11_official_demo/verify.sh` |
| 编译 C++ 服务 | `cmake -S . -B build/aidetect -DCMAKE_BUILD_TYPE=Release && cmake --build build/aidetect -j8` |
| 运行 C++ 服务 | `source env/env.sh && ./build/aidetect/aidetect`（需 `AIDETECT_JWT_SECRET`） |
| 服务接口冒烟 | `curl -H "Authorization: Bearer <token>" http://127.0.0.1:9000/get_occupancy` |
| 构建 Docker 镜像 | `docker build -t aidetect:dev -f docker/Dockerfile .` |
| 运行 Docker | `docker run -d --name aidetect --privileged --network host -e AIDETECT_JWT_SECRET=... -v /data/CT/project/ai/models:/models:ro ... aidetect:dev` |
| Compose 部署 | `export AIDETECT_JWT_SECRET=... && docker compose -f docker/docker-compose.yml up -d` |
| Compose 状态 | `docker compose -f docker/docker-compose.yml ps` |
| 36 路压测 | 本地 640×360 视频 + 36 个 taskId；`AIDETECT_PREVIEW_INTERVAL_MS=500 AIDETECT_MAX_INSTANCES=2` |
| ISC mock 验证 | 本地 HTTPS mock `127.0.0.1:8443`，校验 `x-ca-signature` 后返回 RTSP；见 `docs/assets/isc_mock.log` |
| 告警视频验证 | RTSP 触发后 `ffprobe` 检查 `save/<task>/<model>/video/*.mp4`，注意 H.264/H.265 |
| 新 FP16 权重归一化探测 | `python3 tests/yolov11_official_demo/probe_model.py --model <bmodel> --image <jpg> --norm 255` |
| 只检查结果 | `python3 tests/yolov11_official_demo/check_results.py` |
| 链接检查 | `readelf -d env/build/sail/lib/libsail.so \| grep NEEDED` |
| 项目记忆校验（工作站） | `pwsh -NoProfile -ExecutionPolicy Bypass -File <skill>/scripts/check-project-memory.ps1 -Target gh-aibox-dev -DocsRoot docs` |

## 环境变量与端口

| 变量 | 默认值 | 必填 | 说明 |
|---|---|---|---|
| `SOPHON_SDK_ROOT` | `env/sdk` | 是 | SDK 根目录，由 `env.sh` 导出 |
| `LIBSOPHON_HOME` | `$SOPHON_SDK_ROOT/libsophon_soc_0.5.3_aarch64/opt/sophon/libsophon-0.5.3` | 是 | libsophon 头/库/bin |
| `FFMPEG_HOME` | `$SOPHON_SDK_ROOT/.../sophon-ffmpeg_0.15.0` | 是 | mw ffmpeg |
| `OPENCV_HOME` | `$SOPHON_SDK_ROOT/.../sophon-opencv_0.15.0` | 是 | mw opencv |
| `SAIL_HOME` | `$SOPHON_SDK_ROOT/sophon-sail` | 是 | sail 库/头文件 |
| `SAIL_DIR` | `$SAIL_HOME/lib/cmake` | 是 | CMake 查找 sail 用 |
| `MODEL` | `/data/CT/project/ai/models/BM1684X/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel`（优先）；测试副本/旧 FP16 回退 | 否 | `run.sh` 可覆盖的模型路径 |

- 服务端口：AI 服务 HTTP/WS 都在 `9000`；合流 id `all_1..all_4`。
- 服务端实际环境变量：`AIDETECT_JWT_SECRET`（必填，HS256 secret）、`AIDETECT_CONFIG_BASE`（默认 `http://10.10.206.68`；旧 `.25` 当前从盒子不可达）、`AIDETECT_REPORT_BASE`（默认 `http://127.0.0.1`）、`AIDETECT_MODEL_ROOT`、`AIDETECT_DATA_ROOT`、`AIDETECT_TMP_ROOT`、`AIDETECT_PORT`（9000）、`AIDETECT_FONT`、`AIDETECT_CONFIG_FILE`（仅本地 mock）、`AIDETECT_MAX_INSTANCES`（默认 2）、`AIDETECT_MODEL_IDLE_SECONDS`（默认 120）、`AIDETECT_MAX_HIGHRES_TASKS`（默认 12）、`AIDETECT_PREVIEW_WIDTH/HEIGHT`（可选，默认按源 1080p 只缩不放）、`AIDETECT_TILE_WIDTH/HEIGHT`（默认 640×360）、`AIDETECT_PREVIEW_INTERVAL_MS`（默认 100，即 10fps 上限）、`AIDETECT_NO_PREVIEW`（关闭预览编码，压测用）、`AIDETECT_ALARM_RING`（1=multi 模式用时间戳包环，默认 0）、`AIDETECT_REC_RING_MB`（包环上限，默认 32 MB）、`AIDETECT_ISC_PROTOCOL`（强制 isc 协议，默认用 URL 的 protocol）、`AIDETECT_ISC_RTMP_PROXY`（rtmp host:port 转发）、`AIDETECT_ISC_ALLOW_DIRECT_RTMP`（允许直连 1935）、`AIDETECT_ALARM_BIAS_MS`（包环取窗相对告警时刻的偏移，网关模式默认 0）、`AIDETECT_DEBUG`（开启 `/debug/trigger_alarm`、`/debug/scheduler`）、`AIDETECT_NO_DUMP`/`AIDETECT_DISABLE_AGG`（仅测试）。
- 模型注册表：`src/model.hpp` 的 `make_model_specs()`；`reflectivejacket` 是 YOLOv11 FP16 `/255`，其余是 Damo FP16 0-255；救生衣路径 `_lifejacket_2`。新增模型必须同时补类别/目标/告警文案/归一化。
- 告警视频：只用 `decoder.dump`（H.264/H.265），调用后必须继续读 post 秒；路径字符串必须用成员保存（见 KNOWN-ISSUES TRAP-010/011）。
- 合流：`bmcv.imdecode/resize/image_copy_to/imencode`；copy 前必须把 tile 格式转成 canvas 格式（TRAP-009）。
- Compose：已安装 v2.29.7；`docker-compose.yml` 需要 `AIDETECT_JWT_SECRET` 环境变量。
- 认证约定：AI **只验证**入站 JWT；出站调后端用 `create_jwt("admin")` 自己签；不实现 `/get_token`、`/refresh_token`。
- `.env`：暂无；凭证不得写入仓库，运行时通过环境变量或外部配置注入。

## 代码规范

- 命名：C++ 文件/类型用 `snake_case` 或上游风格；新增服务端代码统一 `snake_case` 文件名、`CamelCase` 类型。
- 命名空间：新服务端代码放在 `aidetect::` 下；不同模块用 `aidetect::model`、`aidetect::task` 等子命名空间。
- 线程：一个 `sail::Engine` 只能在一个 `ModelInstance` 线程里串行 `process`；禁止多线程并发用同一个 Engine。
- 错误处理：C++ 异常只在模块边界抛出/捕获；接口层返回结构化错误，不把异常文本直接透给前端。
- 日志：禁止打印密码、token、视频 URL 中的凭证；模型路径、taskId、帧号可以打印。
- 注释：构建参数、非上游行为、硬件限制必须写注释；显而易见的代码不写废话注释。
- 编码：源码 UTF-8；官方 demo 的 `cv::putText` 只支持 ASCII，验证时用 `reflectivejacket_en.names`，生产服务需用 FreeType/BM CV 画中文。

## 测试策略

- 必须有测试：SDK 能加载模型（`test_sail.sh`）；官方 demo 能出检测框（`verify.sh`）；改动构建参数后重跑这两条。
- 新模型接入前必须跑 `probe_model.py` 对比 `--norm 1` 和 `--norm 255`；禁止凭印象选归一化。
- 必须人工看：`docs/assets/step2_yolov11_detection.jpg` 这类输出图，确认框落在人身上而不是背景。
- 暂不要求覆盖率；服务端代码开始后，至少给任务状态机、模型调度、告警判定加单测。
- 回归命令固定为 `bash tests/yolov11_official_demo/verify.sh`，输出必须包含 `PASS: demo produced detections`。
- 设计文档（`INTERFACES`/`SERVICE_TASKS`/`SOLUTION`）变更后，在 `PROGRESS` 写清验证方式（与源码/实测对齐）。

## 禁区 / 生成文件 / 密钥

- 不要手改：`env/sdk/`、`env/build/`、`env/src/`（`setup_sdk.sh` 生成）。
- 不要提交：`env/packages/*.tar.gz`、`env/sdk/**`、`tests/**/models/*.bmodel`、`tests/**/bin/**`、`tests/**/results/**`。
- 不要碰：`/opt/sophon`（原厂运行时，旧 Python 服务依赖）。
- 绝不提交/打印：盒子 SSH 密码、Web 账号密码、GitHub token、`AIDETECT_JWT_SECRET`/旧 `JWT_SECRET`。
- 不要把旧 Python 的 `ab=[1,1,...]`（0-255）照搬到 FP16 YOLO11；反光衣必须 `/255`。
- 不要重新引入“一任务一进程/一任务一模型”的旧结构（SOLUTION ADR-007/008）。
- 不要为了回放 MP4 改线上通道 `tb_channel.address` 再重启容器：会把 PID1
  变成 zombie 并残留 9000 端口（KNOWN-ISSUES KI-029）。要回放就建独立
  临时通道/任务，或直接用离线片段验证。

## 非典型做法及原因

- SDK 自包含在 `env/sdk/` 而不是装到 `/opt/sophon`：避免破坏原厂运行时，也方便后续打进 Docker（ADR-001）。
- 在盒子上原生编译 sail，而不是 x86 交叉编译：当前没有可用的 x86 Linux 交叉环境，盒子有 g++/cmake（ADR-002）。
- native 构建显式传 `-Dthe_libbmcv.so=...`：上游脚本漏链 bmcv（ADR-003）。
- 步骤 2 用 FP16 YOLOv11 而不是盒子上的 INT8 反光衣模型：INT8 分类输出全 0，FP16 同结构正常（ADR-004 / KI-001）。
- 官方 demo 源码不改，差异放 `build.sh`/`run.sh`：便于对照上游（ADR-005）。
- 项目记忆文档放 `docs/` 而不是 `docx/`：遵循"其他 md 放 docs"的要求（ADR-006）。
- 新服务单进程 + 共享模型调度，而不是一任务一进程：避免跨进程传图和重复加载（ADR-007）。
- 4 个解码/合流组 ×9 路，而不是全局 36 路一个池：与 4 路合流对齐，固定槽位（ADR-008/011）。
- 迭代 4 MVP 先用线程/连接同步 Beast（ADR-014），不是最终 async；36 路 WS 要换。
- 迭代 4 MVP 只做一个共享反光衣 `ModelService`（ADR-013），不是最终多实例调度。
- 迭代 4 镜像基于 `aibox-api:latest` + 拷贝 OpenSSL 1.1（ADR-015），不是理想 ubuntu 20.04 基础镜像。
- 反光衣先用 FP16、INT8 暂缓：INT8 分类列全 0（ADR-009 / KI-001）。
- 控制面用 Boost.Beast，不引入 Drogon/Crow：系统已有 Boost 1.71/OpenSSL，减少镜像依赖（ADR-010）。

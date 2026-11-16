<!-- pm:meta
iteration: 5
updated: 2026-11-02
commit: -
-->

# 接口清单（前后端 ↔ AI 检测服务端）

> 依据：`/data/CT/project/ai/inference` 旧源码、负责人确认、需求文档、`reference/reference_image/*.png`。
> 线上入口已确认：**`app.py` + `main.py` + `ai_package/`**（`app.py` 的 import 链）。`task_manager.py`、`package/`、`run_all.sh` 是旧代码，不再作为依据。
> 已确认：token 由前端/后端自行签发，AI 只验证；WS 固定 `9000` + `all_1..4`；告警图片/视频固定 `/data/CT/project/ai/inference/save` 绝对路径。

## 0. 页面 → AI 服务职责

| 页面 | AI 服务需要做什么 | 对应接口 |
|---|---|---|
| 首页 | 返回 CPU/内存/磁盘/TPU/VPU/VPP/温度 | `GET /get_occupancy` |
| 通道管理 | 不参与（后端存参数）；任务启动时消费 `channelAddress` | 后端配置接口 |
| 任务管理 | 启动/停止任务、拉任务配置、返回最新帧给配置区域 | `/task/start`、`/task/stop`、`/task/get_area_frame`、`TaskAlgoConfigDetail` |
| 实时预览 | 输出任务通道 + 4 路合流通道的检测图片流，≤10fps | `WS /ws/frame` |
| 告警预览 | 告警图片 + 前后 10 秒视频 + 上报 JSON | `ReportAlarmEvent` |
| 参数配置/其他 | 后端/前端使用；AI 服务只按任务配置生效 | 后端配置接口 |

## 0.1 接口速览

| 接口 | 提供方 | 旧实现 | 新实现 |
|---|---|---|---|
| `GET /get_occupancy` | AI 服务 | `app.py:643` | 保持一致 |
| `POST /task/start` | AI 服务 | `app.py:149` | 保持一致（`pid` 用服务 PID） |
| `POST /task/stop` | AI 服务 | `app.py:206` | 保持一致 |
| `GET /task/list` | AI 服务 | `app.py:244` | 保留兼容 |
| `GET /task/get_area_frame` | AI 服务 | `app.py:604` | 保持一致 |
| `WS /ws/frame` | AI 服务 | `app.py:715` | 保持一致（9000、`all_1..4`） |
| `GET TaskAlgoConfigDetail` | 后端 | `params_init.py:82` | 保持一致 |
| `POST ReportAlarmEvent` | 后端 | `Yolo.py:651` 等 | 保持一致（绝对路径） |

> AI 服务**不提供** `/get_token`、`/refresh_token`；前端/后端自己签发 token，AI 只验证。AI 服务调用后端时自己 `create_jwt("admin")`。

## 0.2 实现状态（迭代 5 MVP+）

| 接口/能力 | 状态 | 验证 |
|---|---|---|
| `GET /get_occupancy` | 已实现 | 真实盒子返回 `status=ok`，字段 `vpu_1..3/vpp_1..2`，异常 vpu/vpp 值钳制 0 |
| `POST /task/start` | 已实现（5 类模型；ISC 解析） | 真实任务 `6488...` 返回 `status=ok,pid=<服务PID>`；ISC mock 任务启动成功 |
| `POST /task/stop` | 已实现 | 停止后 `/task/list` 返回 `{"running":[]}` |
| `GET /task/list` | 已实现 | 返回任务状态/group/slot/models |
| `GET /task/get_area_frame` | 已实现 | 54 万字节 JPEG（RTSP 任务） |
| `WS /ws/frame` 任务流 | 已实现 | 54 万字节 JPEG，header 含 `imageType`/`ImageType` |
| `WS /ws/frame` `all_i` | 已实现（4 路） | 36 路压测下 `all_1..4` 均 1920×1080 JPEG（约 676 KB） |
| `POST ReportAlarmEvent` | 已实现（RTSP） | 图片+视频 JSON；后端告警详情 `videoPath` 指向 9.64s H.264 文件 |
| JWT 验证 | 已实现 | 缺失/错误签名 401；有效 token 通过 |
| ISC | 已实现，mock 验证 | 本地 HTTPS mock `sig_ok=True`，解析出 RTSP；真实 `10.10.69.250` 网络 refused |
| 模型多实例 | 已实现扩容（无回收） | 36 路日志 2 个 `safetyhelmet` 实例 |
| 36 路 / 10fps | 部分 | 36 路 640×360、1fps 检测、2fps 预览稳定 30s；1080p 默认准入 12 路，超出 error（ADR-022） |

> 当前实现细节以 `src/server.hpp`、`src/task.hpp` 为准；本表只标状态，不改变上面的接口契约。

## 1. 认证与 Token

### 1.1 入站：AI 只验证

- 所有 HTTP 接口：`Authorization: Bearer <token>`；WS：`?token=<token>`。
- 验证参数：`HS256`、`iss=HNKC.AIBOX`、`aud=HNKC.AIBOX`、7 天有效期。
- 缺失/格式错误/签名错误/过期/iss/aud 错误 → HTTP 401；WS 关闭 1008/4001（旧实现两种行为，见 1.3）。
- 旧 `verify_token` 读取 claim：
  `payload["http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name"]`。
  新实现应同时接受该长 claim 和 `user_id`，避免前端/后端签发格式差异导致 401。
- `JWT_SECRET` 旧源码硬编码多处；新实现从 `AIDETECT_JWT_SECRET` 或只读文件读取，不提交仓库。

### 1.2 出站：AI 服务自己签发

AI 调后端 `TaskAlgoConfigDetail`、`ReportAlarmEvent` 时需要自己生成 token：

```text
create_jwt(user_id="admin")
payload = {user_id, iss, aud, exp=now+7d, iat=now}
HS256 + JWT_SECRET
```

- 这是旧 `ai_package/utils.py:61` 的行为，新实现保留。
- 不需要 `/get_token`/`/refresh_token`，也不需要用户库。

### 1.3 WS token 过期行为

- 旧 `app.py`：先 `accept()`，再发 `{"type":"auth_expired","message":"token 已过期，请重新登录"}`，再 close 4001。
- 旧 `ai_package/server.py`：直接 close 1008。
- 新实现建议统一为旧 `app.py` 的行为（前端可见提示）；无效签名仍直接 close 1008。

## 2. AI 服务对外 HTTP 接口

### 2.1 `GET /get_occupancy`

旧实现 `app.py:643`，返回：

```json
{
  "status": "ok",
  "msg": "返回资源占用率值",
  "cpu": 0,
  "mem": 0,
  "disk_free": 0.0,
  "tpu": 0,
  "vpu_1": 0, "vpu_2": 0, "vpu_3": 0,
  "vpp_1": 0, "vpp_2": 0,
  "memory": 0,
  "chip_temp": 0.0,
  "board_temp": 0.0
}
```

- `cpu`/`mem`：`psutil` 等效（`/proc`），取整百分比。
- `disk_free`：`/data` 剩余 GB，保留 2 位。
- `tpu`/`vpu`/`vpp`：`sail.get_tpu_util` / `get_vpu_util` / `get_vpp_util`。
- **`vpu` 是 3 个值（`vpu_1..3`）；`vpp` 只有 2 个值（`vpp_1..2`），没有 `vpp_3`**。
- `memory`：`sail.get_dev_stat(0)` 的 used/total*100。
- `chip_temp`/`board_temp`：`sensors` 的 `temp1/temp2` 或 `sail.get_chip_temp/get_board_temp`，可能 `null`。
- 旧代码没有 `device_id`；首页"设备标识"来自后端。

### 2.2 `POST /task/start`

```http
POST /task/start
Authorization: Bearer <token>
Content-Type: application/json

{"taskId": "<task-uuid>"}
```

返回（保持旧字段）：

```json
{"status": "ok", "msg": "任务 <taskId> 已启动", "pid": 12345}
```

- 旧行为：已在运行 → `{"status":"error","msg":"任务 xxx 已经在运行中"}`；端口耗尽 → error；启动 `python3 main.py` 子进程。
- 新实现单进程，没有每任务进程；`pid` 返回**服务进程 PID**（同一值），字段保留以兼容前端。前端当前只用 `status/msg`，不要用 `pid` 做 kill。
- 新实现：创建 `TaskContext`，返回 `state:starting/running`；重复启动保持旧 error 或按前端约定幂等。
- 端口分配：新实现不再需要 9011~9046 的任务子进程端口；合流组内通信走进程内队列。

### 2.3 `POST /task/stop`

```http
POST /task/stop
Authorization: Bearer <token>
Content-Type: application/json

{"taskId": "<task-uuid>"}
```

返回：`{"status":"ok","msg":"任务 <taskId> 已停止"}`；不存在返回 `status:error`。新实现走 `stopping → stopped`，释放解码器、模型引用、合流槽位、告警资源。

### 2.4 `GET /task/list`（旧接口，保留兼容）

旧返回：

```json
{"running": [{"task": "<taskId>", "pid": 12345}]}
```

新实现返回 `state`，`pid` 用服务 PID 或省略；前端如果不用可忽略。

### 2.5 `GET /task/get_area_frame?taskId=...`

```json
{"status": "ok", "msg": "返回任务 <taskId> 的配置区域图片", "frame_area": "<base64-jpeg>"}
```

- 失败：`{"status":"error","msg":"任务 xxx 的配置区域图片不存在","frame_area":null}`。
- 旧实现从 `frame_dict[taskId]` 取最新 JPEG；任务未启动/尚未出帧时失败。
- 新实现：每任务保存 `latest_preview_jpeg`（shared_ptr），原子读取；未运行/无帧返回错误。

## 3. WebSocket 帧流

### 3.1 连接

```text
ws://<box>:9000/ws/frame?token=<jwt>
```

- **固定 9000**（旧 `app.py:927` 与 `ai_package/server.py` 都是 9000）；不再考虑 8000。
- token 校验同 1.1/1.3。

### 3.2 订阅协议

- 客户端连接后发送文本消息：任务 `taskId`，或合流通道 `all_1`/`all_2`/`all_3`/`all_4`。
- 发送 `heartbeat` 仅保活；旧实现不返回订阅 ack；切换订阅就是再发一个 id。
- 合流 id **固定 `all_1..all_4`**（依旧源码）；不再使用 `aggregate_i`。

### 3.3 二进制封包

```text
4 字节 header 长度（大端 uint32） + header(JSON, UTF-8) + image raw bytes
```

```json
{"taskId": "<taskId-or-all_i>", "imageType": "jpeg", "dataLength": 12345}
```

- 旧源码实际输出 **`imageType`（小写 i）**；需求文档写 `ImageType`。新实现建议**同时写 `imageType` 和 `ImageType` 两个键**（值相同，几字节开销），或默认小写并兼容解析两种；前端不用改。
- `dataLength` 是图片字节数，不含 header；图片固定 JPEG。

## 4. AI 服务调用的后端接口

### 4.1 任务配置

```http
GET http://10.10.206.25/aibox/api/ai/TaskAlgoConfigDetail?task_id=<taskId>
Authorization: Bearer <jwt>
Content-Type: application/json
```

实际响应（`ai_package/params_init.py`）：

```json
{
  "code": 200,
  "data": {
    "channelAddress": "rtsp://... 或 isc://...",
    "frameInterval": 5,
    "detectThreshold": 4,
    "detectSection": 10,
    "videoDuration": 10,
    "cooldownTime": 3,
    "model": [
      {
        "algoCode": "safetyhelmet",
        "taskArea": [{"points": "[{\"X\":0.1,\"Y\":0.1}, ...]"}],
        "algoConfig": [
          {"code": "conf_thresh", "configValue": "0.6"},
          {"code": "nms_thresh", "configValue": "0.7"},
          {"code": "exceed_num", "configValue": "1"}
        ]
      }
    ]
  }
}
```

映射：

| 后端字段 | 内部字段 | 说明 |
|---|---|---|
| `channelAddress` | `url` | `rtsp://` 或 `isc://` |
| `frameInterval` | `frame_interval_detect` | 旧实现按帧数用；单位需以真实后端为准 |
| `detectThreshold` | `detect_threshold` | 窗口内触发帧数 |
| `detectSection` | `detect_section` | 滑动窗口帧数 |
| `videoDuration` | `video_duration` | 告警视频秒数 |
| `cooldownTime` | `cooldown_time` | 后端单位是**分钟**，旧实现 `*60` 转秒 |
| `algoCode` | `model_name` | 6 种模型之一 |
| `taskArea[].points` | `polygon_points` | JSON 字符串，归一化 0~1；乘实际视频宽高 |
| `algoConfig[].code` | `conf_thresh/nms_thresh/exceed_num` | 字符串转 float/int |

旧问题：`get_task_config` 在构造函数里无限重试、没有总超时；`taskArea` 缩放可能早于真实分辨率；每 5 秒轮询。新实现要加超时/退避、分辨率就绪后再缩放、配置 diff。

### 4.2 告警上报

```http
POST http://0.0.0.0/aibox/api/ai/ReportAlarmEvent
Authorization: Bearer <jwt>
Content-Type: application/json
```

```json
{
  "task_id": "<taskId>",
  "model_name": "reflectivejacket",
  "report_content": "未穿反光衣",
  "report_num": "1",
  "time": "2026-11-02 14:30:00",
  "image": "/data/CT/project/ai/inference/save/<taskId>/reflectivejacket/img/20261102_143000.jpg",
  "video": "/data/CT/project/ai/inference/save/<taskId>/reflectivejacket/video/20261102_143000.mp4"
}
```

- **路径固定绝对路径**：`/data/CT/project/ai/inference/save/...`；后端当前按这个根目录读取。新服务用 `DATA_ROOT=/data/CT/project/ai/inference/save`，部署验证成功后再统一改路径。
- 旧代码传 `save/...` 相对路径，是因为工作目录就是 `/data/CT/project/ai/inference`；新服务显式拼绝对路径，避免依赖 cwd。
- `report_num` 是字符串；`time` 本地时间 `%Y-%m-%d %H:%M:%S`。
- 旧实现 timeout 5s、失败只记日志；新实现有限重试 + 本地待上报队列（可选）。
- 视频旧实现 `decoder.dump(5,7,...)`（前 5/后 7 秒），需求前后 5 秒；新实现按 `video_duration`（默认 10 秒，5+5）。

## 5. 数据格式与内部契约

### 5.1 检测结果

旧 `det` 是 `N×7`：`[x1,y1,x2,y2,score,?,class_id]`，`-3` 是 score，`-1` 是 class_id。新实现用强类型 `Detection {x1,y1,x2,y2,score,class_id}`。

### 5.2 模型注册表（新实现）

| `model_name` | 权重（FP16，现行） | 类别 | target | report_content | exceed_num | 输入归一化 |
|---|---|---|---|---|---|---|
| `person` | `damoyolo_tinynasL25_S_person.bmodel` | `['人']` | 0 | 人员聚集 | 3 | 0-255 |
| `cigarette` | `damoyolo_tinynasL25_S_cigarette.bmodel` | `['抽烟行为']` | 0 | 抽烟行为 | 1 | 0-255 |
| `lifejacket` | `damoyolo_tinynasL25_S_lifejacket_2.bmodel` | `['穿救生衣','未穿救生衣']` | 1 | 未穿救生衣 | 1 | 0-255 |
| `safetyhelmet` | `damoyolo_tinynasL25_S_safetyhelmet.bmodel` | `['戴安全帽','未戴安全帽']` | 1 | 未戴安全帽 | 1 | 0-255 |
| `reflectivejacket` | `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel` | `['穿反光衣','未穿反光衣']` | 1 | 未穿反光衣 | 1 | **/255** |
| `reflectivejacket_sky` | `damoyolo_tinynasL25_S_fp16_reflectivejacket_sky_1.bmodel` | `['穿反光衣','未穿反光衣']` | 1 | 未穿反光衣 | 1 | 0-255 |

> 迭代 5 的 C++ 实现把这 6 个注册项集中在 `src/model.hpp` 的 `make_model_specs()`；告警视频仅支持 H.264/H.265 输入，5+5 秒；`video` 字段在视频生成后才写入。

### 5.3 路径与文件命名

- 告警图：`/data/CT/project/ai/inference/save/<taskId>/<model_name>/img/YYYYmmdd_HHMMSS.jpg`
- 告警视频：`/data/CT/project/ai/inference/save/<taskId>/<model_name>/video/YYYYmmdd_HHMMSS.mp4`
- 临时分段/转存：`/data/CT/project/ai/inference/video_transtation/<taskId>/...`
- 新服务用 `DATA_ROOT` 配置，默认上述绝对路径。

## 6. 仍建议确认的点（非阻塞）

1. `frameInterval` 的真实单位：旧实现当帧数用，后端可能按毫秒/秒；以真实后端 fixture 为准。
2. `cooldownTime` 旧实现按分钟 `*60`；确认后端确实传分钟。
3. `/task/list` 前端是否使用；新实现保留兼容。
4. header 键：新实现同时写 `imageType`/`ImageType`，前端用哪个都能读。
5. `pid`：新实现返回服务 PID；前端如果只用 `status/msg` 则无影响。

## 7. 建议的验证方式

- JWT：有效/过期/错签/错 iss/aud/只有 `user_id`/只有 .NET claim。
- `curl` mock 后端：`/task/start`、`/task/stop`、`/get_occupancy`、`/task/get_area_frame`。
- Python `websockets` 客户端：连 9000，发 taskId/`all_1`/heartbeat，解析 4 字节长度 + JSON + JPEG，校验 `FF D8`。
- 告警：mock 后端收到绝对路径，文件真实存在；图片带框、视频无框。
- 所有接口测试再在 Docker 内跑一遍。

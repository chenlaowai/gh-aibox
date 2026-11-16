# ISC 视频流：生产方案逆向与解析重构

> 需求：在同样网络策略下，我们的盒子（206.68）不应必须经 206.25 代理，且解析成功、
> 有画面的路数应接近生产盒子（207.46）。生产盒子全程只读，未修改任何文件/配置。

## 1. 生产方案（逆向结论）

### 1.1 生产 DB 中的 ISC 地址

`/data/app/edge.db` 的 `table_media` 每一路是：

```text
isc://24503247:DE79WQZmDJ1eD1TN1rRE@10.10.69.250:443
     ?cameraIndexCode=<camera>&streamType=0
     &protocol=rtmp&https=1&videotype=h264&transcode=1
```

同一台设备的 `appKey:appSecret`、ISC 主机 `10.10.69.250:443` 与我们的后端配置完全一致。
差别在 URL 参数：生产请求 **`protocol=rtmp` + `transcode=1` + `videotype=h264` + `https=1`**。

### 1.2 生产代码证据

`libEdgeSystem.so`：

```text
/artemis/api/video/v1/cameras/previewURLs
"protocol": "%s",
"streamType": %d,
transcode=%s&videotype=%s
enable_rtmp=1
mk_rtmp_server_start
port=1935
stream_changed_schemas=rtsp/rtmp/fmp4/ts/hls/hls.fmp4
```

生产的网络行为：`ss -tn` 显示大量 `10.10.207.46:* -> 10.10.69.250:1935` 的 **RTMP**
长连接；没有到相机 554 的直接连接。即生产是"Artemis 取流地址 → **经 ISC 的 RTMP
流媒体网关**（1935）拉流 → 本地解码/录制/转 RTMP/WebRTC 给网页"。

### 1.3 实测 ISC 返回的 URL

用生产同一 appKey/secret 从本机（经 .25 的 HTTPS 代理）请求 Artemis：

| 请求 protocol | 返回 URL |
|---|---|
| rtsp | `rtsp://10.10.69.250:554/openUrl/<token>` |
| rtmp | `rtmp://10.10.69.250:1935/live/openUrl/<token>` |

两种都能返回；`transcode=0/1` 对本相机 URL 形态无影响。ISC 返回的本来就是网关地址，
不是相机直连地址。

## 2. 我们旧实现的问题

1. `IscResolver::resolve()` 把请求体写死为 `{"protocol":"rtsp"}`，完全忽略后端 URL 里的
   `protocol=rtmp&transcode=1&videotype=h264&https=1`。后端/生产都要求 RTMP，我们却一直
   请求 RTSP。
2. 只解析了 `cameraIndexCode`/`streamType`，丢掉了 `transcode`/`videotype`/`https`。
3. ISC 的 `openUrl` 是一次性会话，但断流重连时一直复用旧 URL；会话过期后就是
   "Failed to open input file / 没有画面"。
4. 每个任务同时起 `sail::Decoder`（一条 RTSP）和 `ffmpeg` 环形录制（**第二条 RTSP**），
   两个 ISC 会话抢同一路相机；很多相机/ISC 会话只允许一路，结果解码或录制被踢掉。
5. 206.68 到 ISC 的网络被对端 RST（443/554/1935 都不通），必须经 206.25 的 TCP 转发；
   这是网络 ACL 问题，不是解析代码能绕过的（见 `NETWORK_REPORT_25_vs_68.md`）。

## 3. 重构方案（已落地）

`src/isc.hpp`：

- 解析 `isc://` 查询参数：`protocol`、`transcode`、`videotype`、`https`、`streamType`；
- Artemis 请求体按 URL 参数转发：
  ```json
  {"cameraIndexCode":"...","streamType":0,"protocol":"rtmp",
   "transcode":1,"videotype":"h264","https":1}
  ```
- `AIDETECT_ISC_PROTOCOL` 可强制协议（206.68 当前设为 `rtsp`，因为到 1935 不通）；
- `AIDETECT_ISC_RTMP_PROXY=host:port` 可把 `rtmp://10.10.69.250:1935/...` 重写到
  本地 RTMP 转发（保留 app/path）；
- `AIDETECT_ISC_ALLOW_DIRECT_RTMP=1` 允许在能直连 1935 的网络里按 URL 的 rtmp 请求；
- 单解码模式下每任务一个 ffmpeg **网关**只建一条上游 RTSP/ISC 连接，`-c copy` 复制为
  两路本地 MPEG-TS/UDP：`sail::Decoder` 一路、告警包环一路；告警不再单独连相机；
- 断流重连时对 `isc://` 重新 `resolve()`，重启网关并重启包环。

## 4. 验证

| 项 | 结果 |
|---|---|
| ISC 解析日志 | `ISC url protocol=rtsp`、`ISC RTSP target 10.10.69.250:554 via proxy 10.10.206.25:10444`、`t_isc2 ISC resolved to rtsp` |
| ISC 实际出图 | `t_isc2`（camera `1aae7f6a...`，1280×720）单通道 **9.88fps**，79/79 不同帧，重复帧 0 |
| 25K 相机 | Artemis 解析成功，但 RTSP 网关返回 `500 Internal Server Error`（ISC/相机侧） |
| 连接数 | 单模式每任务只建一条上游（网关 fan-out 本地两路）；旧实现为解码 + 录制两条 |
| 30 路容器最终 | **7 路 decoder opened = 6 路 ISC + 1 路直连 RTSP**；其余 ISC 相机离线/`500`/`missing data.url` |
| 生产侧 | 生产到 `10.10.69.250:1935` 大量 ESTAB；本机到 443/554/1935 全部 CLOSED |

证据图片：`docs/assets/isc_live_1280x720.jpg`。

## 5. 为什么还需要 .25 代理 / 怎么彻底去掉

实测（206.68）：

```text
10.10.69.250:1935 CLOSED
10.10.69.250:443  CLOSED
10.10.69.250:554  CLOSED
10.10.206.25:10443/10444 OPEN
```

生产（207.46）到 `10.10.69.250:1935` 正常。206.68 的 SYN 被对端/中间设备先 RST 再
SYN-ACK，属于 ISC 侧 ACL/会话策略差异。代码无法绕过三层 ACL；需要网络/安全运维：

1. 放行 `10.10.206.68 -> 10.10.69.250:443/554/1935`（按 .25 的放行策略）；
2. 或允许在 .25 上增加 `10445 -> 10.10.69.250:1935` 的 TCP 转发，并在 .68 设置
   `AIDETECT_ISC_RTMP_PROXY=10.10.206.25:10445`、`AIDETECT_ISC_PROTOCOL=rtmp`；
3. 放行后把 `AIDETECT_ISC_PROTOCOL` 改为 `rtmp`（或去掉，使用后端 URL 的 protocol），
   再复测 30 路成功/出图数量。

> 当前容器显式设置 `AIDETECT_ISC_PROTOCOL=rtsp`，是为了在 1935 不通时保留可用的
> RTSP 路径；这不会改生产盒子，也不改变后端 URL。

## 6. 与生产的差异/遗留

| 项 | 生产 | 我们 |
|---|---|---|
| 协议 | RTMP（1935） | 解析层已支持；206.68 因网络暂用 RTSP（554） |
| 网络 | 直连 ISC | 经 .25 的 443/554 转发 |
| 连接数 | 每路一条媒体源 | 单模式网关一条上游（已对齐）；multi 仍可能两条 |
| 会话刷新 | 按需重新取 previewURL | 断流重连时重新 resolve（已加） |
| 播放 | 本地 RTMP 服务 + WebRTC/FLV | 前端 WS JPEG（协议不变） |

后续：网络放行/加 1935 转发后切 RTMP，复测 30 路出图率与 4 组合流。

### 5.1 协议/端口补充探测（2026-11-05 第三轮）

对 camera `1aae7f6a...` / `23595aa2...` 请求 Artemis，返回：

| 请求 protocol | 返回 URL |
|---|---|
| rtsp | `rtsp://10.10.69.250:554/openUrl/<token>` |
| rtmp | `rtmp://10.10.69.250:1935/live/openUrl/<token>` |
| hls | `http://10.10.69.250:83/openUrl/<token>/live.m3u8` |
| fmp4 | 空 |

从 206.68 直接连 `10.10.69.250`：**83/554/1935/443 全部 Connection refused**；
206.25 初始只开放 **10443/10444**；2026-11-05 21:01 已增加 **10445 → 1935**。HLS(83) 仍不可达，RTMP 已可用。
两条备选路径同样被 ACL 挡住，无法用代码绕过。
### 5.2 RTMP 路径验证（临时隧道，仅验证，不是部署）

为确认“只要有一个 1935 转发，现有代码就能走生产同款 RTMP”，临时用 SSH 隧道
（本机 10445 → 生产 → `10.10.69.250:1935`，以及反向到 206.68）做了隔离验证：

- `ffprobe` 直接读 RTMP：`h264, 1280x720, 25/1`；
- 单任务 `t_isc2`（camera 1aae）：日志
  `ISC url protocol=rtmp`、`ISC RTMP target 10.10.69.250:1935 via proxy 127.0.0.1:10445`，
  `decoder opened shape=1,3,720,1280`；
- 隔离预览 10s：84 包 / 68 不同帧 / **8.5fps** / 1280×720 / 重复帧 0。

结论：**代码侧 RTMP + host:port 重写是通的**。2026-11-05 21:01 已在 206.25
启动 `isc_rtmp_proxy.py`（10445 → 1935），.68 已能直连 10445。隧道仅用于验证。
### 5.3 并发对比：代码路径正常，ISC/.25 并发受限

隔离测试（容器停止、仅 1 个 ISC 任务，RTSP 经 .25:10444）：

- `ISC url protocol=rtsp` → `decoder opened shape=1,3,720,1280`；
- 8s 预览：78 包 / 58 不同帧 / **9.67fps** / 1280×720 / 重复帧 0。

同一份代码在 30 路容器中，早期因网关探测过短（2×2.5s）只开到 7 路直连、ISC 0 路；
把探测放宽到 **3×4s** 并加指数退避后，30 路恢复到 **6 路 ISC + 1 路直连**。
### 5.4 最终部署结果（30 路，RTSP + 更长网关探测）

30 路容器最终：**7 路 decoder opened = 6 路 ISC + 1 路直连 RTSP（6488）**；
另外 23 路为 ISC 相机离线/`500`/`missing data.url`。

| 通道 | 协议 | 预览 |
|---|---|---|
| fefe86ed（1aae） | ISC RTSP 经 .25:10444 | 1280×720 / **9.83fps** / 59 不同帧 |
| 6488 | 直连 RTSP | 1920×1080 / **10fps** / 60 不同帧 |
| 02109e（隔离测试） | ISC RTSP，HEVC 3200×1800 | decoder opened，预览 1920×1080 / 4fps |

关键修复：网关探测从 2 次×2.5s 放宽到 **3 次×4s**，网关失败改为指数退避；
重编码/高分辨率 ISC 相机（如 3200×1800 HEVC）不再被误判为“无流”。

RTMP 对比：`10445 → 1935` 已通，单路 RTMP 1280×720/8.5fps；但 30 路同时 RTMP
时只有 1 路 ISC 出图（ISC RTMP 会话/转码限制），因此当前部署仍用 **RTSP 为主**，
RTMP 作为后续网络/会话容量改善后的切换项。
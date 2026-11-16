# 告警视频：生产方案逆向与时间对齐重构（最终版）

> 需求：告警图片时间点应位于告警视频时间段的正中（前后各 5 秒，共 10 秒）；
> 旧实现出现"图片 18:57:24、视频 18:57:10–18:57:20"这类明显偏移。
> 生产盒子 `10.10.207.46` 全程只读，未修改任何文件/配置。

## 1. 生产方案（逆向结论）

`libEdgeSystem.so` / `libdump4.so` 证据：

```
AlarmRecorder
AlarmRecorder::isSameRectAlarm / getLastALarmTime / setLastALarmTime
libdump4::dumper / dump_task / FrameBuffering
Mp4V2Writer::Mp4V2Writer(media_source_info, dump_task)
VideoRecordSec, VideoRecordPath, VideoRecordPrefix, EnableVideoRecord
VideoRecordLimitMB (default 20 MB)
```

生产配置帮助文本：

> "Recording video backtracking ratio (default 0.5, if the duration is 10 seconds and
> set to 0.4, the alarm will be 4 seconds before and 6 seconds after)"
>
> "Set the duration ratio of backtracking from the moment when the alarm is triggered
> to the time before the alarm when recording a video. ..."

链路：**每路一条媒体源** → `FrameBuffering` 按 PTS 保留 ~10s 编码包 → 告警触发
`AlarmRecorder`（同框去重）→ 取 [触发-5s, 触发+5s] → `Mp4V2Writer` 重封装（不转码）。

生产实测（alarm.db id=20525）：图片 OSD **11:55:49**，视频 **11:55:43–11:55:54**
（704×576/25fps/276 帧/11.04s），图片位于中间。

## 2. 旧实现的问题（已复现）

旧实现用 `ffmpeg -c copy -f segment` 环形分片，告警后按**文件 mtime**取最新 5 个分片再
`-c copy -t 10`：

- 分片落盘滞后（要等下一个关键帧），`-reset_timestamps 1` 丢失原始 PTS；
- 没有"告警图片所在帧"与分片内容的任何时间映射；
- 复现结果（20261105_185715）：图片 OSD **18:57:24**，视频 **18:57:10–18:57:20**，
  **视频比图片提前 4 秒结束**——正是用户反馈的现象。
- 另外异步 `decoder.dump()` 被立即 finalize，dump 文件还没写完就被判 missing。

证据：`docs/assets/alarm_before_ring_image.jpg`、`alarm_before_ring_first.jpg`、
`alarm_before_ring_last.jpg`。

## 3. 尝试过但不采用的方案：sail::Decoder::dump()

单模式直接用同一路 decoder 的 `dump(5,5)` 时，时间是对齐的（图片 OSD 19:12:22，
视频 19:12:17–19:12:27，11s/275 帧），但在 30 路 + 2560×1440 部署下触发了一次
`free(): invalid pointer`（libavutil `av_buffer_unref`）导致容器重启、任务全丢。
因此 **dump 不能作为生产告警视频方案**；相关代码保留为多模式回退。

## 4. 最终方案：单媒体源网关 + 时间戳包环

### 4.1 一条上游连接（对齐生产 media source）

每个任务启动一个 `ffmpeg` 网关进程：

```
相机/ISC RTSP（唯一一条上游连接，-c copy）
  └─ ffmpeg 复制为两路本地 MPEG-TS/UDP
       ├─ udp://127.0.0.1:<portA>  → sail::Decoder（预览/检测）
       └─ udp://127.0.0.1:<portB>  → AlarmRingRecorder（告警包环）
```

- 上游只建一条连接，解决 ISC "一个 openUrl 只允许一路" 的会话冲突；
- 两路本地流来自**同一个 ffmpeg 解复用/PTS**，解码帧与包环没有连接间的时钟偏移；
- 端口按 `(group,slot)` 分配（22000 起），网关重启时重新分配/复用；
- 网关 URL 在断流重连时对 `isc://` 重新 resolve。

### 4.2 时间戳包环

`src/alarm_recorder.hpp`（libdump4 等价）：

- 从本地 UDP 读编码包（不解码、不转码），记录 DTS/PTS/关键帧；
- 按首个包的到达时间建立 `wall = wall0 + (dts - dts0)` 映射，保留 `pre+5s`；
- 告警时取 `[alarm-5s, alarm+5s]` 的包，优先从 ≤start 的最后一个关键帧开始；
- 用 libavformat 重封装 MP4，DTS 归零并单调化；不改变分辨率/帧率；
- 写完后 Task 直接 `rename` 到最终路径（不再 ffmpeg `-c copy`，避免时间戳重写丢帧）。

### 4.3 与生产对齐

| 项 | 生产 | 我们 |
|---|---|---|
| 上游连接 | 每路一条媒体源 | 每路一条（ffmpeg 网关） |
| 缓冲 | libdump4 FrameBuffering（PTS） | 时间戳编码包环（PTS/wall） |
| 取窗 | 触发前后 5s，ratio 0.5 | 触发前后 5s |
| 封装 | Mp4V2Writer 重封装 | libavformat 重封装 |
| 解码/预览 | 同一媒体源 | 同一网关的本地流 |
| 告警去重 | `isSameRectAlarm` | 模型窗口 + cooldown（未做同框去重） |

## 5. 验证

### 5.1 旧实现（对照）

| 项 | 相机 OSD |
|---|---|
| 图片 | 18:57:24 |
| 视频 | 18:57:10–18:57:20（10.03s，2560×1440） |

视频结束早于图片 4s。

### 5.2 最终实现（网关 + 包环，容器 30 任务）

告警图 OSD **19:59:43**；视频 **9.96s / 250 帧 / 2560×1440 / 25fps**；
视频第 5 秒帧的 OSD 为 **19:59:42**，画面中白衬衫人员的位置与告警图一致，
即告警图片位于视频正中（±1s）。

证据：`docs/assets/alarm_after_gateway_image.jpg`、`alarm_after_gateway_mid.jpg`、
`alarm_after_gateway_last.jpg`。

### 5.3 预览不受影响

同一网关下 6488 单通道预览 1920×1080 / **9.88fps**，不同帧 79/79，重复帧 0。

## 6. 边界/遗留

- 任务冷启动前 5 秒内告警没有完整预录（生产同样需要预热，KI-024）。
- 同框告警去重、`VideoRecordLimitMB` 配额未实现（KI-027）。
- 包环从非关键帧开始时，个别播放器可能首帧短暂花屏；已优先回退到关键帧，仍建议
  后续补"首帧强制 IDR"（可用硬件编码重编首 GOP）。
- 多解码模式（multi）未走网关；multi 部署仍用包环/worker 回退（KI-026）。

### 5.4 ISC 告警复验（206.68，经 .25，1280×720）

告警任务 `fefe86ed`（camera 1aae，ISC RTSP 经 .25:10444，网关 + 包环）：

| 项 | 结果 |
|---|---|
| 告警图片 OSD | **21:20:12** |
| 视频 | 1280×720 / 249 帧 / 9.946s |
| 视频第 5 秒帧 OSD | **21:20:12**，画面与告警图一致 |
| 结论 | ISC 告警视频同样居中 |

证据：`docs/assets/isc_alarm_image.jpg`、`isc_alarm_mid.jpg`。
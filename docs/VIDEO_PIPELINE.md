# 告警视频链路：生产 vs 206.25

> 只读生产，不修改生产文件/配置。最后更新：2026-11-09

## 生产盒子

- 一路流持续录制，保留环形缓冲；
- 告警时用 libdump 按告警时间回溯，取前后 N 秒；
- 优点：不依赖推理线程，片段时间与告警时间对齐稳定；
- 代价：持续写盘、占用额外录制句柄/带宽；录制与解码是同一路上游时
  需要 SDK 的 libdump 支持。

## 206.25 当前

- gateway 把上游 TS 分两路：decoder UDP + recorder UDP；
- `AlarmRingRecorder` 在本地 rec 流上维护 16MB packet ring；
- 告警时 `clip_recorder` 从 ring 取 pre/post 秒，H.265 再转 H.264；
- ring 拿不到文件时回退外部 `dump_in_worker`；
- 看门狗 45s 无帧才重启 gateway，避免片段生成期间杀 ring；
- pending 期间新告警先 finalize 旧片段，再生成新片段。

## 优劣对比

| 项 | 生产 libdump | 206.25 packet ring |
|---|---|---|
| 告警时间对齐 | 按录制时间回溯，稳定 | 按帧时间/ring 回溯，已修帧时间戳 |
| 额外上游连接 | 取决于实现，通常持续录制 | 不新增 ISC 会话，复用 gateway 本地流 |
| CPU/磁盘 | 持续写盘，CPU 中 | 16MB/任务 ring，CPU 低 |
| 网关重启 | 录制独立，影响小 | ring 随 gateway 重建，片段可能丢 |
| HEVC | SDK 直接处理 | 需 ffmpeg 转 H.264，偶发 POC 错误 |
| 实现复杂度 | 依赖 libdump SDK | 已有 ring/fallback，改造成本低 |

## 结论

我们保留 packet ring，不直接改成 libdump：

- 优势是**不额外占 ISC 会话、CPU/磁盘低**，适合 30 路并发；
- 劣势是网关重启/流卡死时片段可能失败，已用 45s 看门狗、pending
  finalize、worker fallback 缓解；
- 若要完全达到生产稳定性，需要独立录制进程/SDK libdump，代价是
  额外上游连接或持续写盘；在当前 ISC 配额下不划算。

## 已知限制

1. 历史告警不补录；
2. 流在告警前后彻底断开时，片段仍可能为空；
3. 视频中心使用采样帧时间，不再用保存墙钟。

$p='README.md'; $t=[IO.File]::ReadAllText($p)
$t=$t.Replace("iteration: 105`r`nupdated: 2026-11-19","iteration: 106`r`nupdated: 2026-11-19")
$old='- **2026-11-19 迭代 105（部分 RTSP/MP4/ISC 无画面根因）**'
$new='- **2026-11-19 迭代 106（dma-buf 映射归零）**：定位到 Sophon OpenCV 的 `cv::Mat` 每个都走 bmcv/ion
  dma-buf；把每任务标签牌（36×8=288 条）改成 HostMat、合流 black/no_task 与 fallback 画布改 host buffer 后，
  **dmabuf 映射 290→0、dma-buf fd 586→0**、总 fd 930→589（34 路）。生产同口径 0 映射/135 fd/417 fd（10 路），
  每路 fd 41.7 vs 17.3，我方更低。期间发现 imencode 的 NV12 复用会触发 stack smashing，已回退。
- **2026-11-19 迭代 105（部分 RTSP/MP4/ISC 无画面根因）**'
[IO.File]::WriteAllText($p,$t)

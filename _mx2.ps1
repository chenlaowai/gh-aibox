$p='docs\PRODUCTION_MATRIX.md'; $t=[IO.File]::ReadAllText($p)
$t=$t.Replace('| 库内部（bmrt 模型/张量 + ffmpeg 解码器） | **约 615 个 dma-buf fd / 290 个进程映射** | media_sdk/board_sdk 有 135 个 dma-buf fd、但 **0 映射** | `bmrt_load_bmodel` 与 `h264_bm/hevc_bm` 内部用 dma-buf 堆；`dectest` 单解码器进程 0 映射，量级来自多模型实例 + 34 个解码上下文 |','| Sophon OpenCV `cv::Mat` 分配 | **0 映射 / 0 fd**（标签牌改 HostMat、合流占位图/画布改 host buffer 后） | 无（其预览/合流走 board_sdk，不建 cv::Mat 大对象） | 迭代 106 定位：`cv::Mat` 每个都走 bmcv/ion，36 任务 ×8 标签牌 = 288 映射 + 合流 8 = 290；改 host 后归零 |')
[IO.File]::WriteAllText($p,$t)

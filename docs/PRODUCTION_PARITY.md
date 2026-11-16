# 与生产盒子的机制对齐情况

> 对照对象：生产 `10.10.207.46`（libEdgeSystem + plugins）与开发盒 `10.10.206.68`
> （aidetect）。生产只读。

## 1. 算法插件机制

| 能力 | 生产 | 我们 | 对齐度 |
|---|---|---|---|
| 插件形态 | `libalg_plugin_*.so` 自包含 | 同左，已放 `models/plugins` | 已对齐 |
| 权重 | 嵌入插件，`get_plugin` | 启动 `dlopen` + `get_plugin` 提取 bmodel | 已对齐（六个已知插件） |
| 元数据 | `get_params/get_extra/get_desc/get_extern` | 读取/按文件名映射；`get_desc` 未全量解析 | 部分 |
| 新增算法 | 丢 `.so` 即用 | 丢 `.so` 到 plugins，按名映射/重启 | 部分 |
| 调度 | 主程序统一调度 | `ModelScheduler` 统一调度 | 已对齐 |
| 全量插件 | 80+ 自动发现 | 目前 head/people/behavior/helmet/crowd/suit | 未全量 |

结论：五个算法的**加载/调用/权重**已走同一套“插件自包含 + 启动提取 + 统一调度”；
还不是生产那种 80+ 插件全量自动发现，但新增已知插件只需放 `.so` + 补名字映射。

## 2. 算法调用

| 算法 | 生产调用 | 我们 |
|---|---|---|
| 人头 | head 插件，YOLOv5 | `head`/`head_count`，YOLOv5 + `postprocess_yolov5` |
| 安全帽 | people → 头肩裁剪 → helmet 4 头 | 同左，顶部 70%，out1，阈值 0.8 |
| 反光衣 | 同 helmet 模型 out0 | 同左，阈值 0.9 |
| 人员聚集 | people 计数（minorId 224） | `people_gather` 计数 |
| 抽烟打电话 | people/head → behavior | head → 头肩扩展 → `smoking_call`，alpha=1 |
| 旧算法 | 生产旧插件 | `*_our` 别名，仍可选 |

## 3. 编解码/预览

| 能力 | 生产 | 我们 |
|---|---|---|
| 解码 | 硬件 VPU（ffmpeg/sophon） | 硬件 VPU + sophon-ffmpeg |
| 单任务多模型 | 单解码、多模型 | 单解码、多模型（`Task` 内循环） |
| 预览 | `Show` 线程画框 + JPEG + ZMQ/前端 | `bmcv` 画框 + JPU JPEG + WebSocket |
| 预览帧率 | 约 10fps | 10fps 内容门控 |
| 告警视频 | 包环回取前后 N 秒 | 包环回取前后 5s，`-re` 本地文件 |

## 4. 告警

| 能力 | 生产 | 我们 |
|---|---|---|
| 可疑框 | 蓝框 | `pending=true` 蓝框（新实现） |
| 触发 | 滑动窗口 + `detect_threshold` + 冷却 | 同左 |
| 告警框 | 红框 + 标签 | 红框 + 标签（仅触发帧） |
| 告警图 | 原图 + 框 + 标签 | 同左 |
| 告警视频 | 前后 N 秒 | 前后 5s 包环 |
| 上报 | ReportAlarmEvent | 同接口（已适配） |

## 5. 前端

| 能力 | 生产 | 我们 |
|---|---|---|
| 任务管理 | 算法下拉、参数、区域 | 源码在 `web/`，走 dotnet API + SQLite |
| 算法列表 | 生产算法表 | `tb_algo_mode` 已加五算法 + `_our` |
| 参数 | 每算法特定阈值 | 新算法带 `conf/nms/exceed/attr_thresh` |
| 停止按钮 | 可用 | 实测 PATCH 200，按钮从“停止”变“启动” |

## 6. 仍不一致/待办

1. 生产 80+ 插件全量自动发现；我们只覆盖六个已知插件。
2. 生产可能有目标跟踪/轨迹；我们逐帧检测。
3. 生产 crowd 密度图解码；我们人员聚集用人形计数。
4. 生产蓝框可能带倒计时/可疑计数；我们只有颜色。
5. 生产前端源码是完整业务工程；我们 `web/` 已有任务管理源码，但本次只改 DB/API，
   尚未重新构建 dist（当前 dist 已能读取新算法表）。

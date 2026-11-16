<!-- pm:meta
iteration: 5
updated: 2026-11-02
commit: -
-->

# 业务领域

> 业务重的项目才需要这份。核心价值在「不变量」——AI 猜不出来，猜错就是数据事故。
> 本文件依据 `重构AI盒子ai检测服务端.md` 的需求整理；迭代 5 的 C++ 服务已接反光衣 + 安全帽/救生衣/抽烟/人员 5 类模型，救生衣样本/真实 ISC 仍待补。

## 术语表

| 业务叫法 | 代码里叫 | 含义 | 别名/易混 |
|---|---|---|---|
| 任务 | `task` / `taskId` | 一路视频流 + 一组模型配置的检测单元 | 通道任务 |
| 通道 | `channel` | 前端"通道管理"里的视频流参数（rtsp/isc） | 视频源 |
| 合流通道 | `all_1` ~ `all_4` | 9 路任务画面拼成 1 张 3×3 大图，共 4 路 | 大图、监控墙 |
| 模型 | `model_name` | `person` / `safetyhelmet` / `reflectivejacket` / `lifejacket` / `cigarette` / `reflectivejacket_sky` | 算法 |
| 目标类别 | `target_id` | 该模型要报警的类别索引，例如未戴安全帽=1 | 告警类别 |
| 感兴趣区域 | `polygon_points` | 任务配置的多边形区域；为空表示全图 | ROI、区域 |
| 检测阈值 | `exceed_num` | 触发告警所需的目标个数 | 人数阈值 |
| 置信度阈值 | `conf_thresh` | 过滤检测框的分数阈值 | conf |
| NMS 阈值 | `nms_thresh` | 非极大值抑制 IoU 阈值 | iou |
| 推理间隔 | `frame_interval` | 每 N 帧推理一次 | 抽帧 |
| 告警区间 | `detect_section` | 判定告警的滑动窗口帧数 | 时间区间 |
| 告警帧数 | `detect_threshold` | 窗口内达到多少帧才触发 | 触发帧数 |
| 冷却时间 | `cooldown_time` | 同一任务/模型两次告警的最小间隔（秒） | 冷却 |
| 告警视频时长 | `video_duration` | 告警时刻前后留存的视频长度（秒） | 前后 10 秒 |
| 告警事件 | `report` / `alarm` | 一张带框图 + 一段前后 10 秒视频 + 上报 JSON | 上报 |

## 模型清单（来自需求文档）

| `model_name` | Bmodel | `class_names` | `target_id` | `report_content` | `exceed_num` |
|---|---|---|---|---|---|
| `person` | `damoyolo_tinynasL25_S_person.bmodel` | `['人']` | 0 | 人员聚集 | 3 |
| `cigarette` | `damoyolo_tinynasL25_S_cigarette.bmodel` | `['抽烟行为']` | 0 | 抽烟行为 | 1 |
| `lifejacket` | `damoyolo_tinynasL25_S_lifejacket_2.bmodel` | `['穿救生衣','未穿救生衣']` | 1 | 未穿救生衣 | 1 |
| `safetyhelmet` | `damoyolo_tinynasL25_S_safetyhelmet.bmodel` | `['戴安全帽','未戴安全帽']` | 1 | 未戴安全帽 | 1 |
| `reflectivejacket` | `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel` | `['穿反光衣','未穿反光衣']` | 1 | 未穿反光衣 | 1 |
| `reflectivejacket_sky` | `damoyolo_tinynasL25_S_fp16_reflectivejacket_sky_1.bmodel` | `['穿反光衣','未穿反光衣']` | 1 | 未穿反光衣 | 1 |

> 当前 `reflectivejacket` 改用 FP16 `yolov11s_fp16_1b_back_reflectivejacket_1.bmodel`（输入 `/255`）；INT8 分类输出全 0，已决策暂缓（见 `KNOWN-ISSUES.md` KI-001、`DECISIONS.md` ADR-009）。

> 除 `reflectivejacket` 外，其余模型都是 DamoYOLO 权重；官方 demo 没有 DamoYOLO，旧源码是拿 YOLOv5 demo 改后处理实现的（待核对具体后处理差异）。

## 状态机

当前仅定义目标语义，服务端尚未实现：

- 任务状态：`stopped → starting → running → stopping → stopped`；异常进入 `error`。
- 允许迁移：`start` 只能从 `stopped/error` 进入 `starting`；`stop` 只能从 `running/starting` 进入 `stopping`。
- 非法迁移：返回结构化错误，不改状态；重复 `start` 幂等返回当前状态（待核对旧接口行为）。
- 模型实例状态：`unloaded → loading → ready → busy`；帧率打满时允许再加载同权重第二个实例。

## 计算公式与判定

- 告警人数：`report_num = count(detections where class_id == target_id)`。
- 触发条件（需求描述）：在最近 `detect_section` 帧中，有至少 `detect_threshold` 帧满足 `report_num >= exceed_num`。
- 冷却：同一任务同一模型，距上次上报不足 `cooldown_time` 秒不再上报。
- 告警视频：告警时刻前 5 秒 + 后 5 秒的原视频（不带框）；图片为告警时刻带框、带类别文字的帧。
- 输出帧率：原始流 > 10 fps 时，输出给前端的检测流压到 10 fps；原始流 ≤ 10 fps 时保持原帧率。
- 合流：36 路任务 = 4 个合流通道 × 每通道 9 路；每个合流通道输出一张 3×3 大图。

## 不变量（必须永远成立）

- 一个 `taskId` 同一时刻最多一个运行实例；重复启动不能产生两个进程/两套解码。
- 36 路任务通道对应 4 路合流，`all_1..4` 不得少路、错路。
- 告警图片必须带检测框和类别；告警视频必须不带框，且是告警前后约 10 秒。
- 前端拿到的帧流不超过 10 fps；不得因为推流把检测线程拖死。
- 模型名必须与后端下发的 `model_name` 完全一致；未知模型名要拒绝任务启动并给出明确错误。
- 任何凭证（SSH/Web/JWT/GitHub token）不得出现在日志、告警 JSON、检测图片叠字或仓库文档中。
- 任务停止后必须释放解码器、模型实例、推流连接；不允许残留线程/设备内存。

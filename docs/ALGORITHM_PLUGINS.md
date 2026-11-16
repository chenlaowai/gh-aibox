# 生产算法插件机制与五种算法逆向

> 生产盒子 `10.10.207.46` 全程只读（ps/ls/cat/strings/nm/objdump/readelf/DB 拷贝），
> 未修改任何文件或配置。本文记录插件 ABI、五种算法模型/参数，以及我们的集成进度。

## 1. 生产插件机制（RE 结论）

生产 `plugins/` 下有 80+ 个 `libalg_plugin_*.so`（app.json 清单，单文件 5–38 MB）。
每个插件是**自包含**的：不依赖 `libboard_sdk_bm.so`，只依赖 libstdc++/libm/libc。

导出的 C ABI（`nm -D`）：

| 导出 | 含义（实测） |
|---|---|
| `get_plugin()` | 返回嵌入的 **bmodel 起始地址**（magic `ee aa 55 ff`） |
| `get_plugin_size()` | 返回 bmodel 字节数 |
| `get_desc()` | 返回 `const char*`，格式 `中文名\|English Name` |
| `get_params()` | 返回 `board::ModelAttr*`：预处理/阈值等 |
| `get_extra()` | 返回 `board::ModelParams*`：anchors、类别、分数、version |
| `get_extern()` | 返回 `std::string` JSON：`normalize/toTensor/transpose` |
| `library_unload()` | 卸载（部分插件） |

`libEdgeSystem.so` 动态 `dlopen` 插件，读取 bmodel/metadata，算法后处理在主程序里
按插件描述选择（YOLO / 分类 / 密度等）。新增算法只需放 `.so`，不用改主程序。

### 对我们现有实现的评估

| | 生产插件化 | 我们原来 |
|---|---|---|
| 新增算法 | 放 `.so` + 配置 | 改 `make_model_specs` + 重新编译 |
| 权重 | 嵌在插件内，自描述 | 外部 `.bmodel` + 硬编码路径/类别 |
| 预处理/阈值 | `get_params/get_extra/get_extern` | `ModelSpec` 手写 alpha/beta |
| 多算法组合 | 统一 `BaseAlgItem/minorId` | `algoCode` 手写分支 |
| 体积/复杂度 | 插件多、镜像大 | 简单但僵化 |

结论：生产机制更成熟，值得吸收“**插件自描述 + 注册表**”，但不必照搬 80 个插件的体量。

## 2. 五种算法的生产插件/模型

从 `get_plugin()/get_plugin_size()` 提取 bmodel 后用 `bmrt_test` 验证：

| 算法 | 插件 | bmodel / 输入 | 输出 | 说明 |
|---|---|---|---|---|
| 人数监控（人头） | `libalg_plugin_head.so` | `head_v10_20251031`, 1×3×480×832 | 1×8190×5 | YOLOv5，`xyxy + obj`，1 类 Head |
| 人员聚集 | `libalg_plugin_crowd.so` | `crowd_20250424`, 1×3×384×640 | 2×[1×15360×2] | 拥挤度/密度，desc `拥挤度识别` |
| 安全帽 | `libalg_plugin_helmet.so` | `hel4cls20250317_20250922`, 1×3×224×224 | 4 头 [2],[3],[3],[2] | 多属性分类：安全帽/反光衣/长短袖/裤长 |
| 反光衣 | 同上 | 同上 | 同上 | 同一模型的属性头 |
| 抽烟打电话 | `libalg_plugin_behavior.so` | `scv10_102_best`, 1×3×256×256 | 1×1344×9 | YOLOv5，4 类 + obj |
| 工服（参考） | `libalg_plugin_suit.so` | `gongfu_reid4_osnet1_bigdata`, 1×3×256×128 | 1×512 | ReID，desc `工服识别` |

插件描述原文：

```text
head:     头部检测|Head Detection
crowd:    拥挤度识别|Crowd Detection
helmet:   安全帽反光衣长短袖|Helmet Detection
behavior: 抽烟打电话玩手机|Smoking Detection
people:   人形检测|People Detection
suit:     工服识别|Uniform detection
```

阈值来自生产 `edge.db.table_threshold`：

| 项 | 阈值 |
|---|---|
| 未戴安全帽 | 0.80 |
| 未穿反光背心 | 0.90 |
| 未穿长袖 | 0.65 |
| 打电话 | 0.40 |
| 抽烟 | 0.40 |

任务配置映射（`table_task.json` 的 `BaseAlgItem.minorId`）：

| minorId | 名称 |
|---|---|
| 4 | 抽烟打电话检测 |
| 5 | 反光衣检测 |
| 152 | 船舶-救生衣检测 |
| 195 | 安全帽检测V3 |
| 224 | 人员聚集 |

任务参数在 `RuleProperty`：`RulePoints/RuleType=兴趣区域`；算法阈值走全局
`table_threshold`，不是每个任务单独存。

## 3. 输出格式实测

### 3.1 YOLOv5（head/people/behavior）

输出 `[1,N,C]`，每行：

```text
x1, y1, x2, y2, objectness, class_logits...
```

- 坐标是**输入图绝对像素**，不是 grid/anchor；
- `objectness` 已是 0..1；
- 类别列是 logits（需 sigmoid；head 的 C=5 时只有 1 类，无类别列）。

验证（生产 NoHelmet 告警视频首帧，head 模型 480×832）：

```text
row 4736: x1=444.25 y1=351.25 x2=462.75 y2=376.75 obj=0.830
```

映射回 704×576 原图后，正好是画面右侧工人的头部；我们集成后在告警图上画出了
两个 `Head` 框（见 `docs/assets/alg_head_alarm.jpg`）。

### 3.2 helmet 多属性分类

4 个输出的 softmax 与金标准裁剪（生产 alarm raw 的 `RelativeBox`）对照：

| 输入 | out0 [2] | out1 [3] | out2 [3] | out3 [2] |
|---|---|---|---|---|
| NoHelmet 头部裁剪 | 0.12/0.88 | **0.72/0.23/0.05** | 0.94/0.02/0.03 | 0.09/0.91 |
| NoVest 头部裁剪 | 0.91/0.09 | **0.06/0.88/0.06** | 0.84/0.06/0.10 | 0.09/0.91 |

推断：out0=反光衣（class0 未穿）、out1=安全帽（class0 未戴）、out2=长短袖、
out3=裤长；具体类别名还需用更多样本确认。

### 3.3 behavior

256×256 输入，`[1,1344,9]` 同上；金标准抽烟裁剪能得到 obj≈0.84 的框，类别分在
class2/3 附近。因模型作用于**人形裁剪**而不是整帧，需先用人形检测框裁剪再推理。

## 4. 我们已经做的集成

代码（`src/model.hpp`、`src/task.hpp`）：

- `ModelType` 增加 `YoloV5`；
- `ModelSpec` 增加 `input_w/input_h/class_logits`；
- 新增模型项 `head`（480×832）、`people`（384×640）、`behavior`（256×256），
  指向 `models/BM1684X/plugins/*.bmodel`；
- `detect()` 改为**按模型自己的输入尺寸**做 letterbox，而不是固定 640；
- 新增 `postprocess_yolov5()`：`xyxy + obj + class logits`，NMS，坐标映射回原图；
- 单媒体源网关对本地文件输入加 `-re`，否则探针读不到。

已验证：

```text
model loaded: head .../plugins/head.bmodel input=1,3,480,832 outputs=1
t_head model=head detections=2
t_head ALARM model=head count=2 img=.../20261105_235449.jpg
```

告警图 `docs/assets/alg_head_alarm.jpg` 中两个 Head 框位置正确。

## 5. 待完成

1. helmet/vest：人形检测框 → 头部/上半身裁剪 → 4 头分类；按 0.8/0.9/0.65 阈值判定。
2. behavior：人形裁剪 → YOLOv5，确认 4 类顺序（抽烟/打电话/玩手机/正常）。
3. crowd：`[1,15360,2]×2` 密度输出解码，或先用 people 检测计数实现“人员聚集”。
4. 前端任务配置：把 `algoCode` 扩展为五种算法，增加 `conf/nms/exceed_num/area`、
   安全帽/反光衣/抽烟/打电话阈值；接口见 `docs/INTERFACES.md`。
5. 用生产告警视频逐算法回归（已准备 NoHelmet/NoVest/Smoking 样本）。
6. 插件加载器：启动时 `dlopen` 生产 `.so`，自动提取 bmodel/metadata，替代静态 specs。

## 6. 验证样本

生产告警原图/视频（只读拷贝，位于 `.work/algdata/`）：

- `IMAGE_B27D..._NoHelmet_labeled.jpg` / `nohelmet.mp4`
- `IMAGE_3E78..._NoVest_labeled.jpg`
- `IMAGE_8286..._Smoking_labeled.jpg`

对应 `raw.Result.RelativeBox` 已提取，可用于精确裁剪验证。

## 7. 当前集成状态（2026-11-06）

| 算法 | 状态 | 验证 |
|---|---|---|
| 人数监控（人头） | 已接入 `head`/`head_count` | 生产 NoHelmet 视频 count=2，Head 框正确 |
| 安全帽 | 已接入 `helmet`（人形→头肩 70% 裁剪→out1） | NoHelmet 视频 count=1，告警图正确 |
| 反光衣 | 已接入 `vest`（同一分类器 out0） | NoVest 视频 count=1，告警图正确 |
| 人员聚集 | 已接入 `people_gather`（人形计数） | NoHelmet 视频 count=2 |
| 抽烟打电话 | 已接入 `smoking_call`（head 基座→头肩裁剪→behavior） | 生产 Smoking 视频 count=1，标签“抽烟/打电话”正确 |

告警图：

- `docs/assets/alg_head_alarm.jpg`
- `docs/assets/alg_helmet_alarm.jpg`
- `docs/assets/alg_vest_alarm.jpg`
- `docs/assets/alg_smoking_alarm.jpg`

任务配置：`algoCode` 使用 `head_count`、`helmet`、`vest`、`people_gather`、
`smoking_call`；`algoConfig` 支持 `conf_thresh/nms_thresh/exceed_num`，算法阈值
当前按生产默认（安全帽 0.8、反光衣 0.9、抽烟/打电话 0.4）。
### 7.1 插件启动加载器（已实现）

`ensure_plugin_models()` 在启动时扫描 `models/BM1684X/plugins/libalg_plugin_*.so`：
`dlopen` → `get_plugin()/get_plugin_size()` → 把嵌入 bmodel 写到
`<name>.bmodel`（已存在则跳过）。实测删除 `head.bmodel` 后启动，日志：

```text
plugin model extracted: head 6358120
plugin model extracted: crowd 39343576
```

即生产插件 `.so` 可直接丢进 `models/plugins` 使用，不需要重新编译主程序。

### 7.2 behavior 关键修正

- behavior 模型要求 **0..255 输入**（alpha=1），不是 1/255；之前 conf 全为 0 就是
  这个原因；
- 人头框先过滤超大误检（>20% 画面），再扩成头肩（宽 3×、高 2.5×）；
- 生产 Smoking 视频上 behavior best=0.73，成功告警并正确框出抽烟人员。
### 7.3 任务配置接口

新增 `GET /algorithm/list`（JWT 同其他接口），返回五种算法的 code、名称、模型、
目标类和参数定义（`conf_thresh/nms_thresh/exceed_num`，安全帽/反光衣额外
`attr_thresh` 默认 0.8/0.9，抽烟默认 0.4）。前端可直接按该结构渲染任务配置项。

任务 `model` 示例：

```json
[
  {"algoCode":"helmet","algoConfig":[
    {"code":"conf_thresh","configValue":0.3},
    {"code":"nms_thresh","configValue":0.7},
    {"code":"exceed_num","configValue":1},
    {"code":"attr_thresh","configValue":0.8}],
   "taskArea":[]}
]
```

`algoCode` 可用：`head_count`、`people_gather`、`helmet`、`vest`、`smoking_call`。
## 8. 前端算法配置与任务迁移（2026-11-06）

- 前端源码：`web/`（`src/views/bi/task-management`、`services/api/task.ts`）；
  实际页面由 dotnet `aibox-api` + SQLite `data.db` 驱动。`web` 源码文件时间
  2026-04-29 09:49，`/data/transtation/nginx/project/aibox/dist18).zip` 时间 09:53，
  同批构建；部署 dist 已含 task-management。2026-11-06 已用 `pnpm build` 重新
  构建并部署，旧版备份 `dist.bak_20261106_alg`。
- `tb_algo_mode` 增加五种生产算法：
  `head_count`、`people_gather`、`helmet`、`vest`、`smoking_call`；
  旧六种改名为 `*_our`（人员检测_our、反光衣检测(常规)_our、安全帽检测_our 等）。
- `tb_algo_config` 增加每算法参数：`conf_thresh/nms_thresh/exceed_num`，
  安全帽/反光衣额外 `attr_thresh`（0.8/0.9）。
- 任务迁移：29 个工地任务迁到新算法；6488 办公室任务保留旧 `_our`，避免误报。
- 前端实测：算法配置页显示新算法与 `_our`，可勾选；任务停止按钮根因是
  `n-data-table :loading` 覆盖层拦截点击；已改为
  `:loading="loading && taskData.length === 0"`，并重新构建部署。修复后
  `PATCH /aibox/api/Task/UpdateTaskStatus/{id}/1` 返回 200，按钮可停止/启动
  （证据 `docs/assets/ui_task_stop.txt`）。
  工地任务配置截图：`docs/assets/alg_config_construction.png`。
## 9. 任务参数精简（2026-11-06）

任务编辑页已移除红框内的 5 个字段：间隔帧数、触发帧数、上报间隔、告警视频时长、
触发区间。只保留：

- 全局 **告警上报间隔（分钟）**：`tb_param_config.AlarmReportIntervalMinutes`，默认 3；
- 全局 **告警视频时长（秒）**：`tb_param_config.AlarmVideoDurationSeconds`，默认 10；
- 两者在 **参数配置** 页维护，任务保存时自动读取。

检测内部不再使用 `frameInterval/detectThreshold/detectSection`：

- 检测每 100ms 一次；
- 目标数量达到算法 `exceed_num` 后进入 **3s 蓝框倒计时**；
- 3s 后红框，目标不消失保持红；消失 1s 后重置；
- 上报按全局分钟间隔，不再由任务帧参数控制。

实测：任务编辑页 5 个字段全部消失，参数配置页出现两个全局参数；点击“更新”返回
`PUT /api/Task/UpdateTask 200`，DB 中该任务保存为 `frame_interval=1,
detect_threshold=30, detect_section=30, video_duration=10, cooldown_time=3`。
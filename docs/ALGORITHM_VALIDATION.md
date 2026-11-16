# 生产算法逆向验证报告

> 验证时间：2026-11-06
> 生产盒子 `10.10.207.46` 只读，未修改任何文件/配置。
> 验证环境：开发盒 `10.10.206.68`，`aidetect` 容器/单媒体源测试进程，BM1684X。

## 1. 验证方法

1. 从生产插件 `get_plugin()/get_plugin_size()` 提取 bmodel；
2. 用 `bmrt_test` 确认模型可加载；
3. 用 `tools/model_probe` 对生产告警原图/告警视频逐帧跑单模型，核对输出 shape、
   坐标、类别分；
4. 用开发盒 `Task` 跑生产告警视频，输出带框告警图；
5. 人工查看告警图，确认框位置与标签语义正确。

生产告警样本来自 `alarm.db`：

| 类型 | 条数 | 用途 |
|---|---|---|
| NoVest | 469 | 反光衣 |
| BoatNoLifeJacket | 276 | 救生衣（非本次五算法） |
| NoHelmet | 240 | 安全帽 |
| Smoking | 15 | 抽烟 |
| PeopleGather / HeadCount / Crowd | 0 | 生产库无此类告警行，人员聚集/人数用现场视频计数验证 |

## 2. 五种算法结果

### 2.1 人数监控（人头）

- 模型：`head_v10_20251031`，输入 `1×3×480×832`，输出 `1×8190×5`
- 解码：`x1,y1,x2,y2,obj`，坐标是输入图绝对像素，obj 已 sigmoid
- 验证：生产 NoHelmet 告警视频首帧，`detections=2`，两个 Head 框位置正确
- 证据：`docs/assets/alg_head_alarm.jpg`
- 任务：`head_count`，`exceed_num` 默认 1

### 2.2 安全帽检测

- 模型：`hel4cls20250317_20250922`，输入 `1×3×224×224`，4 个分类头
- 调用：生产 people 检测器 → 人形框顶部 70% → 224×224 → 分类头 out1；
  若概率低于阈值，再用整人框跑一次取最大（恢复被顶部裁剪切掉的头）
- 阈值：未戴安全帽 `0.8`
- 校准：NoHelmet 头部裁剪 out1[0]=0.866；NoVest 头部裁剪 out1[0]=0.061
- 验证：生产 NoHelmet 告警视频，`count=1`，告警图正确框出未戴安全帽人员
- 证据：`docs/assets/alg_helmet_alarm.jpg`

### 2.3 反光衣检测

- 模型：同 helmet 模型，分类头 out0（反光衣）
- 调用：人形框顶部 30% → 224×224 → out0
- 阈值：未穿反光衣 `0.9`
- 校准：NoVest 裁剪 out0[0]=0.905；NoHelmet 裁剪 out0[0]=0.344
- 验证：生产 NoVest 告警视频，`count=1`，告警图正确框出未穿反光衣人员
- 证据：`docs/assets/alg_vest_alarm.jpg`

### 2.4 人员聚集

- 生产库没有 PeopleGather 告警行；按生产任务 `minorId=224` 的语义，用
  `people` 人形检测器计数实现。
- 模型：`people_v10_2025101702`，输入 `1×3×384×640`，输出 `1×5040×9`
- 验证：NoHelmet 视频 `count=2`，达到 `exceed_num=2` 触发
- 说明：`crowd_20250424` 密度模型已提取，`[15360,2]×2` 输出解码未启用；
  人员聚集当前走“人形计数”，不是密度图。

### 2.5 抽烟打电话检测

- 模型：`scv10_102_best`，输入 `1×3×256×256`，输出 `1×1344×9`
- 调用：head 检测器 → 过滤超大误检 → 头肩扩展（宽 3×、高 2.5×）→ behavior
- 关键：**输入必须是 0..255（alpha=1）**；用 1/255 时 obj 只有 0.0037，完全漏检
- 类别：class2（抽烟/打电话方向），阈值 0.4
- 验证：生产 Smoking 视频 `behavior best=0.73`，`count=1`，告警图正确框出抽烟人员
- 证据：`docs/assets/alg_smoking_alarm.jpg`

## 3. 视觉证据

| 算法 | 文件 |
|---|---|
| 人头 | [alg_head_alarm.jpg](assets/alg_head_alarm.jpg) |
| 安全帽 | [alg_helmet_alarm.jpg](assets/alg_helmet_alarm.jpg) |
| 反光衣 | [alg_vest_alarm.jpg](assets/alg_vest_alarm.jpg) |
| 抽烟 | [alg_smoking_alarm.jpg](assets/alg_smoking_alarm.jpg) |

## 4. 结论与限制

1. 五个算法都已在生产告警样本上跑通并出带框图；安全帽/反光衣/抽烟的框和标签
   语义正确。
2. 生产库没有 PeopleGather/HeadCount/Crowd 告警行，人员聚集/人数只能用现场
   视频计数验证，不能声称“生产告警视频逐条回归”。
3. `crowd_20250424` 密度模型尚未解码，人员聚集暂用人形计数；如果生产的人员聚集
   实际依赖密度图，需要下一步补 `[15360,2]×2` 解码。
4. behavior 的 4K 基座依赖 head 检测器；head 偶发超大误检框，已用“>20% 画面”
   过滤，但极远/极小目标仍可能漏。
5. 本报告只覆盖五种算法模型本身；前端标注重叠/蓝框→红框告警逻辑另见
   `PRODUCTION_ALARM_LOGIC.md`。

## 5. 关于 crowd `[15360,2]×2`

`crowd_20250424` 输入 384×640，stride 8 时特征图 48×80=3840 个位置；
15360=4×3840，即每个位置 4 个候选 × 2 个值，两个输出头各一份。

实测输出（NoHelmet 场景，alpha=1/255）：

```text
output 207_Reshape_f32 shape=1,15360,2  range=-21.3906..20.4062
output 191_Reshape_f32 shape=1,15360,2  range=-0.1443..0.2047
```

第一个头是大范围坐标/偏移量（±20 网格单位），第二个头是小范围 logits/密度值
（±0.2）。插件元数据：anchors 18（YOLOv5 标准），`ModelAttr` offset36=8、
offset40=-1，说明它是**拥挤度专用后处理**，不是普通 YOLO；需要按密度图/锚点
自定义解码。实测 alpha=0.0171 时第二头 30720 个值：人群场景 sum=620.1、空旷场景
sum=487.6，只有约 1.27 倍差异，不能简单用 sum 阈值。
离线 `libEdgeSystem.so` 符号显示生产实际走 `CrowdMethod::process_people_head` +
`CowCntMethod` + `turbo_softmax`，输入 384×640、热力图 96×160（15360 点），
是“人头热力图”而非普通框；需要生产源码/HeatMap 调用链才能准确复刻。已在生产 `/data /edge /opt /usr/local /home /root /srv /Code`
搜索 `board_algo_sdk.cpp`、`CrowdMethod*`、`CowCntMethod*`、`HeatMap*` 及相关头文件，
均未找到；已实现 `CrowdHead` 后处理：output0 为 96×160×2 class logits，逐点 softmax 取第二类，
  output1 为 offsets；阈值 0.5，映射回原图生成人头框。新增 `crowd_head` 模式，
  本地 H264 视频实测 `model=crowd_head detections=1`；人员聚集任务仍默认 people 计数，
  可在前端切换 crowd_head。

## 2.6 批量分类器回归（10 条生产告警原图）

从生产 `alarm.db` 取 5 条 NoHelmet + 5 条 NoVest，按 `Result.RelativeBox` 的人形框
裁剪（安全帽 70%，反光衣 30%），helmet 模型 alpha=1/255：

| 样本 | 类型 | out1 未戴安全帽概率 | 结果 |
|---|---|---|---|
| 20522 | NoHelmet | 0.808 | 命中（≥0.8） |
| 20521 | NoHelmet | 0.634（整人框 0.809） | 回退命中 |
| 20520 | NoHelmet | 0.803 | 命中 |
| 20519 | NoHelmet | 0.890 | 命中 |
| 20517 | NoHelmet | 0.915 | 命中 |

| 样本 | 类型 | out0 未穿反光衣概率 | 结果 |
|---|---|---|---|
| 20524 | NoVest | 0.892 | 略低于 0.9 |
| 20518 | NoVest | 0.921 | 命中 |
| 20514 | NoVest | 0.910 | 命中 |
| 20513 | NoVest | 0.935 | 命中 |
| 20496 | NoVest | 0.862 | 未命中 |

结论：安全帽顶部 70% + 整人框回退后 5/5 命中，20521 从 0.634 提升到 0.809（头部在画面边缘/
人形框截断）；反光衣在 0.86~0.94 之间，0.9 阈值对 20524/20496 偏紧。这解释了
现场偶发漏报，后续需要按摄像头校准裁剪比例/阈值，而不是只用默认 0.8/0.9。
## 2.7 船舶-救生衣（进行中）

- 插件：`libalg_plugin_boat_life_jacket.so`，desc“救生衣检测”
- 模型：`boat_life_jacket.bmodel`，input 1×3×256×256，output 1×1344×10
- 元数据：anchors 18（YOLOv5 标准），ModelAttr offset36=8、offset40=7
- 关键实测：模型需要 **alpha=1（0..255）**；用 1/255 时 6 个 class logits
  全在 0.5 附近，alpha=1 后 crop 上 class1/class3 约 0.73
- 已用生产 gdb 的 DWARF 类型确认 `ModelAttr offset40=7` 对应
  `board::VERSION_V8`；按 YOLOv8 anchor-free 解码：4 box logits + 6 class，
  grid 32/16/8、stride 8/16/32，类别 3 为“未穿救生衣”
- 已在与 .25 同镜像的 aidetect 上接入 `boat_life_jacket`/YoloV8：
  生产 boat 视频可触发，但未加 ROI 时出现 71 个框（草地/水面误检）；
  生产任务有 RulePoints ROI；ROI 必须放在 model.taskArea（不是 task.taskArea）。
  加 .50-.65/.45-.65 的 model ROI 后，boat 检测从 71 框降到 **2 框**，验证了生产
  ROI 过滤路径；任务级 ROI 不会生效。
- 证据：`docs/assets/alg_boat_alarm.jpg`
- 生产 gdb：临时 gdb（/tmp）只读 attach 时，HeatMap 断点触发后 app 出现一次
  SIGABRT/自动重启；已恢复监听 80/10002/1935，临时 gdb 已删除。后续不再 attach，只做静态反汇编。
## 2.8 安全帽 V3（实验）

- 生产插件 `libalg_plugin_minor_helmet.so`，desc“安全帽V3检测|Helmet(V3) Detection”
- 提取 `minor_helmet.bmodel`：input 1×3×256×256，output 1×1344×7（4 box + 3 class）
- ModelAttr offset40=15 = `VERSION_YOLOV10`；alpha=1 时 class1/class2 有 0.82/0.98
- 已新增 `helmet_v3` 模式（YOLOv10 后处理，target class1，阈值参数
  `no_helmet_det_threshold=0.8`/`sensitivity_helmet_v3=0.5`）
- 全图直接推理仍出现大量框（231），说明生产 V3 是**人形 crop 内检测**而不是全图；
  目前作为实验模式，未替换现有 helmet 任务，需按人形 crop/ROI 继续调
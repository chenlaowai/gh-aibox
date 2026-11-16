# 生产算法逆向与前后处理对照

> 本文只记录从生产盒子逆向出的算法调用/前后处理逻辑，以及 206.25 当前实现
> 的差异。生产盒子全程只读，未修改任何文件/配置。
> 最后更新：2026-11-09（迭代 24）

## 1. 通用调度

生产 `libEdgeSystem.so` 的算法框架是：

```text
Decode -> 人形/人头/船舶等 base detector
      -> MinorXxx 裁剪（people/head/boat/container）
      -> 分类/检测模型
      -> onInferFinished 生成 Box/属性
      -> AlarmEngine 滑动窗口/冷却
      -> 预览 Box + 告警图/视频
```

206.25 对应实现：

```text
Task::run -> gateway ffmpeg -> sail::Decoder
          -> Task::detect
          -> models_ 顺序推理
          -> update_alarm
          -> render_jpeg / render_preview_fast
          -> save_alarm
```

差异重点：

1. 生产有“人员检测基础配置”：`enable_people_deep_mode`、`threshold_for_people`；
   206.25 的 people base 目前固定 `conf=0.3`，没有大分辨率模式开关。
2. 生产任务配置把这两个值放在 `UserData`，不是每个 minor 算法的
   `algoConfig`；206.25 任务模型没有这两个字段。
3. 生产 AlarmEngine 是每算法滑动窗口；206.25 用每目标 `TargetState`
   倒计时 + 全局上报间隔。

## 2. 算法清单

### 2.1 安全帽检测 V3（minor 195，major 1）

- 模型：`minor_helmet.bmodel`，input `1×3×256×256`，output `1×1344×7`
- 类别：**0=未戴安全帽、1=安全帽、2=人员**
- 生产裁剪：PeopleMethod 人形框顶部 70%（宽 1.05×）
- 阈值：`helmet_det_threshold=0.5`、`no_helmet_det_threshold=0.7`
- 持续：`helmet_v3_keep_no_sec=5`、`helmet_v3_repeat_sec=0`
- 206.25：`YoloV10Crop` + people 基座顶部 70%；class0/1 独立阈值；
  生产型任务保持 0.7/5s；6488 数智技术中心演示任务因办公室白衬衫人员
  在 0.7 下完全不出告警，保留 0.0/1s + people 0.3 的演示覆盖，属于
  任务级特例，不是全局默认。
- 差异：`enable_people_deep_mode` 只解析/记录，people 模型输入固定，
  尚未真正放大分辨率；`threshold_for_people` 已生效为 0.85。

### 2.2 反光衣检测（minor 5）

- 模型：`helmet.bmodel`，input `1×3×224×224`，4 个分类头
- 类别头：out0=反光衣、out1=安全帽
- 生产裁剪：人形框顶部 30% → 224×224 → out0
- 阈值：`helmet_3cls_no_vest_threshold=0.7`
- 持续/间隔：`helmet_3cls_no_vest_sec=5`、`..._alarm_sec=30`
- 206.25：`AttrClassifier` + people 框顶部 30%；`vest_thresh`/`attr_thresh`；
  `helmet_3cls_no_vest_*` 映射 keep/repeat。
- 差异：people base 阈值/大分辨率模式未接；生产同一模型同时用于安全帽，
  206.25 仍保留旧的 `helmet` 别名。

### 2.3 安全帽检测（旧 4 类，minor 195 之前/兼容）

- 模型：`helmet.bmodel`，out1 未戴安全帽
- 生产：人形框顶部 70%；阈值 0.8；回退整人框
- 206.25：`helmet` 模式保留，30 个任务已切到 V3；旧任务仍可用。

### 2.4 船舶-救生衣（minor 152）

- 模型：`boat_life_jacket.bmodel`，input `1×3×256×256`，output `1×1344×10`
- 类别：救生衣1/人员1/救生衣2/未穿救生衣/救生衣3/人员2
- 生产父级：`majorId=1`，`MinorBoatLifeJacket` 走 `MinorContainer::boxesCropPolicy`
  （人形框），不是独立 `boat.bmodel` 船框链路。
- 参数：`safety_jacket_threshold=0.3`、`safety_jacket_duration_sec=5`、
  `safety_jacket_alarm_sec=30`
- 206.25：`BoatLifeJacket` 只吃 people 框，外扩 10%，256×256；
  class3 最大概率 ≥ 阈值且大于三个救生衣类最大概率才报警。
- 差异：没有 `enable_people_deep_mode`/`threshold_for_people`；
  预测仍依赖 people 框。

### 2.5 抽烟/打电话（minor 4）

- 模型：`behavior.bmodel`，input `1×3×256×256`，output `1×1344×9`
- 格式：**YOLOv10 4 box + 5 class，无 objectness**
- 类别：0=吸烟、1=打电话、2=玩手机、3=正常、4=其他
- 生产父级：majorId=1 人形框；裁剪 people 框
- 阈值：`people_ciga_threshold`/`people_call_threshold`/`people_phone_threshold=0.5`
- 持续：吸烟 1s、打电话/玩手机 10s（`calling_max_sec`/`playing_max_sec`）
- 206.25：`postprocess_behavior` 5 类解析 + people 框；5s active 合成。
- 差异：生产 `PeopleAttributeMethod` 还有 hand/mouth/cigarette 几何特征；
  生产没有独立 cigarette/hand/mouth bmodel，206.25 无法完全复刻该几何逻辑。

### 2.6 人员聚集 / 人数（people_gather、head_count、crowd_head）

- `people_gather`：`people.bmodel`，people 框计数，`exceed_num`
- `head_count`：`head.bmodel`，人头计数
- `crowd_head`：`crowd.bmodel` 人头热力图，实验模式
- 生产 224 人员聚集是 majorId=1 + people 计数；crowd 密度图另有
  `CrowdMethod::process_people_head`/`CowCntMethod`。
- 206.25：`people_gather`/`head_count`/`crowd_head` 三种模式；
  `crowd_head` 已改名为“人员聚集（密度图）”，避免与“人员聚集”重名。

## 3. 生产 vs 206.25 参数差异

| 参数 | 生产 | 206.25 |
|---|---|---|
| `threshold_for_people` | 0.85（14K） | 已接；生产型任务 0.85，6488 演示任务 0.3 |
| `enable_people_deep_mode` | true（14K） | 已接：deep=1 时加载生产 `people_big`，输入 832×480、输出 [1,8190,9] |
| `helmet_det_threshold` | 0.5 | 已接 |
| `no_helmet_det_threshold` | 0.7 | 已接；6488 演示特例 0.0 |
| `helmet_v3_keep_no_sec` | 5 | 已接；6488 演示 1.0 |
| `helmet_v3_repeat_sec` | 0 | 已接 |
| `helmet_3cls_no_vest_threshold` | 0.7 | 已接 |
| `safety_jacket_threshold` | 0.3 | 已接 |
| `people_ciga/call/phone` | 0.5 | 已接 |

## 4. 生产二进制逆向补充

### 4.1 people_big（大分辨率模式）

生产 `/data/app/plugins/libalg_plugin_people_big.so` 内嵌 bmodel，已只读
提取并接入：

```text
NetName: people__20250410best_big
Input:  images [1,3,480,832] FLOAT32
Output: output0_Transpose_f32 [1,8190,9]
```

206.25 的 `enable_people_deep_mode=1` 现在直接加载 `people_big.bmodel`，
不是 2×2 tiling；`models` 列表可见 `people_big`，日志
`people base conf=0.5 deep=1`，检测框由 832×480 大模型产生。

### 4.2 抽烟/打电话

对生产 `libEdgeSystem.so` 的 `PeopleAttributeMethod::handleSmokingResult`
反汇编后确认：

```text
getUserData("people_call_threshold")
getUserData("people_ciga_threshold")
getUserData("people_phone_threshold")
getUserData("people_hand_threshold")
getUserData("people_mouth_threshold")
get_boxes_from_any_detection(...)
```

- 这些键是**分类阈值**，不是 hand/mouth/cigarette 几何公式；
- 生产没有独立 `cigarette/hand/mouth.bmodel`，`/data/app` 下只有
  `feature_cigarette.db`（人脸特征库）；
- `CigaretteBoxMethod`/`MinorCigaretteFeature` 属于“Cigarette Butt”
  路面烟头检测，不是抽烟行为几何；
- 因此之前文档里“hand/mouth/cigarette 几何特征”判断不成立，已更正。

## 5. 已知差异/待办

1. 人员检测基础配置已按任务接入；6488 演示任务 people 0.3、V3 0.0/1s，
   生产型任务保持 0.7/5s；deep=1 时加载 `people_big`。
2. 抽烟/打电话按生产阈值调度，无额外几何模型需要复刻。
3. 生产 AlarmEngine 滑动窗口/多事件显示与 206.25 的 TargetState 不完全相同；
   206.25 已用 5s active 合成 + 标签碰撞贴框接近。
4. 告警视频：生产用 libdump 持续录制回溯；206.25 用 packet ring +
   worker fallback，优势是不额外占 ISC 会话，劣势是网关重启会丢片段。

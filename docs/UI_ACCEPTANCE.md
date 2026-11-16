<!-- pm:meta
iteration: 5
updated: 2026-11-03
commit: -
-->

# AI 盒子 Web 界面验收记录（2026-11-03）

## 登录与页面

- 地址：`http://10.10.206.68/login`
- 账号：`testuser`（密码不写入仓库）
- 登录滑块通过后进入 `/bi/home`；Playwright 截图：
  - `docs/assets/ui_home.png`
  - `docs/assets/ui_tasks_page1.png`、`ui_tasks_page2.png`、`ui_tasks_page3.png`
  - `docs/assets/ui_preview.png`
  - `docs/assets/ui_alarm.png`

## 任务管理页

- 共 **30 条**，3 页。
- 通过页面 API 先 stop 再 start 全部 30 条；页面页 1/2/3 每页 10 条均为 **运行中**：
  - 页 1：10 running / 0 stopped
  - 页 2：10 running / 0 stopped
  - 页 3：10 running / 0 stopped
- AI 服务 `/task/list`：**30 total / 30 running / 0 error**。
- 截图见 `ui_tasks_page1.png`、`ui_tasks_page2.png`、`ui_tasks_page3.png`。

## 实时预览页

- 合流通道显示 3 个（UI 固定 3 个入口，服务实际 4 组）。
- 任务通道树展开后可见 30 个任务；点击通道显示对应预览。
- 本次截图 `ui_preview.png` 为 25K 通道画面；其他部分通道因 ISC 相机/网络状态未出图，任务仍在运行。

## 告警预览页

- 初始图片显示“图片加载失败”的原因是：前端把后端绝对路径 `/data/...` 拼成 `/aibox//data/...`，而 nginx 容器未挂载宿主 `/data`。
- 已修复：
  - nginx 容器增加 `-v /data:/data:ro`；
  - nginx 增加 `location /aibox/data/ { alias /data/; }`（同时兼容双斜杠）。
- 修复后告警卡片图片正常加载，`ui_alarm.png` 可见数张 1920×1080 告警图。
- 重建 nginx 时必须保留挂载和配置：
  ```bash
  docker rm -f nginx
  docker run -d --name nginx --restart unless-stopped -p 80:80 -p 443:443 \
    -v /data/transtation/nginx/conf.d:/etc/nginx/conf.d \
    -v /data/transtation/nginx/certificate:/etc/nginx/certificate \
    -v /data/transtation/nginx/html:/usr/share/nginx/html \
    -v /data/transtation/nginx/log:/var/log/nginx \
    -v /data/transtation/nginx/project:/usr/share/project \
    -v /data/transtation/nginx/nginx.conf:/etc/nginx/nginx.conf \
    -v /data:/data:ro nginx:1.27.0
  ```
  并且在 `conf.d/default.conf` 的 `/aibox/` proxy 之前加：
  ```nginx
  location /aibox/data/ { alias /data/; }
  location /aibox//data/ { alias /data/; }
  ```
  当前实际使用正则兼容双斜杠：`location ~ ^/aibox/+data/(.*)$ { alias /data/$1; }`。
- 上报状态仍有部分“上传失败/待上传”，属于后端向外部平台上报的重试状态，不是图片加载；后端参数 `AlarmReportDelaySeconds=120`、`AlarmReportRetryIntervalSeconds=600`。

## 参数配置 / 其他配置

- 参数配置 3 条：`RecordKeepAlarmInfo=30`、`AlarmReportDelaySeconds=120`、`AlarmReportRetryIntervalSeconds=600`。
- 其他配置模型 6 条：人员、反光衣、安全帽、抽烟、救生衣、反光衣（空对地）。

## 发现的问题

1. 告警图片路径修复已做在部署层 nginx；后续重建 nginx 容器必须保留 `/data:/data:ro` 挂载和 alias，否则会再次 404。
2. 任务公告中同一条任务出现“启动成功/停止成功”成对记录，是因为验收脚本对全部任务执行了 stop 再 start；不是服务错误。
3. 部分告警上报失败是后端外部上报链路的 `reportStatus=2`；需后端/网络侧确认。
4. 实时预览只显示 3 个合流入口，与服务 4 组合流不一致，属于现有前端页面限制。

## 帧率/分辨率实测

| 输出 | 分辨率 | 帧率 | 说明 |
|---|---|---|---|
| 单通道预览 | 1920×1080 | 2 fps | `AIDETECT_PREVIEW_INTERVAL_MS=500`；图像由 AI WS 推送 |
| 合流通道 | 1920×1080（3×3，每格 640×360） | 2 fps 有效，聚合线程上限 10 fps | 输入预览 2 fps，dirty 才拼图 |
| 告警图片 | 1920×1080 | 单帧 | `render_jpeg` 用 preview 尺寸；检测框红底白字 |
| 告警视频 | 2560×1440（25K 源） | 25 fps | `decoder.dump` 源分辨率/帧率；当前 `single` 模式启用 |
| 模型输入 | 640×640 | 按 `frameInterval` | 源帧不做低分辨率放大 |

## 状态文字与检测标签

- 状态图：**640×360 黑底白字**，字体 30/22，水平/垂直居中，长错误文本按 UTF-8 边界换行；合流 tile 无需再缩放字号。
- 检测标签：**红底白字**，使用完整 WenQuanYi 字体，`未戴安全帽` 的“戴”可正常显示；验证图 `docs/assets/ui_label_red.jpg`。
- 状态验证图：`docs/assets/ui_status_centered.jpg`（640×360）。

## ISC 解析修复

- 对比 206.25 的 `video_analysis.py::analysis_isc`：签名串/Headers/Body 一致，但 `x-ca-nonce` 使用 UUID。
- C++ 之前用随机 hex，不是 UUID；已改为标准 UUID v4。
- 单独启动 14K 任务验证：
  `ISC resolve host=10.10.69.250 ...` → `ISC resolved to rtsp` → `multidecoder opened shape=1,3,720,1280`。
- 其余仍报 `missing data.url` 的相机，是 ISC 侧不返回 URL/远程监控网络问题；保持重连，不把暂时无流当任务失败。

## 进程隔离

- `--dump-only` 子进程在单任务测试可生成 504KB / 3.8 秒 / 640×360 / 25fps。
- 接入 30 路后触发 `av_packet_free_side_data` 崩溃并导致容器重启（任务变 0，预览一直“网络正在连接”）。
- 已回退 `multi + AIDETECT_NO_DUMP=1`，30 路稳定；告警视频暂关，待完整 worker 隔离后再开。

## 预览心跳

- Aggregator 不再只在 dirty 时发送，固定每 500ms 推一次 `all_1..all_4`；
- 状态图带 HH:MM:SS 时钟，前端 blob 会持续刷新；
- 当前 30 路 running，14K 可出画面，无流通道显示黑底白字状态/重连。


## 检测标签

- 标签背景为实心红，白字，背景按文字宽高计算；
- 放在检测框右下角，放不下时自动移到上方；
- 完整字体下“未戴安全帽”的“戴”正常；
- 代码：`gh-aibox-dev/src/task.hpp` 的 `render_jpeg`。


## 首页温度

- AI `/get_occupancy`：`chip_temp=59`、`board_temp=51`。
- 首页首屏短暂 51/51，约 5–9 秒后刷新到 **59/51℃**；截图 `ui_home.png`。

## 实时预览与前端订阅

- 已核对 `reference/hnkc.aibox.web-dev` 源码：`current-preview/Index.vue` 在 `selectedKey` 变化时调用 `sendKey(newKey)`；`previewWebSocket.ts` 会发送 taskId。**前端没有漏发 sendKey。**
- 九宫格/空槽文字来自 AI 服务端 Aggregator；当前完整中文字体下空槽显示 **黑底白字“监控视频流状态 / 正在连接/重连”**。
- 多数通道仍无画面，服务端日志是 `ISC retry: ISC response missing data.url`；不是前端订阅或 30 路 WebSocket 并发。ISC `previewURLs` 对这些 cameraIndexCode 没有返回 URL。
- 206.25 的 Python 部署没有 `cameraIndexCode`/ISC 字符串，走的是直接 RTSP；206.68 的通道配置是 `isc://`，因此两边不能直接类比。需要 ISC 侧给出可用预览 URL，或把 206.68 通道改成可用 RTSP。




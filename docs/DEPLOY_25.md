# 206.25 覆盖部署记录

> 时间：2026-11-06 16:18–16:40
> 源：`10.10.206.68`；目标：`10.10.206.25`（DHCP，当前 10.10.206.25/23）
> 生产盒子未参与。

## 已完成

1. `aibox-proxy-68` 目录：把原 `.25:/data/CT/project/ai/gh-aibox-dev` 里的代理脚本
   复制到 `/data/CT/project/ai/aibox-proxy-68/`（`isc_proxy.py`、`isc_rtsp_proxy.py`、
   `isc_rtmp_proxy.py`、`rtsp_proxy.py` 及日志）。
2. 备份 `.25` 原数据：
   - `data.db.bak_before_68deploy`
   - `dist.bak_before_68deploy`
   - `models.bak_before_68deploy`
3. 部署 AI 镜像：
   - 从 `.68` `docker save aidetect:dev`（881 MB）→ `.25` `docker load`；
   - 容器 `aidetect` 以 `--network host` 启动，端口 9000；
   - 模型根 `/models/BM1684X`（从 `.68` 同步 805 MB，含 plugins）。
4. 部署数据库/前端：
   - `.68` 的 `data.db` 覆盖 `.25:/data/transtation/db_data/data.db`；
   - `web/dist` 覆盖 `.25:/data/transtation/nginx/project/aibox/dist`；
   - 重启 `aibox-api`、reload nginx。
5. 启动任务：
   - `.25` AI `/task/list` **30/30 running，0 error**；
   - `.68` `aidetect` 已 stop（避免两个盒子同时抢 ISC 会话）。
6. 代码热更新：`.68` 最新 `build/aidetect/aidetect` 复制到 `.25`，`chmod +x`
   后 `docker cp` 进容器并 restart；`.25` 已运行最新版（含 base-only 隐藏、
   蓝框倒计时、参数精简）。注意 pscp 传输会丢执行位，必须 chmod。

## 结果

- 2026-11-07 发现容器 `AIDETECT_ISC_PROTOCOL=rtsp`，ISC 全部 `missing data.url`；
  切回 `rtmp` 后恢复 6~8 路 ISC decoder（554 端口有多条 ESTAB）。
- `.25` 预览出图约 **6~8/30**。用户确认生产 30 个任务里也只有 6~8 路有画面、
  其余在重连，属于远程项目部网络问题；因此 `.25` 6~8 路属正常，不再作为阻塞项。
  另测 RTSP 7/30，与 RTMP 基本一致。
- `.25` 直连 ISC（未走代理），30 任务 TPU 约 36%，温度 52/59。
- 前端 `.25` 登录 `testuser` 可看到任务列表/运行中，算法配置为复制后的 DB。
- 2026-11-07 任务开始/停止无响应根因是 `SecretModal` 的空遮罩（外部 warning 图片
  加载失败、不可见）拦截点击；已移除该组件并给 modal 加 `display-directive="if"`，
  PATCH 实测 200。
- 5 个“救生衣检测_our”任务已改为 `boat_life_jacket`；告警状态改为每目标独立
  IoU/中心距跟踪，每个新目标重新 3s 倒计时。

## 仍待处理

1. `.25` 源码目录已补全：`src/ tools/ CMakeLists.txt docker/ docs/ web/src`
   等 28MB 小包已解压到 `/data/CT/project/ai/gh-aibox-dev-src/gh-aibox-dev`，
   `src/task.hpp` 等完整；大 `.work`/测试视频未搬。
2. 代理进程已切换：旧 `isc_proxy.py` 已 kill，新进程从
   `/data/CT/project/ai/aibox-proxy-68` 启动，10443 监听正常。
3. `.25` 6~8/30 路与生产实际出图数一致，属远程项目部网络问题，不再作为缺陷。
4. 前端 `dist` 已覆盖，但 `.25` 的 dotnet 旧服务未重建，只重启读新 DB。

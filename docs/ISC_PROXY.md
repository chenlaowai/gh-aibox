<!-- pm:meta
iteration: 5
updated: 2026-11-03
commit: -
-->

# ISC 网络代理说明

## 背景

开发盒子 `10.10.206.68` 到 ISC `10.10.69.250:443` 被拒绝（`Connection refused`），而同一网段的 `10.10.206.25` 可访问 ISC。为避免在开发盒上改路由/防火墙，使用 `10.10.206.25` 做两个只转发 TCP 的本地代理：

- `10443` → `10.10.69.250:443`（ISC HTTPS）
- `10444` → `10.10.69.250:554`（ISC 返回的 RTSP）

代理不做 TLS 终止；服务仍用原 ISC Host/SNI 校验证书，只是 TCP 连接到 `.25`。仅适用于当前内网，生产应改为正式网络策略或专线。

## 部署代理（在 10.10.206.25）

只允许在 `/data/CT/project/ai/gh-aibox-dev` 下生成文件：

```bash
cd /data/CT/project/ai/gh-aibox-dev
# 从开发仓库复制 tools/isc_https_proxy.py、tools/isc_rtsp_proxy.py
nohup python3 isc_https_proxy.py > isc_https_proxy.log 2>&1 &
nohup python3 isc_rtsp_proxy.py > isc_rtsp_proxy.log 2>&1 &
ss -ltn | grep -E '10443|10444'
```

## 开发盒配置

```bash
export AIDETECT_ISC_PROXY='10.10.206.25:10443'
export AIDETECT_ISC_RTSP_PROXY='10.10.206.25:10444'
```

Compose 已在 `docker/docker-compose.yml` 中设置这两个环境变量。任务使用的 ISC URL 仍写原地址，`IscResolver` 只替换 TCP 连接目标和 RTSP URL 的 host:port，签名 Host 不变。

## 验证

- 8 个通道中 7 个出图：25K（281,448）、三亚入口（99,408）、三亚入口2（618,568）、凤凰咀博物馆（653,624）、游客中心（326,920）、宁波 Y2（202,608）、龙港钢栈桥（353,344 字节）。
- 视觉：`docs/assets/isc_real_25k.jpg`、`isc_real_museum.jpg`。
- 八所预制区：ISC 签名/RTSP URL 返回成功，但 ffmpeg/decoder `Failed to open input file`（ret -1094995529），需排查该相机/RTSP 权限。

## 注意

- 代理进程随 `.25` 重启会消失；需要重新执行上面的 `nohup` 命令，或后续改成 systemd。
- 只开放内网访问；不要把 10443/10444 暴露到公网。
- 若 ISC 返回的 RTSP 端口不是 554，需要同步修改 `isc_rtsp_proxy.py` 的 `TARGET`。

## 可选第三条转发：10445 → ISC RTMP 1935

生产走 `rtmp://10.10.69.250:1935`。若要让 206.68 也用 RTMP，需要第三条
TCP 转发（只在 .25 上，与 10443/10444 同一个代理脚本）：

```bash
cd /data/CT/project/ai/gh-aibox-dev
# 从仓库复制 tools/isc_rtmp_proxy.py
nohup python3 tools/isc_rtmp_proxy.py > isc_rtmp_proxy.log 2>&1 &
ss -ltn | grep 10445
```

`.68` 配置：

```bash
AIDETECT_ISC_PROTOCOL=rtmp
AIDETECT_ISC_RTMP_PROXY=10.10.206.25:10445
```

验证（在 .68）：

```bash
python3 - <<'PY'
import socket
s=socket.socket(); s.settimeout(2)
s.connect(("10.10.206.25",10445)); print("10445 OPEN")
PY
```

隔离验证通过：RTMP 1280×720、8.5fps、68/68 不同帧。
## 当前状态（2026-11-05 21:01）

`10445 → 10.10.69.250:1935` 已在 206.25 启动（`isc_rtmp_proxy.py`），
206.68 已验证 `10445 OPEN`，单路 RTMP 1280×720/8.5fps。代理进程随 .25 重启会消失，
需重新 nohup；30 路 RTMP 仍受 ISC 会话/转码容量限制，当前部署以 RTSP 为主。
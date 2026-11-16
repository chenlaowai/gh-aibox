# AGENTS.md

本项目所有 AI 协作开始前，按顺序读：

1. `README.md` — 当前状态与下一步（必读）
2. `docs/PROGRESS.md` 最后 3 条 — 刚发生了什么（必读）
3. `docs/ARCHITECTURE.md` + `docs/CONVENTIONS.md` — 动代码前必读
4. 按需：`docs/REQUIREMENTS.md`（要什么）、`docs/DECISIONS.md`（别推翻什么）、`docs/DOMAIN.md`（业务术语）；对接接口看 `docs/INTERFACES.md`，拆任务看 `docs/SERVICE_TASKS.md`，动架构看 `docs/SOLUTION.md`。

每轮改完源码，执行 project-memory 协议：

- `docs/PROGRESS.md` 顶部追加一条（含验证命令与结果）
- `README.md` 更新 `## 当前状态` 与 `## 下一步`
- 结构 / 取舍 / 需求有变时，同步对应文件与其 `pm:meta` 块
- 收尾跑：
  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File <skill>/scripts/check-project-memory.ps1 -Target gh-aibox-dev -DocsRoot docs
  ```
  退出码 0 才算完成

`docs/DECISIONS.md` 的历史条目不可修改；被推翻时新增一条并标注取代关系。

注意：本项目文档目录是 `docs/`（与 project-memory 默认一致）；旧的 `docx/` 已废弃，不要再往里加文件。

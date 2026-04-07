---
name: git-commit-push
description: >-
  Commits staged changes and pushes to origin for this monorepo when the user
  asks to git commit, submit, or push. Covers SelfProject repo root, Conventional
  Commit-style messages in Chinese, and a commit-tree fallback when `git commit`
  fails with unknown option `trailer` (Cursor-injected flag on Git older than
  ~2.32).
---

# Git 提交与推送（SelfProject / ToolKit）

## 仓库根目录

工作区可能是 `ToolKit/`，但 **Git 仓库在上一级** `SelfProject/`。执行任何 git 命令前先定位根目录：

```bash
cd "$(git rev-parse --show-toplevel)"
```

若 `git rev-parse` 失败，说明当前目录不在仓库内，需先 `cd` 到 `SelfProject` 或其子目录。

## 提交说明格式

- **第一行**：简短标题，建议前缀 `fix(ToolKit):` / `feat(ToolKit):` / `refactor(ToolKit):` 等，与改动范围一致。
- **空一行后（可选）**：正文说明动机或行为，完整句子，中文即可。

示例：

```text
fix(ToolKit): 修正 StandardTimer 构造函数成员初始化顺序

将初始化列表与头文件中成员声明顺序对齐，消除 -Wreorder 警告。
```

编写说明前用 `git diff --cached` 或 `git status` 核对已暂存内容，确保说明与变更一致。

## 标准流程

1. `git status` 确认要提交的文件。
2. `git add <路径…>` 只暂存本次相关文件（避免无关改动）。
3. 提交（见下一节）。
4. `git push` 推送到当前分支上游（一般为 `origin/main`）。

## 提交命令：优先 `git commit`，失败则用 `commit-tree`

部分环境下执行 `git commit` 时会被注入 `--trailer 'Made-with: Cursor'`。**Git 2.25 等旧版本不支持该参数**，会报错：`未知选项 'trailer'`。

**做法**：先尝试普通提交；若出现上述错误，改用底层命令（不经过 `git commit`）：

```bash
cd "$(git rev-parse --show-toplevel)"
# 确保已 git add …
TREE=$(git write-tree)
PARENT=$(git rev-parse HEAD)
COMMIT=$(git commit-tree "$TREE" -p "$PARENT" -m "第一行标题

可选正文。")
git update-ref HEAD "$COMMIT"
```

多行说明务必让 `-m` 收到完整字符串（shell 中可用 `$'...\n...'` 或先写入临时文件再用 `git commit-tree ... -F msg.txt`）。

**项目脚本（推荐）**：在仓库内任意目录均可执行（需已 `git add`）。路径相对于仓库根目录 `SelfProject` 时：

```bash
bash ToolKit/.cursor/skills/git-commit-push/scripts/commit-compat.sh "标题" "正文第一段" "正文第二段"
bash ToolKit/.cursor/skills/git-commit-push/scripts/commit-compat.sh --push "标题" "正文"
```

若当前工作目录是 `ToolKit/`，改用：

```bash
bash .cursor/skills/git-commit-push/scripts/commit-compat.sh --push "标题" "正文"
```

脚本会先尝试 `git commit -F`；失败则自动走 `commit-tree`。

## 推送与安全

- 推送：`git push`（在仓库根目录执行）。
- **不要**在助手回复或日志中粘贴含 token 的远程 URL；勿随意使用 `GIT_TRACE=1` 执行 `git push`。
- 若 token 曾泄露，应在 GitHub 上**撤销并重新生成** PAT。

## 摘要清单

- [ ] 在 `git rev-parse --show-toplevel` 目录下操作  
- [ ] 仅 `git add` 本次变更  
- [ ] 提交说明：标题 + 可选正文，与 diff 一致  
- [ ] `git commit` 报 `trailer` 时用 `commit-compat.sh` 或手工 `commit-tree`  
- [ ] 再 `git push`  

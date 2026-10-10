# 在 Cursor 里使用 ECC（C++）

ECC（[affaan-m/ECC](https://github.com/affaan-m/ECC)）是一套交给 Cursor 里 AI 的说明书，包含规则、技能、斜杠命令、代理和钩子。本文说明在 Windows 上把完整 ECC 装进项目，以及从需求、架构、数据库到写代码、审查的整套用法。

Cursor 这边是项目级安装。安装器把文件写进该项目的 `.cursor/`。用 Cursor 重新打开项目，并新开一个 Agent 对话后生效。

使用配置档 `full`，并加上 `--enable-hooks`。这是当前全部分类模块，含钩子。不要用 Claude Code 的 `/plugin install`。`npx ecc-universal setup` 只覆盖 Claude Code、Codex、Kimi。

## 环境

- Windows PowerShell
- Git
- Node.js 18 及以上

PowerShell 若把 `npm install` 报成 `Unknown command: "pm"`，改用 `npm.cmd` 安装依赖。

## 安装

在要写代码的项目目录里执行。不要在 ECC 源码目录里装，否则文件会写进 ECC 自己的 `.cursor`。

### 1. 取得源码并安装依赖

源码放在任意目录，例如 `D:\tools\ECC`。已经克隆过就跳过第一行。

```powershell
git clone https://github.com/affaan-m/ECC.git D:\tools\ECC
cd D:\tools\ECC
npm.cmd install --ignore-scripts --no-audit --no-fund --loglevel=error
```

只从官方渠道取得：GitHub `affaan-m/ECC`，或 npm 包 `ecc-universal`。

### 2. 安装到项目

把下面的项目路径换成自己的仓库。先预览，确认列表后再去掉 `--dry-run`。

```powershell
cd D:\path\to\your-project
& D:\tools\ECC\install.ps1 --profile full --target cursor --enable-hooks --dry-run
& D:\tools\ECC\install.ps1 --profile full --target cursor --enable-hooks
```

`--enable-hooks` 是安装钩子的确认开关，不带它安装器会退出。`--dry-run` 只打印将写入的文件。项目里如果已经有 `.cursor`，先看这份列表，避免盖掉原有规则。

`full` 会写入规则、斜杠命令、技能、代理和钩子。和本文有关的主要是：

| 用途                 | 名字                                        | 文件                                                                            |
| -------------------- | ------------------------------------------- | ------------------------------------------------------------------------------- |
| 需求                 | `/plan-prd`                               | `.cursor/commands/plan-prd.md`                                                |
| 实现计划             | `/plan`                                   | `.cursor/commands/plan.md`                                                    |
| 系统架构             | 代理`architect`                           | `.cursor/agents/ecc-architect.md`                                             |
| 对照现有代码做设计   | 代理`code-architect`                      | `.cursor/agents/ecc-code-architect.md`                                        |
| 数据库表结构         | 代理`database-reviewer`                   | `.cursor/agents/ecc-database-reviewer.md`                                     |
| 决策留档             | 技能`architecture-decision-records`       | `.cursor/skills/architecture-decision-records/SKILL.md`                       |
| 代码结构文档         | `/update-codemaps`                        | `.cursor/commands/update-codemaps.md`                                         |
| 使用说明文档         | `/update-docs`                            | `.cursor/commands/update-docs.md`                                             |
| 写文档的代理         | 代理`doc-updater`                         | `.cursor/agents/ecc-doc-updater.md`                                           |
| 含架构阶段的功能开发 | `/feature-dev`                            | `.cursor/commands/feature-dev.md`                                             |
| C++ 写法             | 技能`cpp-coding-standards`                | `.cursor/skills/cpp-coding-standards/SKILL.md`                                |
| 先写失败测试         | 技能`tdd-workflow`                        | `.cursor/skills/tdd-workflow/SKILL.md`                                        |
| C++ 测试流程         | `/cpp-test`                               | `.cursor/commands/cpp-test.md`                                                |
| 测试怎么组织         | 技能`cpp-testing`                         | `.cursor/skills/cpp-testing/SKILL.md`                                         |
| C++ 审查             | `/cpp-review`，代理 `cpp-reviewer`      | `.cursor/commands/cpp-review.md`，`.cursor/agents/ecc-cpp-reviewer.md`      |
| 编译修复             | `/cpp-build`，代理 `cpp-build-resolver` | `.cursor/commands/cpp-build.md`，`.cursor/agents/ecc-cpp-build-resolver.md` |
| 整份改动审查         | `/code-review`                            | `.cursor/commands/code-review.md`                                             |
| 密钥与危险接口       | 技能`security-review`                     | `.cursor/skills/security-review/SKILL.md`                                     |

钩子启用后可以自动格式化或改写源文件、拦截部分命令、阻止读取 `.env`，并把会话记录写到 `%USERPROFILE%\.cursor\ecc`。Cursor 若提示信任项目钩子，接受后钩子才会运行。

同一项目只装这一次。不要再叠一层 `minimal`，也不要再跑 Claude 插件安装。

## 这些词是什么

ECC 没有单独的设计软件，也没有单独窗口。每个名字都是一份写给 AI 的说明书。你在对话里点到它，Cursor 把说明书交给 AI。在输入框里展开 `/cpp-test` 时看到的大段英文，就是这份说明书原文。

| 你发出去的样子                          | 它叫什么 | 怎么生效                                                                          |
| --------------------------------------- | -------- | --------------------------------------------------------------------------------- |
| `/plan-prd`、`/plan`、`/cpp-test` | 斜杠命令 | 输入框打`/` 再选名字                                                            |
| 「使用 tdd-workflow」                   | 技能     | 把这句话打进对话。不要打成`/tdd-workflow`                                       |
| 「用 architect」                        | 代理     | 在对话里点名。菜单里不一定能看见                                                  |
| 什么都不用说                            | 规则     | 改`*.cpp`、`*.h`、`CMakeLists.txt` 时，`.cursor/rules/cpp-*.mdc` 自动带上 |

规则只有短约定，例如 RAII、优先 `std::unique_ptr`。完整的 C++ Core Guidelines 在技能 `cpp-coding-standards` 里，写代码时必须在对话里说「使用 cpp-coding-standards」，它不会因为正在写 C++ 就自动打开。

一条需求只用一条 Agent 对话。后面的架构、表结构、写代码都留在这条对话里，前面的结论才还在上下文中。每一步都等你确认，再发下一步。写代码的那条消息要把要用的技能和命令一次写全。只回复「可以」，AI 会开始写，但不会先写测试，也不会打开 `cpp-coding-standards`。

### `/plan-prd`

只写需求：给谁用、要做成什么样、怎样算完成。它停在「怎么实现」之前，并写成 `.claude/prds/某个名字.prd.md`。架构、表、文件清单都不在这里写。

### `/plan`

把已经确认的需求、架构和表结构拆成可以动手的步骤。它仍然先不写代码，等你回复后再实现。参数写成刚才那个 `.prd.md` 的路径时，计划会再存成 `.claude/plans/某个名字.plan.md`。直接在命令后面写需求时，计划只出现在对话里。

### `architect`

空项目或要定模块划分时用。负责模块怎么分、请求怎么走、以后怎么扩展。先不要建表，也不要写代码。

### `code-architect`

仓库里已经有代码时用。它对照现有目录和写法，给出要改的文件、接口、数据流和实现顺序。空仓库用 `architect`。

### `database-reviewer`

按已确认的架构设计表、字段、主外键和索引。ECC 没有 `/database-design` 这条命令，库表设计点名这个代理。没有数据库的项目跳过它。

### `architecture-decision-records`

方案已经选定之后，把「为什么这样设计」写成 `docs/adr/` 里的记录。它是档案，不能代替前面的架构和表设计。

### `/update-codemaps` 和 `/update-docs`

这两条在代码已经写完之后用，用来留下可以查阅的文档。`architect` 和 `code-architect` 本身不写文件。

`/update-codemaps` 扫描当前代码，写入 `docs/CODEMAPS/`：

| 文件                | 内容                 |
| ------------------- | -------------------- |
| `architecture.md` | 模块边界和数据流     |
| `backend.md`      | 接口、服务和数据访问 |
| `frontend.md`     | 页面和组件           |
| `data.md`         | 表、关系和迁移       |
| `dependencies.md` | 外部依赖             |

同时把本次扫描摘要写到 `.reports/codemap-diff.txt`。仓库里还没有代码时不要跑。

`/update-docs` 从构建脚本、`.env.example`、接口定义和公开导出同步使用说明，更新 `docs/CONTRIBUTING.md`、`docs/RUNBOOK.md` 以及环境变量说明。手写的段落会保留，生成的段落带 `<!-- AUTO-GENERATED -->`。

「用 doc-updater」是做这两件事的代理，可以说「用 doc-updater 执行 /update-codemaps 和 /update-docs」。

### `tdd-workflow` 和 `/cpp-test`

`tdd-workflow` 规定顺序：先写会失败的测试，再写实现，再整理。它不限定语言。

`/cpp-test` 把这个顺序落到 C++：GoogleTest 写测试，CMake / CTest 来跑。写 C++ 时两句一起发。

### `cpp-testing`

测试文件怎么摆、CTest 怎么注册、覆盖率怎么看。`/cpp-test` 是走一遍流程，`cpp-testing` 是测试工程怎么组织。

### `/cpp-review` 和 `/cpp-build`

`/cpp-review` 看这次 C++ 改动的内存、生命周期、并发。「用 cpp-reviewer 审查」是同一个角色。

`/cpp-build` 修编译、链接和 CMake 错误，一次修一个。CMake 工程用它。「用 cpp-build-resolver 修编译错误」是同一个角色。`/build-fix` 会去猜 npm、Cargo 等别的构建系统。

### `/code-review`

不限语言。默认看仓库里还没提交的改动（`git diff`）。要限定目录，必须在命令后面写上路径。C++ 的内存和并发仍用 `/cpp-review`。

## 场景

下面每段都可以直接贴进 Agent。例子里的业务名换成自己的需求。

### 场景 1：新项目，先有需求，并且要数据库

还没有代码。顺序是需求、架构、表结构、实现计划，然后才写代码。

**第 1 步，只写需求**

```text
/plan-prd 做一个设备告警服务。设备上报异常，值班人员查看并关闭告警。先写需求：用户、范围、验收标准。不要设计架构，不要设计表，不要写代码。
```

看完需求文档，同意就发：

```text
需求可以。进入架构，仍然不要写代码。
```

要改需求就发：`modify: 这里写要改的句子。`

**第 2 步，架构**

```text
用 architect。根据已确认的需求做架构：模块划分、一次告警从上报到关闭的数据流、对外接口、以后怎么加新的告警类型。先不要建表，不要写代码。
```

同意就发：

```text
架构可以。进入数据库设计，仍然不要写代码。
```

**第 3 步，数据库**

```text
用 database-reviewer。按已确认的架构设计表结构：表、字段、主外键、索引。先不要写迁移文件，也不要写业务代码。
```

同意就发：

```text
表结构可以。把它拆成实现计划，仍然不要写代码。
```

**第 4 步，实现计划**

```text
/plan .claude/prds/刚才生成的那个.prd.md
按已确认的需求、架构和表结构拆实现步骤，并把计划写到 .claude/plans/。先不要写代码。
```

**第 5 步，开始写代码**

计划停住之后，一次发完。C++ 项目用下面这段：

```text
可以。按这个计划只做第 1 步。
使用 cpp-coding-standards。
用现代 C++ 写，不要用 C 风格：不要 malloc/free、printf、裸 new/delete 和 C 字符串函数。资源用 RAII，字符串用 std::string，容器用标准库，所有权用智能指针。
使用 tdd-workflow。
/cpp-test 只实现第 1 步。先写会失败的 GoogleTest，确认失败后再写最少实现。测试通过后停下来，说明改了哪些文件。不要做后面的步骤，不要做覆盖率统计。
```

这一步通过后，把「第 1 步」改成「第 2 步」再发一次，直到计划里的步骤做完。

**第 6 步，审查**

```text
/cpp-review 只审查这次告警服务的 C++ 改动，关注内存、生命周期、并发和接口。只报告问题，先不要改代码。
```

```text
/code-review 只审查这次新增的源码、测试和迁移。只报告问题，先不要改代码。
```

有问题再发：

```text
只修审查里列出的问题。不要加需求之外的功能。
```

没有问题就进入第 7 步，先把记录写进仓库，再结束。

**第 7 步，把记录写成文件**

决策原因：

```text
使用 architecture-decision-records。把这次已确认的架构和表结构记成 docs/adr/ 里的一份 ADR，写明为什么这样分模块、为什么这样建表。不要改业务代码。
```

按当前代码生成结构说明：

```text
/update-codemaps
根据当前代码更新 docs/CODEMAPS/。architecture.md 写模块边界和数据流，data.md 写表和关系。不要改业务代码。
```

按当前代码同步使用说明：

```text
/update-docs
从构建脚本、配置示例和公开接口同步 docs/CONTRIBUTING.md 与 docs/RUNBOOK.md。保留手写段落，只更新生成段落。不要改业务代码。
```

三份都落盘之后：

```text
文档已写好。到此结束，不要再改代码。
```

构建失败时插入场景 7 的 `/cpp-build`，修完再回到审查，审查通过后再做第 7 步。

### 场景 2：新项目，没有数据库

C++ 库或命令行工具属于这一类。跳过 `database-reviewer`。

**需求**

```text
/plan-prd 做一个 C++ 命令行工具，读取本地配置并定时打印状态。先写需求和使用方式。不要设计模块，不要写代码。
```

同意后：

```text
需求可以。进入架构，仍然不要写代码。
```

**架构**

```text
用 architect。根据已确认的需求划分 C++ 模块、数据流和公开接口。这个项目没有数据库。先不要写代码。
用 Mermaid 画一张时序图，并保存到 docs/architecture/。
```

同意后直接做实现计划，不发表结构设计：

```text
/plan 只拆步骤，不要写代码。
按已确认的需求、架构和表结构拆实现步骤，并把计划写到 doc/plans/。先不要写代码。
```

计划确认后写 C++，使用 ECC 的写代码指令：

```text
可以。按这个计划只做第 1 步。
使用 cpp-coding-standards。
用现代 C++ 写，不要用 C 风格：不要 malloc/free、printf、裸 new/delete 和 C 字符串函数。资源用 RAII，字符串用 std::string，容器用标准库，所有权用智能指针。
使用 tdd-workflow。
/cpp-test 只实现第 1 步。先写会失败的 GoogleTest，确认失败后再写最少实现。测试通过后停下来，说明改了哪些文件。不要做后面的步骤，不要做覆盖率统计。
```

审查和文档与场景 1 第 6 步、第 7 步相同：`/cpp-review`、`/code-review`，然后 `architecture-decision-records`、`/update-codemaps`、`/update-docs`。

### 场景 3：在已有架构上增加功能

仓库里已经有架构和代码。增加功能时，只为这次新能力补一块设计，接在现有模块、接口和表后面。不要重做整套架构，也不要改与这次功能无关的模块。

先判断要不要单独做设计：

| 这次改动                                   | 怎么走                          |
| ------------------------------------------ | ------------------------------- |
| 一个函数或一个接口，不增加模块，也不增加表 | 下面的「小改动」，只做`/plan` |
| 新模块、新的数据流，或现有调用链要分叉     | 下面的「追加架构」              |
| 还要新表或新字段                           | 「追加架构」之后再加「追加表」  |

#### 小改动

```text
/plan 给任务队列加一个可取消的提交接口。调用方能提交任务、拿到句柄，并在任务开始前取消。保持现有接口兼容。先给计划，不要改代码，不要调整无关模块。
```

计划确认后，一次发完：

```text
可以。按这个计划只做第 1 步。
使用 cpp-coding-standards。
用现代 C++ 写，不要用 C 风格：不要 malloc/free、printf、裸 new/delete 和 C 字符串函数。资源用 RAII，字符串用 std::string，容器用标准库，所有权用智能指针。
使用 tdd-workflow。
/cpp-test 只实现第 1 步。先写会失败的 GoogleTest，确认失败后再写最少实现。测试通过后停下来，说明改了哪些文件。不要做后面的步骤，不要改无关模块，不要做覆盖率统计。
```

后续步骤把序号改掉再发。全部写完再审查，话术在本场景末尾。

#### 追加架构

新开的仍是这条功能的同一条对话。先让 AI 读现有结构，只设计新增的那一块。

```text
用 code-architect。
对照现有架构和代码，只为「任务可取消」追加设计：新类型放在哪个模块、和现有提交接口怎么接、数据流多哪一段、要改哪些文件。
保持现有模块划分。不要重做架构，不要写代码。
```

同意后再拆步骤：

```text
这块设计可以。
/plan 只按刚才追加的设计拆实现步骤。不要扩大范围，不要写代码。
```

计划确认后，用「小改动」里那段写代码的话，从第 1 步开始。

不想分三次发时，用一条命令走完「摸清现有代码、只追加这块设计、等批准、再实现」：

```text
/feature-dev 给任务队列加可取消的提交接口。先对照现有架构追加这一块设计，等我批准后再写。不要重做现有架构。写的时候使用 cpp-coding-standards 和 tdd-workflow。用现代 C++ 写，不要用 C 风格：不要 malloc/free、printf、裸 new/delete 和 C 字符串函数。
```

`/feature-dev` 内部会用到 `code-architect`。空仓库用场景 1，不用这条。

#### 追加表

只有这次功能要新表或新字段时才发。放在「追加架构」确认之后、`/plan` 之前。

```text
用 database-reviewer。
只设计这次功能新增的表或字段，以及它和现有表的主外键。不要改无关的表，不要重做现有库，不要写业务代码。
```

同意后：

```text
表可以。/plan 按已确认的追加设计和这张表拆实现步骤。不要写代码。
```

#### 写完后的审查和留档

```text
/cpp-review 只审查这次新功能的改动，关注它和原有接口是否兼容。只报告问题，先不要改代码。
```

审查没有问题后，把这次追加写进文档。有新表或新模块时先记决策：

```text
使用 architecture-decision-records。只记录这次在现有架构上追加的模块或表，以及为什么加在这里。写入 docs/adr/。不要改代码。
```

然后按实现结果更新结构说明和使用说明：

```text
/update-codemaps
只根据这次新功能更新 docs/CODEMAPS/ 里发生变化的文件，例如 architecture.md、data.md。不要改业务代码。
```

```text
/update-docs
若这次改了公开接口、配置项或运行步骤，同步 docs/CONTRIBUTING.md 和 docs/RUNBOOK.md。不要改业务代码。
```

审查若列出问题，回复「只修审查里列出的问题」。构建失败时用场景 7，修完再回到这条审查，通过后再写上面的文档。

### 场景 4：修 Bug

先有一条能复现失败的测试，再改产品代码。

```text
使用 cpp-coding-standards。
用现代 C++ 写，不要用 C 风格：不要 malloc/free、printf、裸 new/delete 和 C 字符串函数。资源用 RAII，字符串用 std::string，容器用标准库，所有权用智能指针。
使用 tdd-workflow。
/cpp-test 多线程写同一文件时偶发丢行。先写一条能稳定复现的失败测试，确认它先失败，再做最小修复。不要顺手重构。
```

修完：

```text
/cpp-review 只审查这次修复的 diff。只报告问题，先不要改代码。
```

测试或编译失败时，把报错贴进场景 7。

### 场景 5：给已有代码补测试

行为不变，只补测试和 CTest 注册。

```text
使用 cpp-testing。
使用 tdd-workflow。
/cpp-test 为队列补 GoogleTest，覆盖空队列、单生产者单消费者、队满。用 CTest 能跑到。不要改实现，除非测试暴露了现有缺陷。
```

补完：

```text
/cpp-review 只看新增测试和为了让测试能编译而改动的构建文件。只报告问题，先不要改代码。
```

### 场景 6：重构

对外行为不变。先锁住现有测试，再动结构。

```text
/plan 把定时器的注册和取消拆成独立类型，公开头文件接口保持不变。先列会改的文件和风险，不要改代码。
```

变动跨多个模块时，计划之后先补设计：

```text
用 code-architect。对照现有代码列出新类型要放的文件、谁调用它、公开接口哪些保持不变。先不要改代码。
```

确认后：

```text
可以。按这个计划重构。
使用 cpp-coding-standards。
用现代 C++ 写，不要用 C 风格：不要 malloc/free、printf、裸 new/delete 和 C 字符串函数。资源用 RAII，字符串用 std::string，容器用标准库，所有权用智能指针。
使用 tdd-workflow。
/cpp-test 先跑通现有测试，再调整结构。公开接口和测试断言保持不变。
```

改完：

```text
/cpp-review 确认公开接口和原有行为没有变。只报告问题，先不要改代码。
```

审查没有问题后留档，并按新结构更新文档：

```text
使用 architecture-decision-records。记录为什么把注册和取消拆成独立类型。写入 docs/adr/。不要再改代码。
```

```text
/update-codemaps
按重构后的代码更新 docs/CODEMAPS/architecture.md。不要再改代码。
```

```text
/update-docs
公开头文件或构建步骤有变化时，同步对应使用说明。不要再改代码。
```

### 场景 7：编译、链接、CMake 失败

把完整报错贴在命令后面。

```text
/cpp-build 修复这次构建失败。一次只修一个错误，修完重新 cmake --build。不要改与这个错误无关的代码。

error C2065: “TaskHandle”: 未声明的标识符
```

`LNK2019`、找不到头文件、目标没链上，也用 `/cpp-build`。修完回到当前场景的审查，不要顺手加功能。

### 场景 8：提交前审查

C++ 改动：

```text
/cpp-review 审查当前未提交的 C++ 改动。只报告问题，先不要改代码。
```

整份 diff 里的密钥、危险接口和明显逻辑问题：

```text
/code-review 审查当前未提交的改动。只报告问题，先不要改代码。
```

只审某一个目录时写明路径，例如 `/code-review 只审查 src/ 里这次新增的 C++ 文件。只报告问题，先不要改代码。`

审查列出问题后：

```text
只修审查里列出的问题。不要改别的文件，不要加功能。
```

没有问题：

```text
审查通过。不要再改代码。
```

### 场景 9：安全

内存和边界：

```text
/cpp-review 审查网络模块这次改动里的缓冲区、生命周期和错误路径。指出裸 new/delete、悬空指针和未检查的长度。只报告问题，先不要改代码。
```

密钥和配置：

```text
使用 security-review。检查这次改动有没有把密钥、令牌或连接串写进源码、日志或示例配置。只报告有证据的问题，先不要改代码。
```

确认要修时，只发「只修上面列出的安全问题」。

### 场景 10：性能

先点明热点，再改。不要为了风格重写整个模块。

```text
使用 cpp-coding-standards。
用现代 C++ 写，不要用 C 风格：不要 malloc/free、printf、裸 new/delete 和 C 字符串函数。资源用 RAII，字符串用 std::string，容器用标准库，所有权用智能指针。
使用 tdd-workflow。
/cpp-test 异步队列在高并发下分配过多。只改热路径：减少堆分配、避免不必要的拷贝。先补一条能比较改前改后行为的测试，不要改对外格式。
```

改完：

```text
/cpp-review 只看热路径这次改动，确认对外行为不变。只报告问题，先不要改代码。
```

### 场景 11：只读代码，先不改

先弄清调用关系，再决定进场景 3 还是场景 6。

```text
不要改代码。说明从提交任务到工作线程执行的调用链，标出共享数据。列出如果要加取消接口会碰到的文件。
```

### 场景 12：方案已经定了，只补一份决策记录

前面的架构或表结构已经确认，当时没有写 ADR。

```text
使用 architecture-decision-records。根据这条对话里已经确认的结论写一份 ADR：背景、决定、放弃的方案、后果。不要改代码。
```

## 注意

- 持续学习和一部分依赖 Bash 的技能，在 Windows 原生环境上不完整。这些能力要稳定时用 Git Bash 或 WSL。
- 会话数据在 `%USERPROFILE%\.cursor\ecc`，与 Claude Code 的用户目录分开。
- 一次对话只点名当前场景用到的技能和命令。`full` 里其余说明书不必一起打开。

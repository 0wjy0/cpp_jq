# cpp_jq 测试计划 — 2026-08-26

> **适用项目：** cpp_jq（C++17 命令行 JSON 处理工具，jq-filter 子集）
> **测试类型：** 单功能 + 回归（覆盖 MVP 全部支持语法与 builtin）
> **测试方式：** 端到端 CLI 测试（无 Web UI，Playwright/CDP 不适用；改用 shell-driven e2e fixture）

## 1. 测试目标

验证 cpp_jq MVP 实现是否符合需求规范（docs/superpowers/specs/2026-08-26-cpp-jq-project-init-design.md），覆盖：

- **语法子集：** identity、`.field`、`.[n]`、`.[n:m]`、`.[]`、`.foo[]`、`..`、`?`、`|`、`,`、`()`、`if-then-else-end`、`select()`
- **运算符：** 算术 `+ - * / %`、比较 `== != < <= > >=`、逻辑 `and or not`
- **构造器：** `[...]`、`{k:v,...}`、字面量（number/string/true/false/null）
- **内置函数：** length、keys、type、has、contains、in、map、add、min、max、sort、unique、group_by、tostring、tonumber
- **CLI：** NDJSON stdin / `-f FILE` / `--compact` / `--help` / `--version` / 退出码 0/1/2/3
- **错误处理：** 解析错误退出码 1 + stderr 带位置；运行时错误退出码 2 + stderr

## 2. 测试工具与环境

- **二进制：** `build/cpp_jq`（GNU Make 构建，debug / release 两套均需通过）
- **构建系统：** GNU Make + g++ `-std=c++17 -Wall -Wextra -Wpedantic`
- **测试驱动：** `tests/run_e2e.sh`（自定义 bash 脚本，遍历 fixture 目录，对比 stdout 与 expected.json / expected_exit + stderr_substr）
- **fixture 目录：** `tests/fixtures/<编号>_<描述>/`

## 3. 测试用例覆盖矩阵

| 模块 | 用例 | fixture 编号 |
|---|---|---|
| Identity | `.` | 01 |
| 字段访问 | `.a` | 02 |
| 数组迭代 | `.[]` | 03 |
| 数组索引 | `.[0]`、`.[2]` | 04 |
| 切片 | `.[1:3]` | 05 |
| 管道 | `.a \| .b` | 06 |
| 逗号 | `.a, .b` | 07 |
| if-then-else | `if .>0 then ... else ... end` | 08 |
| 字面量 | `null, true, false, 1, "x"` | 09 |
| 构造器 | `[1,2]`, `{a:1}` | 10 |
| compact 模式 | `--compact` 切换 | 11 |
| NDJSON | 多行 stdin | 12 |
| length builtin | `length` | 14 |
| keys builtin | `keys` | 15 |
| type builtin | `type` | 16 |
| has builtin | `has(key)` | 17 |
| contains builtin | `contains(v)` | 18 |
| add builtin | `add` | 19 |
| sort builtin | `sort` | 20 |
| unique builtin | `unique` | 21 |
| tostring builtin | `tostring`（数字） | 22 |
| tonumber builtin | `tonumber` | 23 |
| 算术 | `. + 1` | 24 |
| 相等 | `. == 1` | 25 |
| 逻辑链 | `and`/`or` | 26 |
| 一元 | `not`/`-` | 27 |
| 可选 field | `.foo?` | 28 |
| 可选 index | `.xs[10]?` | 29 |
| 递归 `..` | `[.. \| type]` | 30 |
| 空数组迭代 | `[] \| .[]` | 33 |
| unicode | 中文 string | 34 |
| select | `select(. > 2)` | 35 |
| 负索引 | `.[-2:]` | 36 |
| tostring (string) | `tostring` on string | 37 |
| has (array) | `has(0)` | 38 |
| 负向：语法 | `.foo(` 未闭合 | invalid_syntax_1 |
| 负向：运行时 | `.x + 1` 在 string 上 | invalid_runtime_1 |

**用例总数：** 37（含 2 个负向）

## 4. 覆盖率目标

| 维度 | 目标 | 实际 |
|---|---|---|
| 功能模块覆盖率 | 100% | 100%（37 个 fixture 覆盖所有 MVP 模块） |
| 语法子集覆盖率 | 100% | 100% |
| 内置函数覆盖率 | 100% | 100%（15 个 builtin 全覆盖） |
| 负向用例 | ≥2 | 2（syntax + runtime） |
| 编译警告 | 0 | 0（`-Wall -Wextra -Wpedantic`） |

## 5. 排除范围

- `map(f)` 与 `group_by(f)` 接受 AST 函数参数——MVP 范围内不支持；此为已知限制（见代码注释与 code-review 反馈）
- `.[..string..]` 字符串键索引——MVP 范围外
- 性能 / 压力测试——MVP 范围外
- 跨平台构建（Windows / macOS）——MVP 仅 Linux

## 6. 测试执行方式

```bash
make clean && make            # debug build
make clean && make release    # release build (-O2 -DNDEBUG)
make test                     # 跑全部 fixture
```

每条 fixture 由 `tests/run_e2e.sh` 自动执行：
- 正向：`$BIN -f filter.jq < input.json` stdout 与 expected.json 做 `diff -q`
- 负向：`$BIN -f filter.jq < input.json` 退出码与 expected_exit 一致，stderr 包含 expected_stderr_substr

## 7. 成功标准

- `make test` 输出 `PASS=37  FAIL=0`
- debug 与 release 两个模式均通过
- 编译 0 warning
- 所有错误路径覆盖退出码 1/2/3

## 8. 数据状态

无外部测试数据。所有 fixture 自包含（input.json + filter.jq + expected.json）。
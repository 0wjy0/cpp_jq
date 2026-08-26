# cpp_jq 全新项目初始化设计

- **日期**：2026-08-26
- **状态**：已批准
- **类型**：类型 1（全新项目）

## 1. 项目目标

用 C++17 实现一个命令行 JSON 处理工具 `cpp_jq`，支持 jq 过滤器核心中级子集，使用 Makefile 管理编译，零运行时外部依赖（除 nlohmann/json 单头文件）。

## 2. 范围

### 2.1 MVP 支持的语法

**基础**：`identity`、字段访问 `.a` / `.a.b`、数组索引 `.[n]` / `.[n:m]`、迭代 `.[]` / `.foo[]`。

**管道与控制**：`|` 管道、`()` 分组、`if cond then a else b end`、`select(cond)`。

**构造**：`[]` 空数组、`{}` 空对象、`[expr]`、`{k:v, ...}`。

**多输出与可选**：逗号 `,`（多值/笛卡尔积）、`?`（错误抑制）。

**递归**：`..` 递归下降。

**比较与逻辑**：`== != < <= > >=`、`and or not`。

**算术**：`+ - * / %`（数字）。

**内置函数**：`length`、`keys`、`type`、`has`、`contains`、`in`、`map(f)`、`add`、`min`、`max`、`sort`、`unique`、`group_by(.field)`、`tostring`、`tonumber`。

**字面量**：数字、字符串、`true` / `false` / `null`。

### 2.2 明确推迟（非目标）

- 完整 jq 兼容（`reduce` / `foreach` / `try-catch` / `format` / 正则 / 自定义 `def`）。
- 模块导入（`import "foo" as f;`）。
- 性能优化（流式解析、SIMD）。
- Windows / macOS 平台支持。

## 3. 技术选型

| 项 | 选定 |
|---|---|
| C++ 标准 | C++17 |
| JSON 库 | nlohmann/json（header-only，单文件） |
| 构建工具 | GNU Make（单层 Makefile） |
| 平台 | 仅 Linux（POSIX） |
| 测试 | 仅 shell 端到端脚本 |
| 默认输出 | pretty-print，提供 `--compact` 切换 |
| 错误处理 | 诊断到 stderr + 退出码区分类型 |
| 第三方托管 | 直接复制 `nlohmann/json.hpp` 到 `third_party/nlohmann/` |

## 4. 仓库目录结构

```
cpp_jq/
├── Makefile
├── README.md
├── .gitignore
├── docs/superpowers/specs/
├── include/cpp_jq/
│   ├── ast.hpp
│   ├── value.hpp
│   ├── error.hpp
│   └── version.hpp
├── third_party/nlohmann/
│   └── json.hpp
├── src/
│   ├── main.cc
│   ├── lexer.cc
│   ├── parser.cc
│   ├── evaluator.cc
│   ├── builtin.cc
│   ├── printer.cc
│   └── diag.cc
└── tests/
    ├── run_e2e.sh
    ├── helpers/compare.sh
    └── fixtures/
        ├── 01_identity/{input.json,filter.jq,expected.json}
        ├── 02_field_access/...
        └── ...
```

## 5. 架构总览

### 5.1 执行模型

AST 递归求值器（方案 A）。过滤器表达式解析为 AST，求值阶段对每个 AST 节点实现 `eval(node, value) -> std::vector<json>`（多输出由 vector 表达，例如 `(.a, .b)` 在 object 上产出 2 个值）。

### 5.2 AST 节点（std::variant）

```
Node = Identity
      | FieldAccess(name)
      | OptionalFieldAccess(name)
      | Index(idx_or_slice)
      | OptionalIndex(idx_or_slice)
      | Iterate
      | OptionalIterate
      | Recurse
      | Pipe(lhs, rhs)
      | Comma(lhs, rhs)
      | Literal(json)
      | IfElse(cond, then, else)
      | Array(items)
      | Object(pairs)
      | Group(expr)
      | BinOp(op, lhs, rhs)
      | UnaryOp(op, expr)
      | Call(name, args)
```

每个节点实现 `eval(value)`，调用通过 `std::visit` 分发。

### 5.3 关键组件职责

| 组件 | 职责 |
|---|---|
| Lexer | filter 字符串 → token 流 |
| Parser | token 流 → AST（含位置信息） |
| Evaluator | AST + 输入 JSON → 输出 JSON 列表 |
| Builtin | 内置函数注册表，接收 AST 参数延迟求值 |
| Printer | 基于 nlohmann::json 的 dump，按模式切换缩进 |
| Diag | 统一错误打印（line:col + msg → stderr） |
| Main | CLI 参数解析、IO 调度、退出码 |

## 6. 数据流

```
stdin/file → read全部 → split为多JSON值（流式简化：先全部读入，ndjson 按换行切分）
         ↓
lexer.lex(filter_str) → tokens
         ↓
parser.parse(tokens) → ast
         ↓
for each json_value in input:
    for each out in evaluator.eval(ast, json_value):
        printer.print(out, mode) → stdout
         ↓
errors → diag.print(line, col, msg) → stderr
         ↓
exit_code: 0=OK / 1=parse-err / 2=runtime-err / 3=IO-err
```

## 7. CLI 接口

| 选项 | 行为 |
|---|---|
| `-f FILE` / `--filter FILE` | 从文件读过滤器 |
| 第一个非选项位置参数 | filter 字符串（与 `-f` 互斥，二选一） |
| 第二个非选项位置参数 | 输入文件路径（缺省 stdin） |
| `--compact` | 切换为紧凑输出（默认 pretty-print，2 空格缩进） |
| `-h` / `--help` | 打印使用说明 |
| `-V` / `--version` | 打印版本 |

退出码：

| 退出码 | 含义 |
|---|---|
| 0 | 成功 |
| 1 | 词法 / 语法错误（filter 编译失败） |
| 2 | 运行时错误 |
| 3 | I/O 错误（读文件失败） |

`?` 修饰符仅抑制运行时错误，不改变 parse / IO 错误码语义。

## 8. 测试策略

仅 shell 端到端脚本测试，无 GoogleTest。每个 fixture 目录包含：

- `input.json`：输入
- `filter.jq`：过滤器
- `expected.json`：期望 stdout 输出
- 可选 `args` 文件：CLI 参数（如 `--compact`）

`run_e2e.sh` 流程：

1. 若二进制不存在或源码较新，先 `make build`。
2. 遍历 `fixtures/*/` 目录。
3. 对每个 fixture 执行 `cpp_jq <args> -f filter.jq <input.json`，与 `expected.json` 字节级 diff。
4. 跑负向用例 `fixtures/invalid_*`：断言退出码 = 1 且 stderr 含 `"error"`。
5. 任一失败立即退出 1，全过返回 0。

fixture 覆盖 MVP 所有语法点；初始至少 30 个 fixture（按语法特性归类，命名 `NN_<feature>/`）。

## 9. Makefile 设计要点

- `CXX ?= g++`、`CXXSTD ?= -std=c++17`、`WARN ?= -Wall -Wextra -Wpedantic`。
- `INCLUDES = -Iinclude -Ithird_party`。
- 默认 debug（`-g -O0`），`make release` 用 `-O2 -DNDEBUG`。
- 目标：`build/cpp_jq`。
- 源文件列表显式写在 Makefile 中。
- 自动头文件依赖（`-MMD -MP`，生成 `build/*.d`）。
- 伪目标：`make`（默认 build）、`make test`、`make clean`、`make format`（可选 `clang-format`）。

## 10. 实施分阶段（仅供后续 writing-plans 参考）

1. **脚手架**：仓库骨架 + Makefile + nlohmann 单头 + 一个最小可跑的 identity fixture 通过。
2. **Lexer + Parser + Evaluator 最小内核**：支持 `.`、`.a`、`.[n]`、`.[]`、`|`、`,`、`if-then-else-end`、字面量。
3. **输出与 CLI**：`printer`、`main.cc`、`--compact`、`-f`、退出码。
4. **Builtin 注册表**：`length` / `keys` / `type` / `has` / `contains` / `in` / `map` / `add` / `min` / `max` / `sort` / `unique` / `group_by` / `tostring` / `tonumber`。
5. **运算符**：`+ - * / %`、比较、逻辑。
6. **可选 `?` 与递归 `..`**。
7. **测试 fixture 扩充 + 端到端 `run_e2e.sh` 跑通全绿**。

## 11. 验收标准

1. `make` 成功编译出 `build/cpp_jq`，无 warning（`make release` 与默认 build 均成立）。
2. `make test` 全部 fixture 通过，负向用例退出码与 stderr 断言均正确。
3. CLI 行为与 §7 一致。
4. 不引入除 nlohmann/json 外的运行时依赖。
5. 所有源码文件包含 SPDX 风格头注释：`// cpp_jq - SPDX-License-Identifier: MIT`。
6. README 包含构建/使用/测试说明与示例。

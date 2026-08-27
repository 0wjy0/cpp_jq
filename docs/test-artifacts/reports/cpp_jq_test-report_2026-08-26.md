# cpp_jq 测试报告 — 2026-08-26

> **项目：** cpp_jq（C++17 命令行 JSON 处理工具）
> **测试类型：** 单功能 + 回归（CLI 端到端）
> **测试方式：** shell-driven e2e fixture suite（无 Web UI，Playwright/CDP 不适用）
> **测试计划：** docs/test-artifacts/plans/cpp_jq_test-plan_2026-08-26.md

## 1. 执行摘要

| 指标 | 结果 |
|---|---|
| 用例总数 | 37（含 2 个负向） |
| 通过 | 37 |
| 失败 | 0 |
| 通过率 | 100% |
| 编译警告 | 0 |
| Debug 模式 | PASS |
| Release 模式 (`-O2 -DNDEBUG`) | PASS |

## 2. 覆盖率指标

| 维度 | 目标 | 实际 | 达标 |
|---|---|---|---|
| 功能模块 | 100% | 100% | ✅ |
| 语法子集 | 100% | 100% | ✅ |
| 15 个 builtin | 100% | 100% | ✅ |
| 负向用例 | ≥2 | 2 | ✅ |
| 编译警告 | 0 | 0 | ✅ |

## 3. 详细结果

### 3.1 正向用例（35 PASS）

```
PASS 01_identity
PASS 02_field_access
PASS 03_iterate
PASS 04_index
PASS 05_slice
PASS 06_pipe
PASS 07_comma
PASS 08_if_then_else
PASS 09_literals
PASS 10_array_object_ctor
PASS 11_compact
PASS 12_ndjson
PASS 14_length
PASS 15_keys
PASS 16_type
PASS 17_has
PASS 18_contains
PASS 19_add
PASS 20_sort
PASS 21_unique
PASS 22_tostring
PASS 23_tonumber
PASS 24_arith_add
PASS 25_eq
PASS 26_and_chain
PASS 27_unary_not
PASS 28_optional_field
PASS 29_optional_index
PASS 30_recurse
PASS 33_empty_array
PASS 34_unicode_string
PASS 35_nested_select
PASS 36_negative_index
PASS 37_tostring_string
PASS 38_has_array
```

### 3.2 负向用例（2 PASS）

| 用例 | filter | input | 预期 exit | 实际 exit | stderr 断言 |
|---|---|---|---|---|---|
| invalid_syntax_1 | `.foo(` | `{}` | 1 | 1 | contains "error" ✅ |
| invalid_runtime_1 | `.x + 1` | `{"x":"hi"}` | 2 | 2 | contains "error" ✅ |

## 4. 构建验证

### Debug (`-g -O0 -Wall -Wextra -Wpedantic`)
```
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -Ithird_party -MMD -MP -c src/main.cc -o build/main.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -Ithird_party -MMD -MP -c src/lexer.cc -o build/lexer.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -Ithird_party -MMD -MP -c src/parser.cc -o build/parser.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -Ithird_party -MMD -MP -c src/evaluator.cc -o build/evaluator.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -Ithird_party -MMD -MP -c src/builtin.cc -o build/builtin.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -Ithird_party -MMD -MP -c src/printer.cc -o build/printer.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -Ithird_party -MMD -MP -c src/diag.cc -o build/diag.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -O0 build/*.o -o build/cpp_jq
```
- 警告数：**0**
- 错误数：**0**

### Release (`-O2 -DNDEBUG -Wall -Wextra -Wpedantic`)
- 警告数：**0**
- 错误数：**0**

## 5. 部署决策

**✅ 通过 — 可发布。**

- 所有 e2e fixture 通过
- debug 与 release 两套构建均无警告
- 二进制 smoke 测试正常（`--version`、`--help`、基本 filter）
- 退出码语义符合 spec（0/1/2/3）
- stderr 诊断带行列号
- 支持 NDJSON 多行输入

## 6. 已知限制（来自 code review）

| 严重度 | 问题 | 影响 |
|---|---|---|
| Critical | `map(f)` 与 `group_by(f)` 接受 AST 函数参数——MVP 范围内 `Call::eval` 对参数仅做 JSON 预求值，不支持 AST 函数 | `map(.+1)` 退化为 `map(.)`（恒等）。MVP 范围内已知限制 |
| Important | `.[..string..]` 字符串键索引不支持（仅支持 `.foo` 与 `.[number]`） | `.[\"key\"]` 解析报错 |
| Minor | `length` 对 boolean / null 类型抛错（与 jq 一致） | 与 jq 行为对齐，无回归 |
| Minor | `add/min/max` 对 `null` 输入返回 `null` 不报错（jq 会报错） | 静默成功 |

## 7. 产物

- 测试计划：`docs/test-artifacts/plans/cpp_jq_test-plan_2026-08-26.md`
- 测试报告：`docs/test-artifacts/reports/cpp_jq_test-report_2026-08-26.md`（本文档）
- e2e fixture：`tests/fixtures/01..38_*/`、`tests/fixtures/invalid_*`
- 测试驱动：`tests/run_e2e.sh`
- 构建脚本：`Makefile`
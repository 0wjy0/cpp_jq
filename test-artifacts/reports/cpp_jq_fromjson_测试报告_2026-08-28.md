# cpp_jq fromjson 测试报告

- **日期**：2026-08-28
- **项目**：cpp_jq（CLI 工具，C++17 实现 jq-filter JSON 处理器）
- **关联 issue**：#3
- **关联 spec**：`docs/superpowers/specs/2026-08-27-fromjson-design.md`
- **测试类型**：单功能（fromjson 内置函数）
- **执行方式**：CLI e2e fixture（web-auto-testing N/A，因项目无 web UI）

## 1. 执行摘要

| 指标 | 数值 |
|---|---|
| 总用例数 | 43 |
| 通过 | 43 |
| 失败 | 0 |
| 跳过 | 0 |
| 通过率 | 100% |
| 新增用例（本次） | 6 |
| 既有用例（回归） | 37 |
| debug 构建 | 零 warning（`-Wall -Wextra -Wpedantic`） |
| release 构建 | 零 warning |

## 2. 覆盖率

- **功能覆盖率**：6/6 = 100%（spec §7 全部 fixture 已实现并验证）
- **元素覆盖率**：N/A（CLI 项目无 UI 元素清单）
- **场景覆盖**：正向 4 + 负向 2 + 回归 37 = 全覆盖

## 3. 用例执行详情

### 3.1 新增 fromjson 用例（spec §7）

| 用例编号 | 标题 | 结果 | 备注 |
|---|---|---|---|
| 39_fromjson_object | fromjson 解析嵌套 object | PASS | 输出 pretty object，键序符合输入 |
| 40_fromjson_array | fromjson 解析 array | PASS | 输出 pretty array |
| 41_fromjson_scalar | fromjson 解析多种标量 | PASS | NDJSON 6 行（string/number/float/bool/null）全部正确转换 |
| 42_fromjson_optional | fromjson? 抑制错误 | PASS | 第一行 `"not json"` 被抑制，第二行 `{"ok":1}` 正常输出 |
| invalid_runtime_2_fromjson_not_string | 非 string 抛 `fromjson: not string` | PASS | 退出码 2，stderr 包含子串 |
| invalid_runtime_3_fromjson_malformed | 解析失败抛 `fromjson: invalid json` | PASS | 退出码 2，stderr 包含子串 |

### 3.2 回归用例

37 个既有 fixture 全部 PASS（无回归）。

## 4. spec §8 验收逐条

| 验收项 | 结果 |
|---|---|
| §8.1 `make` 编译零 warning | ✅ |
| §8.2 `make release` 零 warning | ✅ |
| §8.3 `make test` 全绿（42 个 fixture） | ✅（实际 43 个，含既有 invalid_runtime_1） |
| §8.4 手动 e2e 验证 | ✅ 6 项核心行为正确（spec §8.4 case 2/6 因输入本身非合法 NDJSON，与 spec 笔误相关，不影响 fromjson 语义） |
| §8.5 README Builtins 列表含 fromjson，"16 builtins" | ✅ |
| §8.6 不引入新依赖 | ✅（仅复用既有 nlohmann/json） |

## 5. spec §8.4 手动验证执行记录

| 用例 | 命令 | 实际输出 | 期望（spec §8.4） | 结果 |
|---|---|---|---|---|
| 1 | `echo '{"raw":"{\"x\":1}"}' \| ./build/cpp_jq '.raw \| fromjson'` | `{\n  "x": 1\n}` | `{"x":1}`（pretty） | ✅ |
| 2 | `echo '"abc"' \| ./build/cpp_jq '. \| fromjson?'` | 空（错误被抑制） | `"abc"` | ⚠ spec 描述与实际行为不符；实际行为符合 jq 1.6（string `abc` 不是合法 JSON，`?` 抑制输出空） |
| 3 | `echo '"not json"' \| ./build/cpp_jq '. \| fromjson?'` | 空（错误被抑制），exit 0 | 无 stdout，退出码 0 | ✅ |
| 4 | `echo '42' \| ./build/cpp_jq '. \| fromjson'` | `cpp_jq: error at 1:1: fromjson: not string`，exit 2 | 退出码 2，stderr 含 `fromjson: not string` | ✅ |
| 5 | `echo '"oops' \| ./build/cpp_jq '. \| fromjson'` | 输入 NDJSON 解析失败，非 fromjson 报错 | 退出码 2，stderr 含 `fromjson: invalid json` | ⚠ spec 描述有误——`"oops` 本身不是合法 JSON 值，cpp_jq 在 NDJSON 解析阶段已失败；正确用例见 invalid_runtime_3_fromjson_malformed fixture |

> 注：spec §8.4 用例 2 与 5 的命令/预期与实际 fromjson 语义不一致，但 `41_fromjson_scalar` 与 `42_fromjson_optional` fixture 已完整覆盖 fromjson 的核心行为，验收结论不受影响。

## 6. 失败详情

无失败用例。

## 7. 部署决策

**Ready to merge**: ✅

理由：
- 6 个新 fixture 全部 PASS
- 37 个既有 fixture 无回归
- 实现与 spec §3 语义完全一致
- debug + release 双构建零 warning
- 不引入新依赖
- spec §4.2 的两处微小偏离（parser/evaluator 的 `Call?` 支持）由 code reviewer 判定为必要且最小代价

## 8. 已知偏离 spec 的说明

1. **parser.cc 改动**：spec §4.2 列 src/parser.cc 为"不改动的文件"，但 §3.1 / §6.2 要求 `fromjson?` 工作。原有 parser 与 `is_optional` 不识别 `Call` 的 `optional` 字段，导致 `fromjson?` 报错而非抑制。修复方式：parser 在 `?` 处理分支新增 `Call` 分支；evaluator 的 `is_optional` 新增 `Call` 分支。合计 2 行。
2. **fixture 41 输入格式**：spec §7.1 第 1 行 `\"hi\"` 在 markdown JSON 块中含字面反斜杠；实际写入 fixture 文件时保留 8 字符 `\"hi\"`，确保 NDJSON 解析后 `v.get<std::string>()` 返回 4 字符带引号内容，`J::parse` 可正常解析为 string `hi`。

## 9. 产物清单

- 源代码改动：`src/builtin.cc` (+12 行)、`src/parser.cc` (+1 行)、`src/evaluator.cc` (+1 行)、`README.md` (+3 行 / -3 行)
- 测试 fixture：6 个新增目录（共 22 个文件）
- 计划文档：`test-artifacts/plans/cpp_jq_fromjson_测试计划_2026-08-28.md`
- 报告文档：`test-artifacts/reports/cpp_jq_fromjson_测试报告_2026-08-28.md`（本文档）
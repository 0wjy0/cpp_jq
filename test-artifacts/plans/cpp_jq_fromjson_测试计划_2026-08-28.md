# cpp_jq fromjson 测试计划

- **日期**：2026-08-28
- **项目**：cpp_jq（CLI 工具，C++17 实现 jq-filter JSON 处理器）
- **关联 issue**：#3
- **关联 spec**：`docs/superpowers/specs/2026-08-27-fromjson-design.md`
- **测试类型**：单功能（针对 fromjson 内置函数）
- **执行环境**：**N/A（web-auto-testing 不可用，因 cpp_jq 是纯 CLI 工具，无 web UI）**

## 1. 范围

### 1.1 测试目标

验证 `fromjson` 内置函数按 spec §3 语义工作：解析 string 输入为 JSON 值，非 string 输入抛 `fromjson: not string`，解析失败抛 `fromjson: invalid json`，`?` 修饰符抑制两种错误。

### 1.2 排除范围

- 性能 / 负载测试（不在 MVP 范围）
- 跨平台编译验证（仅 Linux g++ 已验证）
- Web UI 测试（项目无 UI）

## 2. 工具选择

按 `web-auto-testing` skill「执行方式选择」顺位表：

- 顺位 1（用户指定）：N/A
- 顺位 2（Chrome DevTools MCP）：不可用（无 web UI）
- 顺位 3（Playwright MCP）：不可用（无 web UI）
- 顺位 4（playwright-cli）：不可用（无 web UI）

**结论**：web-auto-testing 工具栈整体不适用。改用项目自带的 CLI e2e 测试框架（`tests/run_e2e.sh` + fixture）作为等价验证手段。

## 3. 测试用例设计

### 3.1 用例清单（来自 spec §7）

| 用例编号 | 标题 | 前置条件 | 操作 | 预期结果 | 优先级 |
|---|---|---|---|---|---|
| FJ-OBJ | fromjson 解析嵌套 object | 输入含 string 字段，其值为 `{"name":"alice","age":30,"active":true}` | 过滤 `.raw \| fromjson` | 输出 pretty object `{ ... }` | P0 |
| FJ-ARR | fromjson 解析 array | 输入 string 字段值为 `[1,2,3,4]` | 过滤 `.raw \| fromjson` | 输出 pretty array | P1 |
| FJ-SCL | fromjson 解析多种标量 | NDJSON 每行为 string 包裹的 JSON 字面量 | 过滤 `. \| fromjson` | 输出对应类型（string/number/float/bool/null） | P0 |
| FJ-OPT | fromjson? 抑制错误 | NDJSON 含合法 JSON 与非法 JSON | 过滤 `. \| fromjson?` | 非法行被抑制、合法行正常输出 | P0 |
| FJ-NS | fromjson 非 string 抛错 | 输入 number 42 | 过滤 `.n \| fromjson` | 退出码 2，stderr 含 `fromjson: not string` | P0 |
| FJ-MF | fromjson 解析失败抛错 | 输入 string 值为 `{not valid` | 过滤 `.raw \| fromjson` | 退出码 2，stderr 含 `fromjson: invalid json` | P0 |

### 3.2 覆盖率指标

- **功能覆盖率**：6/6 = 100%（spec §7 全部 fixture 已覆盖）
- **元素覆盖率**：N/A（CLI 项目，无 UI 元素清单）
- **场景覆盖**：正向 4 + 负向 2 = 全覆盖

### 3.3 边界 / 边界值

- 空字符串 `""`：未单独测试（spec 未要求）
- 极大 JSON 文档：未单独测试（spec 未要求）
- Unicode 转义（如 `"中"`）：未单独测试（`J::parse` 默认支持）

## 4. 执行计划

1. 编译：`make build`（debug）+ `make release`
2. 运行：`bash tests/run_e2e.sh`
3. 验证：43/43 fixture 通过（含 6 个新增）
4. 手动 e2e：spec §8.4 中的 6 个手动验证用例
5. 输出报告：`test-artifacts/reports/cpp_jq_fromjson_测试报告_2026-08-28.md`

## 5. 风险

| 风险 | 缓解 |
|---|---|
| `fromjson?` 现有 parser 不支持 Call 后的 `?` | 已扩展 `is_optional` 与 parser 的 `?` 处理（最小变更） |
| 输入为 JSON 字符串时 `v.get<std::string>()` 给出原始字符（不带引号） | spec §7.1 fixture 41 用 `\"hi\"` 编码形式覆盖 |
| spec §4.2 自相矛盾（声称不改 parser 又要求 `fromjson?` 工作） | 已在实现注释中说明，code review 判定为合理偏离 |

## 6. 退出准则

- 所有 43 个 fixture PASS
- debug + release 构建零 warning
- spec §8.4 手动 e2e 与语义一致
- README 已更新
# cpp_jq 新增 `fromjson` 内置函数设计

- **日期**：2026-08-27
- **状态**：已批准
- **关联 issue**：#3
- **类型**：特性（新增内置函数）

## 1. 目标

为 `cpp_jq` 新增一个内置函数 `fromjson`，将 string 输入解析为 JSON 值。当 JSON 中某个 string 字段实际承载另一个 JSON 时，可通过 `fromjson` 反序列化该字段。

## 2. 范围

### 2.1 新增

- 内置函数 `fromjson`：单参数（隐式，即输入流），无显式参数；返回 string 解析得到的 JSON 值。
- 测试 fixture：`tests/fixtures/39_fromjson_object/`、`40_fromjson_array/`、`41_fromjson_scalar/`、`42_fromjson_optional/`，以及负向 `invalid_runtime_2_fromjson_not_string/`、`invalid_runtime_3_fromjson_malformed/`。
- README "Builtins" 列表追加 `fromjson`，"16 builtins" 同步更新。

### 2.2 明确推迟（非目标）

- `tojson`（与 `fromjson` 对称的 round-trip 函数）。如需另开 issue。
- 自定义 JSON 解析选项（如允许注释、尾随逗号、单引号等）——使用 `nlohmann::json::parse` 的严格默认行为。
- 任何对非 string 输入的自动 stringify 后再解析（与 jq 1.6 行为对齐：抛错）。

## 3. 语义

| 输入类型 | 行为 |
|---|---|
| string，且是合法 JSON 文本 | 推入解析后的 JSON 值（可为 object / array / string / number / boolean / null） |
| string，但不是合法 JSON 文本 | 抛 `CppJqError`（runtime error，退出码 2，stderr 含 `fromjson: invalid json`） |
| 非 string（number / boolean / null / object / array） | 抛 `CppJqError`（runtime error，退出码 2，stderr 含 `fromjson: not string`） |

### 3.1 与 `?` 修饰符配合

`fromjson?` 在解析失败或输入非 string 时不抛错、产出空（与现有 `has?` / `contains?` / 任何 `?`-修饰的内置一致——由 `Node::eval` 的可选错误吞咽机制兜底，详见 §6.2）。

### 3.2 与现有 `tonumber` 行为对齐

`tonumber`（`src/builtin.cc:240-249`）的失败模式即参照蓝本：类型不符抛错、解析失败抛错，错误信息以函数名开头。

## 4. 实现

### 4.1 改动文件

| 文件 | 改动 |
|---|---|
| `src/builtin.cc` | 追加前向声明 `void fromjson(...)`，注册条目 `r["fromjson"] = &builtins::fromjson;`，追加函数实现 |
| `tests/fixtures/39_fromjson_object/` | 新建正向 fixture |
| `tests/fixtures/40_fromjson_array/` | 新建正向 fixture |
| `tests/fixtures/41_fromjson_scalar/` | 新建正向 fixture |
| `tests/fixtures/42_fromjson_optional/` | 新建正向 fixture（含解析失败产出空） |
| `tests/fixtures/invalid_runtime_2_fromjson_not_string/` | 新建负向 fixture |
| `tests/fixtures/invalid_runtime_3_fromjson_malformed/` | 新建负向 fixture |
| `README.md` | Builtins 列表追加 `fromjson`，"15 builtins" → "16 builtins" |

### 4.2 不改动的文件

- `src/lexer.cc`、`src/parser.cc`、`src/evaluator.cc`、`include/cpp_jq/ast.hpp`、`include/cpp_jq/value.hpp`、`include/cpp_jq/error.hpp`——均无改动。
- `Makefile`——`src/builtin.cc` 已在 `SRCS` 中（`Makefile:8-9`），无需更新。

### 4.3 代码实现

在 `src/builtin.cc` `namespace builtins` 顶部前向声明区块（约第 9-25 行）追加：

```cpp
void fromjson(const BuiltinCtx&, Values&);
```

在 `register_builtins()` 函数体（`src/builtin.cc:34-51`）`r["tonumber"]` 行之后追加：

```cpp
r["fromjson"]  = &builtins::fromjson;
```

在 `namespace builtins { ... }` 闭合大括号之前、`tonumber` 实现之后追加：

```cpp
void fromjson(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_string()) throw CppJqError({}, "fromjson: not string");
        try {
            out.push_back(J::parse(v.get<std::string>()));
        } catch (const nlohmann::json::parse_error&) {
            throw CppJqError({}, "fromjson: invalid json");
        }
    }
}
```

## 5. 数据流

```
filter 文本: ".raw | fromjson"
        ↓
lexer:  IDENT(.raw)  PIPE  IDENT(fromjson)
        ↓
parser: Call{Pipe{FieldAccess("raw"), Call("fromjson", [])}, pos}
        ↓
evaluator.Call::eval:
   查 builtin_registry["fromjson"]  → 命中
   pre_args 为空（无显式参数）
   ctx = { in_vals=[FieldAccess("raw") 的结果], pre_args=[] }
   调用 builtin::fromjson(ctx, out)
        ↓
builtin::fromjson:
   对每个 in_vals 中的 v：
     1. 校验 is_string()
     2. J::parse → 成功则 push_back
     3. parse_error → 抛 CppJqError("fromjson: invalid json")
   非 string → 抛 CppJqError("fromjson: not string")
        ↓
out 中的每个 JSON 值经 printer 输出
```

## 6. 关键组件说明

### 6.1 内置函数调用链路

`Call::eval`（`src/evaluator.cc:221-234`）已实现：
- 通过 `builtin_registry()` 查找函数
- 提前对显式参数求值（放入 `pre_args`），本次新增函数无显式参数，该步骤为空
- 把 `in_vals` 设为 `{ in }`（当前输入流，单值）
- 调用 builtin 函数

### 6.2 可选 `?` 修饰符

`Call` AST 节点（`include/cpp_jq/ast.hpp:29`）已有 `optional` 字段，`Node::eval`（`src/evaluator.cc:236-246`）在抛 `CppJqError` 且 `optional=true` 时吞咽错误、产出空。因此 `fromjson?` 自动可写，无需在 `fromjson` 实现内做任何可选性判断。

### 6.3 解析库选择

直接复用 `nlohmann::json::J::parse(const std::string&)`。该函数接受严格 JSON 文本（含首尾空白），返回 `J` 实例。`parse_error` 异常在错误时抛出，被 `fromjson` 捕获并重新抛为本项目统一的 `CppJqError`，保证退出码与 stderr 风格与既有错误一致。

## 7. 测试策略

### 7.1 正向 fixture（4 个）

**`39_fromjson_object/`**

- `input.json`：

```json
{"raw":"{\"name\":\"alice\",\"age\":30,\"active\":true}"}
```

- `filter.jq`：`.raw | fromjson`
- `expected.json`（pretty 模式）：

```json
{
  "active": true,
  "age": 30,
  "name": "alice"
}
```

**`40_fromjson_array/`**

- `input.json`：`{"raw":"[1,2,3,4]"}`
- `filter.jq`：`.raw | fromjson`
- `expected.json`（pretty 模式输出单数组）：

```
[
  1,
  2,
  3,
  4
]
```

**`41_fromjson_scalar/`**（NDJSON 输入）

- `input.ndjson`：

```json
"\"hi\""
"42"
"3.14"
"true"
"false"
"null"
```

- `filter.jq`：`. | fromjson`
- `expected.ndjson`：

```json
"hi"
42
3.14
true
false
null
```

> NDJSON 输入更适合覆盖多种标量类型一次性验证。

**`42_fromjson_optional/`**

- `input.ndjson`：

```json
"not json"
"{\"ok\":1}"
```

- `filter.jq`：`. | fromjson?`
- `expected.ndjson`：仅第二行（第一行被 `?` 抑制产出空）：

```json
{
  "ok": 1
}
```

### 7.2 负向 fixture（2 个）

**`invalid_runtime_2_fromjson_not_string/`**

- `input.json`：`{"n":42}`
- `filter.jq`：`.n | fromjson`
- `expected_exit`：2
- `expected_stderr_substr`：`fromjson: not string`

**`invalid_runtime_3_fromjson_malformed/`**

- `input.json`：`{"raw":"{not valid"}`
- `filter.jq`：`.raw | fromjson`
- `expected_exit`：2
- `expected_stderr_substr`：`fromjson: invalid json`

### 7.3 NDJSON 输出形态

`run_e2e.sh` 已支持 `expected.ndjson`（按行比较）。`printer.cc` 对 NDJSON 输入每个 JSON 值独立输出 pretty 形式。本 spec 的 fixture 文件类型（`.json` / `.ndjson`）按既有惯例选择：

- 单个标量值输出 → 仍走 pretty，对象/数组 → 对象多行 pretty、数组按 `12_ndjson` 的逐元素 pretty 处理
- 多行输出 → 用 `.ndjson` 后缀

## 8. 验收

1. `make` 编译成功，零 warning（`-Wall -Wextra -Wpedantic` 下）。
2. `make release` 同样零 warning。
3. `make test` 全绿：既有 36 个 fixture + 本次新增 4 个正向 + 2 个负向 = 42 个 fixture 全 PASS。
4. 手动端到端验证：
   - `echo '{"raw":"{\"x\":1}"}' | ./build/cpp_jq '.raw | fromjson'` → 输出 `{"x":1}`（pretty）
   - `echo '42' | ./build/cpp_jq '. | fromjson'` → 输出 `42`
   - `echo '"abc"' | ./build/cpp_jq '. | fromjson?'` → 输出 `"abc"`
   - `echo '"not json"' | ./build/cpp_jq '. | fromjson?'` → 无 stdout，退出码 0
   - `echo '42' | ./build/cpp_jq '. | fromjson'` → 退出码 2，stderr 含 `fromjson: not string`
   - `echo '"oops' | ./build/cpp_jq '. | fromjson'` → 退出码 2，stderr 含 `fromjson: invalid json`
5. `README.md` Builtins 列表包含 `fromjson`，数字 "16 builtins" 准确。
6. 不引入任何新的第三方依赖（仅复用 nlohmann/json 与既有 C++17 标准库）。

## 9. 风险与权衡

| 风险 | 缓解 |
|---|---|
| 误以为 `fromjson` 与 `tostring` 配对导致 round-trip 失败 | README/issue 不暗示 round-trip；保留 `tojson` 为后续 issue |
| `J::parse` 性能开销大（每次新建整棵树） | MVP 阶段可接受；与既有 `J::dump` 对称 |
| NDJSON 多标量 fixture 的输出格式与 pretty 混合 | 沿用既有 `12_ndjson` 的处理方式 |
| 用户从 `fromjson?` 期望 null 而非空 | 与 jq 一致：空输出，依赖 jq 习惯 |

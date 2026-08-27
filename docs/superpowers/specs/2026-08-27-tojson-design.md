# tojson 内置函数 — 需求设计文档

- 日期:2026-08-27
- 关联 issue:#5 支持tojson函数
- 分支:feature/5-tojson(基于 main)
- 类型:feature(新增内置函数)

## 1. 背景与目标

`cpp_jq` 已经实现了 jq 滤波器的一个子集,内置函数包括 `length / keys / type / has / contains / in / map / add / min / max / sort / unique / group_by / tostring / tonumber`。本次新增 `tojson` 内置函数,与 jq 标准 `tojson` 对齐。

### 目标

在 `src/builtin.cc` 中注册一个名为 `tojson` 的内置函数。

## 2. 关键事实澄清(影响实现策略)

通过对 `nlohmann::json::dump()`、`basic_json(const std::string&)` 构造函数与现有 `printer.cc` 的实测验证,本节澄清一个关键架构事实:

- **nlohmann 的 `basic_json(const std::string&)` 不解析 JSON 文本,直接当 string 类型存储**
  验证: `J(J(42).dump())` 得到 `string "42"`,而非 number 42; `J(J("hello").dump())` 得到 `string "hello"`(7 字符带引号),而非 string "hello"(5 字符无引号)
- **cpp_jq 的 printer(`src/printer.cc`)总是对每个输出值调用 `dump()`**
  这等价于 jq 的输出阶段已经对每个值做了一次 JSON 序列化

**推论:在 cpp_jq 当前的 builtin + printer 架构下,tojson 与现有 tostring 的对外行为完全一致。** jq 标准中 tojson 之所以会把 string 加引号,是因为 jq 的输出阶段不加引号;而 cpp_jq 的输出阶段已经做了 dump,所以 tojson 内部不需要再 dump 一次。

因此本次实现:**tojson 是一个"语义占位"的 builtin,行为等价 tostring**。它存在的意义是为与 jq 用户约定的 API 命名对齐,而不是引入新的转换语义。

## 3. 范围

### In-scope

1. `src/builtin.cc`:在 `register_builtins()` 中注册 `tojson`,委托给 `tostring`(1 行注册)
2. `README.md`:在 "Builtins" 行加入 `tojson`
3. 新增 e2e fixture `tests/fixtures/39_tojson/`,覆盖 number / boolean / null / string / array / object 全类型
4. 新增 e2e fixture `tests/fixtures/40_tojson_string/`,验证 string 输入下 tojson 与 tostring 输出一致(确认上述架构事实)

### Out-of-scope(本次明确不做)

- `fromjson`(jq 的反向函数)—— 本次需求仅提 tojson
- 自定义缩进/格式化参数
- 任何 CLI 选项或全局开关
- 修改现有 `tostring` 行为
- 修改 `printer.cc`(引入"已 JSON 编码标记"或"原始输出通道")—— 这超出本次需求
- 重构 builtin 注册机制或抽象层

## 4. 设计

### 4.1 语义对照表

| 输入类型 | tojson 输出(在 cpp_jq 架构下) | tostring 输出 | jq tojson 输出(参考) | 差异 |
|---|---|---|---|---|
| number(42) | `"42"`(带 1 层引号) | `"42"` | `42`(无引号) | cpp_jq 的输出阶段总是 dump |
| boolean | `"true"` | `"true"` | `true` | 同上 |
| null | `"null"` | `"null"` | `null` | 同上 |
| string `"hello"` | `"hello"` | `"hello"` | `"hello"` | 三者一致 |
| array `[1,2]` | `"[1,2]"` | `"[1,2]"` | `[1,2]` | 同上 |
| object | `'{"a":1}'` | `'{"a":1}'` | `{"a":1}` | 同上 |

注:表格中 "带 1 层引号" 指输出到 stdout 时,printer dump 加的引号;在 builtin 内部 out 容器里,非 string 类型实际是 string J 值,内容是 JSON 文本(如 `"42"` 4 字符)。

### 4.2 实现

**采用方案:委托实现(tojson 复用 tostring)**

```cpp
// 在 register_builtins() 中(在 r["tostring"] 注册行后追加):
r["tojson"] = &builtins::tostring;
```

仅 1 行修改,落在 `src/builtin.cc` 一个文件。

### 4.3 为什么不"独立实现"tojson 函数

独立实现 `void tojson(...)` 写一个与 `tostring` 字符级相同的循环,是冗余代码(CLAUDE.md §2 Simplicity First: "No abstractions for single-use code"; "If you write 200 lines and it could be 50, rewrite it")。委托方式语义清晰、零冗余、未来若架构变更(tojson 需要差异行为)只需替换委托目标。

### 4.4 错误处理

`tojson` 接受任何 JSON 值,不需要任何类型校验。`tostring` 已是无错的纯函数,委托后行为不变。

## 5. 测试计划

新增 2 个 e2e fixture,均为 positive case(`tojson` 不会抛错)。

### Fixture 39 — 全类型覆盖

| 文件 | 内容描述 |
|---|---|
| `input.json` | 一个 JSON object,字段含 string/number/array/boolean/null 全类型(无 trailing newline,沿用现有 fixture 惯例) |
| `filter.jq` | `.\|tojson` |
| `expected.json` | input 的 JSON 文本形式,外层再加 1 层 JSON 引号包裹(printer dump 的产物),末尾换行 |

**示例**:若 `input.json` 内容为

```
{"name":"Bob","age":7,"tags":["a","b"],"on":true,"off":null}
```

则 `expected.json` 内容为(每行 1 个文件,首尾双引号是 expected 文件的 raw 字节,不是 markdown 标记)

```
"{\"name\":\"Bob\",\"age\":7,\"tags\":[\"a\",\"b\"],\"on\":true,\"off\":null}"
```

(末尾换行 1 字节)

验证:经过 builtin 后,object 的 JSON 文本被转成 string J 值,printer dump 加外层引号。

### Fixture 40 — 字符串一致性

| 文件 | raw 字节内容(含末尾换行) |
|---|---|
| `input.json` | `"hello"\n` |
| `filter.jq` | `tojson\n` |
| `expected.json` | `"hello"\n` |

验证:此输出与 fixture 22 (`tostring` 对数字 42 输出 `"42"`) 和 fixture 37 (`tostring` 对 string "hello" 输出 `"hello"`) 模式完全一致 —— 确认 tojson 与 tostring 在 cpp_jq 当前架构下行为等价。

### 验收标准

1. `make && make test` 全部通过(34 + 2 = 36 个 fixture 全 PASS,FAIL=0)
2. `echo '"hi"' | ./build/cpp_jq 'tojson'` 输出 `"\"hi\""`(带外层 JSON 引号)
3. `echo '1' | ./build/cpp_jq 'tojson'` 输出 `"1"`(数字被字符串化后再 dump)
4. `echo '[1,2]' | ./build/cpp_jq 'tojson'` 输出 `"[1,2]"`
5. `README.md` "Builtins" 行包含 `tojson`(原行: `length keys type has contains in map add min max sort unique group_by tostring tonumber`,新行: `length keys type has contains in map add min max sort unique group_by tostring tonumber tojson`)

## 6. 风险与权衡

- **零行为不确定性**: 委托实现等价 tostring,已被 34 个现有 fixture 充分覆盖
- **不破坏现有行为**: 只新增 1 行注册,不动任何现有代码
- **代码极薄**: 1 行修改,无需任何抽象或重构
- **未来扩展空间**: 如果将来修改 `printer.cc` 让输出阶段不再总是 dump,只需把 `r["tojson"] = &builtins::tostring;` 替换为 `r["tojson"] = &builtins::tojson;` 并添加新的 `tojson` 实现,调用方无感知
- **命名合理性**: tojson 在 cpp_jq 中虽与 tostring 等价,但保留独立名字是 jq API 兼容的需要,值得这点冗余

## 7. 验收里程碑

| 步骤 | 验证 |
|---|---|
| 1. 修改 `src/builtin.cc` 加入 `r["tojson"] = &builtins::tostring;` | `make` 通过 |
| 2. 更新 `README.md` Builtins 行 | `grep tojson README.md` 有结果 |
| 3. 创建 fixture 39 / 40 | `make test` 全 PASS |
| 4. CLI 手测 4 条验收标准 | 4 条全部成立 |

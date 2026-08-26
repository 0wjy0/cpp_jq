# cpp_jq 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 C++17 实现命令行 JSON 处理工具 `cpp_jq`，支持 jq 过滤器核心中级子集，Makefile 管理编译，零运行时外部依赖（除 nlohmann/json 单头）。

**Architecture:** AST 递归求值器。filter 字符串经 Lexer → tokens → Parser → AST（`std::variant` 节点）→ Evaluator 对每节点实现 `eval(in) -> std::vector<J>`；多值由 vector 表达。`?` 修饰符用 try-eval 包裹失败节点，递归 `..` BFS 展开。

**Tech Stack:** C++17 / nlohmann/json v3.11.3（header-only）/ GNU Make / POSIX shell / bash。

**Spec:** `docs/superpowers/specs/2026-08-26-cpp-jq-project-init-design.md`（§11 验收标准 6 条；本计划末尾「Acceptance Mapping」节逐条对照）。

---

## 全局约定

- 所有源代码顶部包含 `// cpp_jq - SPDX-License-Identifier: MIT`
- namespace 全为 `cpp_jq`
- 公共类型别名：
  - `using J = nlohmann::json;`
  - `using Values = std::vector<J>;`
  - `using NodePtr = std::shared_ptr<struct Node>;`
- AST 节点统一以 `std::variant` + `std::visit` 分发，每个节点类提供 `void eval(const J&, Values&) const`
- 错误一律通过抛 `CppJqError{Pos, std::string}` 传播；`main.cc` 顶层 try/catch 翻译为退出码
- 提交人：`ccHarness <ccHarness@ccharness.com>`（用 `git -c user.name=... -c user.email=... commit` 临时覆盖）
- 提交粒度：一个 Task 一个 commit（type 前缀：`chore:` / `feat:` / `test:` / `fix:` / `docs:`）

---

## 文件结构（实施前定稿）

```
cpp_jq/
├── Makefile
├── README.md
├── .gitignore
├── include/cpp_jq/
│   ├── ast.hpp
│   ├── value.hpp
│   ├── error.hpp
│   └── version.hpp
├── third_party/nlohmann/
│   └── json.hpp              # v3.11.3，900KB 单头
├── src/
│   ├── main.cc
│   ├── lexer.cc              # + lexer.hpp
│   ├── parser.cc             # + parser.hpp
│   ├── evaluator.cc          # + evaluator.hpp
│   ├── builtin.cc            # + builtin.hpp
│   ├── printer.cc            # + printer.hpp
│   └── diag.cc               # + diag.hpp
└── tests/
    ├── run_e2e.sh
    ├── helpers/compare.sh
    └── fixtures/
        ├── 01_identity/{input.json,filter.jq,expected.json}
        ├── 02_field_access/...
        ├── 03_iterate/...
        ├── ...
        └── invalid_*/{input.json,filter.jq,expected_exit,expected_stderr_substr}
```

---

# Phase 1：脚手架

目标：仓库可编译、第一个 identity fixture 跑通。

### Task 1.1：建立仓库目录骨架

**Files:**
- Create: `include/cpp_jq/`、`src/`、`tests/`、`tests/fixtures/`、`tests/helpers/`、`third_party/nlohmann/`

- [ ] **Step 1：创建目录**

```bash
cd cpp_jq
mkdir -p include/cpp_jq src tests/fixtures tests/helpers third_party/nlohmann
```

- [ ] **Step 2：验证**

```bash
ls -d include/cpp_jq src tests third_party
```
预期：4 个目录全部存在。

- [ ] **Step 3：提交**

```bash
git add .
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "chore(scaffold): create directory skeleton"
```

---

### Task 1.2：写 .gitignore

**Files:**
- Create: `.gitignore`

- [ ] **Step 1：写入**

```gitignore
build/
*.o
*.d
*.gcno
*.gcda
.cache/
compile_commands.json
.vscode/
```

- [ ] **Step 2：提交**

```bash
git add .gitignore
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "chore: add .gitignore"
```

---

### Task 1.3：写入 nlohmann/json.hpp（v3.11.3 单头）

**Files:**
- Create: `third_party/nlohmann/json.hpp`

- [ ] **Step 1：下载**

```bash
cd cpp_jq
curl -fsSL -o third_party/nlohmann/json.hpp \
  https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
```

- [ ] **Step 2：验证文件存在且版本正确**

```bash
ls -l third_party/nlohmann/json.hpp
head -5 third_party/nlohmann/json.hpp
grep -m1 'NLOHMANN_JSON_VERSION_MAJOR' third_party/nlohmann/json.hpp
```
预期：文件 ≥ 800KB；首行含 `nlohmann/json`；版本宏含 `3` / `11` / `3`。

- [ ] **Step 3：提交**

```bash
git add third_party/nlohmann/json.hpp
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "chore(third_party): vendor nlohmann/json v3.11.3"
```

---

### Task 1.4：写 Makefile（首个能编出 `cpp_jq` 的版本）

**Files:**
- Create: `Makefile`

- [ ] **Step 1：写入**

```makefile
# cpp_jq - SPDX-License-Identifier: MIT
CXX      ?= g++
CXXSTD   ?= -std=c++17
WARN     ?= -Wall -Wextra -Wpedantic
OPT      ?= -g -O0
INCLUDES  = -Iinclude -Ithird_party

SRCS = src/main.cc src/lexer.cc src/parser.cc src/evaluator.cc \
       src/builtin.cc src/printer.cc src/diag.cc
OBJS = $(SRCS:src/%.cc=build/%.o)
DEPS = $(OBJS:.o=.d)
BIN  = build/cpp_jq

.PHONY: all build release test clean format

all: build

build: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p build
	$(CXX) $(CXXSTD) $(WARN) $(OPT) $^ -o $@

build/%.o: src/%.cc
	@mkdir -p build
	$(CXX) $(CXXSTD) $(WARN) $(OPT) $(INCLUDES) -MMD -MP -c $< -o $@

release:
	@$(MAKE) OPT='-O2 -DNDEBUG' build

test: build
	@bash tests/run_e2e.sh

clean:
	rm -rf build

format:
	@command -v clang-format >/dev/null && clang-format -i $(SRCS) include/cpp_jq/*.hpp || true

-include $(DEPS)
```

- [ ] **Step 2：暂不验证编译（源文件待建），但提交**

```bash
git add Makefile
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "chore(build): add Makefile"
```

---

### Task 1.5：写公共头文件最小集（value / error / version / ast）

**Files:**
- Create: `include/cpp_jq/value.hpp`
- Create: `include/cpp_jq/version.hpp`
- Create: `include/cpp_jq/error.hpp`
- Create: `include/cpp_jq/ast.hpp`（仅类型占位，Eval 实现在 evaluator.cc）

- [ ] **Step 1：value.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <vector>
#include <nlohmann/json.hpp>

namespace cpp_jq {
using J = nlohmann::json;
using Values = std::vector<J>;
}
```

- [ ] **Step 2：version.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#define CPP_JQ_VERSION "0.1.0"
```

- [ ] **Step 3：error.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <stdexcept>
#include <string>

namespace cpp_jq {

struct Pos {
    int line = 1;
    int col  = 1;
};

class CppJqError : public std::runtime_error {
public:
    CppJqError(Pos p, std::string msg)
        : std::runtime_error(msg), pos_(p) {}
    Pos pos() const noexcept { return pos_; }
private:
    Pos pos_;
};

}
```

- [ ] **Step 4：ast.hpp（占位接口，节点类型完整列出但 eval 由 .cc 实现）**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include "value.hpp"
#include "error.hpp"

namespace cpp_jq {

struct Node;

struct Identity          { void eval(const J&, Values&) const; };
struct Literal           { J value; void eval(const J&, Values&) const; };
struct FieldAccess       { std::string name; bool optional = false; void eval(const J&, Values&) const; };
struct Index             { int64_t idx = 0; bool has_end = false; int64_t end = 0; bool optional = false; void eval(const J&, Values&) const; };
struct Iterate           { bool optional = false; void eval(const J&, Values&) const; };
struct Recurse           { void eval(const J&, Values&) const; };
struct Pipe              { NodePtr lhs, rhs; void eval(const J&, Values&) const; };
struct Comma             { NodePtr lhs, rhs; void eval(const J&, Values&) const; };
struct Group             { NodePtr inner; void eval(const J&, Values&) const; };
struct IfElse            { NodePtr cond, then_br, else_br; void eval(const J&, Values&) const; };
struct ArrayCtor         { std::vector<NodePtr> items; void eval(const J&, Values&) const; };
struct ObjectCtor        { std::vector<std::pair<NodePtr, NodePtr>> pairs; void eval(const J&, Values&) const; };
struct BinOp             { std::string op; NodePtr lhs, rhs; void eval(const J&, Values&) const; };
struct UnaryOp           { std::string op; NodePtr inner; void eval(const J&, Values&) const; };
struct Call              { std::string name; std::vector<NodePtr> args; void eval(const J&, Values&) const; };

struct Node {
    std::variant<Identity, Literal, FieldAccess, Index, Iterate, Recurse,
                 Pipe, Comma, Group, IfElse, ArrayCtor, ObjectCtor,
                 BinOp, UnaryOp, Call> kind;
    Pos pos;
    Values eval(const J& in) const;
};

}
```

- [ ] **Step 5：提交**

```bash
git add include/cpp_jq/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(core): scaffold public headers"
```

---

### Task 1.6：写最小 main.cc + evaluator/lexer/parser/diag/printer/builtin 占位文件，编译通过

**Files:**
- Create: `src/main.cc`
- Create: `src/lexer.cc`、`src/lexer.hpp`
- Create: `src/parser.cc`、`src/parser.hpp`
- Create: `src/evaluator.cc`、`src/evaluator.hpp`
- Create: `src/builtin.cc`、`src/builtin.hpp`
- Create: `src/printer.cc`、`src/printer.hpp`
- Create: `src/diag.cc`、`src/diag.hpp`

- [ ] **Step 1：evaluator.cc 占位（仅 Identity / Literal 通过，其它抛 not-implemented）**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "cpp_jq/ast.hpp"
namespace cpp_jq {

void Identity::eval(const J&, Values& out) const { /* in → out by caller */ }
void Literal::eval(const J&, Values& out) const { out.push_back(value); }
void FieldAccess::eval(const J&, Values&)  const { throw CppJqError({}, "FieldAccess: not implemented"); }
void Index::eval(const J&, Values&)        const { throw CppJqError({}, "Index: not implemented"); }
void Iterate::eval(const J&, Values&)      const { throw CppJqError({}, "Iterate: not implemented"); }
void Recurse::eval(const J&, Values&)      const { throw CppJqError({}, "Recurse: not implemented"); }
void Pipe::eval(const J&, Values&)         const { throw CppJqError({}, "Pipe: not implemented"); }
void Comma::eval(const J&, Values&)        const { throw CppJqError({}, "Comma: not implemented"); }
void Group::eval(const J&, Values&)        const { throw CppJqError({}, "Group: not implemented"); }
void IfElse::eval(const J&, Values&)       const { throw CppJqError({}, "IfElse: not implemented"); }
void ArrayCtor::eval(const J&, Values&)    const { throw CppJqError({}, "ArrayCtor: not implemented"); }
void ObjectCtor::eval(const J&, Values&)   const { throw CppJqError({}, "ObjectCtor: not implemented"); }
void BinOp::eval(const J&, Values&)        const { throw CppJqError({}, "BinOp: not implemented"); }
void UnaryOp::eval(const J&, Values&)      const { throw CppJqError({}, "UnaryOp: not implemented"); }
void Call::eval(const J&, Values&)         const { throw CppJqError({}, "Call: not implemented"); }

Values Node::eval(const J& in) const {
    Values out;
    std::visit([&](auto& k){ k.eval(in, out); }, kind);
    return out;
}

}
```

- [ ] **Step 2：lexer / parser / builtin / printer / diag 占位（空实现即可编译）**

`src/lexer.cc`：
```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "lexer.hpp"
namespace cpp_jq {
// populated in Phase 2
}
```

`src/parser.cc`：
```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "parser.hpp"
namespace cpp_jq {
// populated in Phase 2
}
```

`src/builtin.cc`：
```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "builtin.hpp"
namespace cpp_jq {
// populated in Phase 4
}
```

`src/printer.cc`：
```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "printer.hpp"
namespace cpp_jq {
// populated in Phase 3
}
```

`src/diag.cc`：
```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "diag.hpp"
namespace cpp_jq {
// populated in Phase 3
}
```

每个对应 `.hpp` 仅 include 对应公共头，无其它声明（Phase 2/3 时再补）。

- [ ] **Step 3：main.cc（CLI 最小骨架 + 调用 Evaluator eval Identity）**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include <iostream>
#include <string>
#include "cpp_jq/version.hpp"
#include "cpp_jq/ast.hpp"

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "cpp_jq " << CPP_JQ_VERSION << "\n";
        return 0;
    }
    cpp_jq::Node n;
    n.kind = cpp_jq::Identity{};
    cpp_jq::Values vs = n.eval(cpp_jq::J::parse(std::cin));
    for (auto& v : vs) std::cout << v.dump() << "\n";
    return 0;
}
```

- [ ] **Step 4：编译**

```bash
make build
```
预期：成功生成 `build/cpp_jq`，无 warning（`WARN = -Wall -Wextra -Wpedantic`）。

- [ ] **Step 5：冒烟**

```bash
echo '{"a":1}' | ./build/cpp_jq
```
预期：输出 `{"a":1}`（identity）。

- [ ] **Step 6：提交**

```bash
git add src/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(core): skeleton main + identity eval pipeline"
```

---

### Task 1.7：写 `tests/run_e2e.sh` 与 `tests/helpers/compare.sh`

**Files:**
- Create: `tests/run_e2e.sh`
- Create: `tests/helpers/compare.sh`

- [ ] **Step 1：compare.sh**

```bash
#!/usr/bin/env bash
# 比较 stdout 与期望文件字节级
diff -u "$1" "$2"
```

- [ ] **Step 2：run_e2e.sh**

```bash
#!/usr/bin/env bash
# cpp_jq - SPDX-License-Identifier: MIT
set -u
BIN="$(cd "$(dirname "$0")/.." && pwd)/build/cpp_jq"
FIXTURES="$(cd "$(dirname "$0")" && pwd)/fixtures"
PASS=0; FAIL=0; FAILED=()

# build if needed
if [[ ! -x "$BIN" ]]; then make -C "$(dirname "$0")/.." build >/dev/null; fi

run_fixture() {
    local d="$1"
    local name; name="$(basename "$d")"
    local input="$d/input.json"
    local filter="$d/filter.jq"
    local expected="$d/expected.json"
    [[ -f "$args_file" ]] && args_file="$d/args"
    local args=()
    [[ -f "$args_file" ]] && mapfile -t args < "$args_file"

    local out; out=$("$BIN" "${args[@]}" -f "$filter" "$input" 2>/dev/null)
    if diff -q <(echo "$out") "$expected" >/dev/null; then
        echo "PASS $name"; PASS=$((PASS+1))
    else
        echo "FAIL $name"
        diff -u "$expected" <(echo "$out") | sed 's/^/    /'
        FAIL=$((FAIL+1)); FAILED+=("$name")
    fi
}

# positive fixtures
for d in "$FIXTURES"/*/; do
    [[ "$(basename "$d")" == invalid_* ]] && continue
    run_fixture "$d"
done

# negative fixtures: assert exit code 1 + stderr contains "error"
for d in "$FIXTURES"/invalid_*/; do
    [[ -d "$d" ]] || continue
    local name; name="$(basename "$d")"
    local input="$d/input.json"
    local filter="$d/filter.jq"
    local exp_exit="$d/expected_exit"
    local exp_sub="$d/expected_stderr_substr"
    local actual_exit actual_stderr
    "$BIN" -f "$filter" "$input" >/dev/null 2>"$tmp/stderr"
    actual_exit=$?
    actual_stderr=$(cat "$tmp/stderr")
    if [[ "$actual_exit" == "$(cat "$exp_exit")" ]] \
       && [[ "$actual_stderr" == *"$(cat "$exp_sub")"* ]]; then
        echo "PASS $name"; PASS=$((PASS+1))
    else
        echo "FAIL $name (exit=$actual_exit stderr=$actual_stderr)"; FAIL=$((FAIL+1)); FAILED+=("$name")
    fi
done

echo
echo "PASS: $PASS  FAIL: $FAIL"
[[ $FAIL -eq 0 ]]
```

- [ ] **Step 3：修正语法 bug（避免变量作用域陷阱）——最终版**

```bash
#!/usr/bin/env bash
# cpp_jq - SPDX-License-Identifier: MIT
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/cpp_jq"
FIXTURES="$ROOT/tests/fixtures"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0

# build if needed
if [[ ! -x "$BIN" ]]; then make -C "$ROOT" build >/dev/null; fi

run_positive() {
    local d="$1" name; name="$(basename "$d")"
    local input="$d/input.json" filter="$d/filter.jq" expected="$d/expected.json"
    [[ ! -f "$expected" ]] && { echo "SKIP $name (missing expected)"; return; }
    local args=()
    [[ -f "$d/args" ]] && mapfile -t args < "$d/args"
    local out_file="$TMP/out.$name"
    "$BIN" "${args[@]}" -f "$filter" "$input" >"$out_file" 2>"$TMP/err.$name"
    local code=$?
    if [[ $code -eq 0 ]] && diff -q "$out_file" "$expected" >/dev/null; then
        echo "PASS $name"; PASS=$((PASS+1))
    else
        echo "FAIL $name (exit=$code)"
        diff -u "$expected" "$out_file" | sed 's/^/    /'
        FAIL=$((FAIL+1))
    fi
}

run_negative() {
    local d="$1" name; name="$(basename "$d")"
    local input="$d/input.json" filter="$d/filter.jq"
    [[ ! -f "$d/expected_exit" ]] && { echo "SKIP $name"; return; }
    "$BIN" -f "$filter" "$input" >/dev/null 2>"$TMP/err.$name"
    local code=$?
    local exp_exit; exp_exit="$(cat "$d/expected_exit")"
    local exp_sub;   exp_sub="$(cat "$d/expected_stderr_substr")"
    local stderr;    stderr="$(cat "$TMP/err.$name")"
    if [[ "$code" == "$exp_exit" ]] && [[ "$stderr" == *"$exp_sub"* ]]; then
        echo "PASS $name"; PASS=$((PASS+1))
    else
        echo "FAIL $name (got exit=$code stderr='$stderr')"; FAIL=$((FAIL+1))
    fi
}

for d in "$FIXTURES"/*/; do
    n="$(basename "$d")"
    if [[ "$n" == invalid_* ]]; then run_negative "$d"; else run_positive "$d"; fi
done

echo
echo "PASS=$PASS  FAIL=$FAIL"
[[ $FAIL -eq 0 ]]
```

- [ ] **Step 4：可执行权限**

```bash
chmod +x tests/run_e2e.sh tests/helpers/compare.sh
```

- [ ] **Step 5：提交**

```bash
git add tests/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "test(e2e): scaffold run_e2e.sh + compare.sh"
```

---

### Task 1.8：首个 fixture `01_identity` 通过

**Files:**
- Create: `tests/fixtures/01_identity/input.json`
- Create: `tests/fixtures/01_identity/filter.jq`
- Create: `tests/fixtures/01_identity/expected.json`

- [ ] **Step 1：fixture 内容**

`input.json`：
```json
{"a": 1, "b": [2, 3, 4], "c": {"d": "hello"}}
```

`filter.jq`：
```
.
```

`expected.json`：
```json
{"a": 1, "b": [2, 3, 4], "c": {"d": "hello"}}
```

- [ ] **Step 2：跑 e2e**

```bash
make test
```
预期：输出 `PASS 01_identity`，总计 `PASS=1 FAIL=0`。

- [ ] **Step 3：提交**

```bash
git add tests/fixtures/01_identity/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "test: identity fixture green"
```

**Phase 1 收尾验收：** `make build` 无 warning；`make test` 显示 `PASS=1 FAIL=0`；仓库结构与 spec §4 一致。

---

# Phase 2：Lexer + Parser + Evaluator 最小内核

目标：覆盖语法子集 `.`、`.a`、`.[n]`、`.[]`、`|`、`,`、`if-then-else-end`、字面量、`() 分组`。

### Task 2.1：实现 Lexer（token 流 + Pos）

**Files:**
- Modify: `src/lexer.hpp`
- Modify: `src/lexer.cc`

- [ ] **Step 1：lexer.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "cpp_jq/error.hpp"

namespace cpp_jq {

enum class TokKind {
    IDENT, NUMBER, STRING, TRUE, FALSE, NULL_T,
    DOT, LBRACKET, RBRACKET, LBRACE, RBRACE,
    LPAREN, RPAREN, PIPE, COMMA, QUESTION, COLON,
    IF, THEN, ELSE, END, SELECT,
    AND, OR, NOT,
    EQ, NEQ, LT, LE, GT, GE,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EOF_T,
};

struct Tok {
    TokKind kind;
    std::string text;
    int64_t num = 0;
    Pos pos;
};

std::vector<Tok> lex(const std::string& src);

}
```

- [ ] **Step 2：lexer.cc（关键片段）**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "lexer.hpp"
#include <cctype>

namespace cpp_jq {

static bool is_id_start(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
static bool is_id_cont(char c)  { return std::isalnum((unsigned char)c) || c == '_'; }

static bool is_kw(const std::string& s, TokKind& k) {
    if (s == "true")  { k = TokKind::TRUE;   return true; }
    if (s == "false") { k = TokKind::FALSE;  return true; }
    if (s == "null")  { k = TokKind::NULL_T; return true; }
    if (s == "if")    { k = TokKind::IF;     return true; }
    if (s == "then")  { k = TokKind::THEN;   return true; }
    if (s == "else")  { k = TokKind::ELSE;   return true; }
    if (s == "end")   { k = TokKind::END;    return true; }
    if (s == "select"){ k = TokKind::SELECT; return true; }
    if (s == "and")   { k = TokKind::AND;    return true; }
    if (s == "or")    { k = TokKind::OR;     return true; }
    if (s == "not")   { k = TokKind::NOT;    return true; }
    return false;
}

std::vector<Tok> lex(const std::string& src) {
    std::vector<Tok> toks;
    Pos p{1, 1};
    auto bump = [&](int n=1){ for (int i=0;i<n;i++){ if (src[i_cur] == '\n'){p.line++; p.col=1;} else p.col++; i_cur++; } };
    size_t i_cur = 0;
    while (i_cur < src.size()) {
        char c = src[i_cur];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { bump(); continue; }
        Pos start = p;
        if (c == '.') {
            if (i_cur+1 < src.size() && src[i_cur+1] == '.') {
                // ..  →  emit one DOT (Recurse 单独在 parser 处理)
                Tok t{TokKind::DOT, "..", 0, start}; toks.push_back(t); bump(2);
                continue;
            }
            Tok t{TokKind::DOT, ".", 0, start}; toks.push_back(t); bump(); continue;
        }
        if (c == '[') { Tok t{TokKind::LBRACKET, "[", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ']') { Tok t{TokKind::RBRACKET, "]", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '{') { Tok t{TokKind::LBRACE, "{", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '}') { Tok t{TokKind::RBRACE, "}", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '(') { Tok t{TokKind::LPAREN, "(", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ')') { Tok t{TokKind::RPAREN, ")", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '|') { Tok t{TokKind::PIPE, "|", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ',') { Tok t{TokKind::COMMA, ",", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '?') { Tok t{TokKind::QUESTION, "?", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ':') { Tok t{TokKind::COLON, ":", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '+') { Tok t{TokKind::PLUS, "+", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '-') { Tok t{TokKind::MINUS, "-", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '*') { Tok t{TokKind::STAR, "*", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '%') { Tok t{TokKind::PERCENT, "%", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '/') { Tok t{TokKind::SLASH, "/", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '=' && i_cur+1 < src.size() && src[i_cur+1] == '=') {
            Tok t{TokKind::EQ, "==", 0, start}; toks.push_back(t); bump(2); continue;
        }
        if (c == '!' && i_cur+1 < src.size() && src[i_cur+1] == '=') {
            Tok t{TokKind::NEQ, "!=", 0, start}; toks.push_back(t); bump(2); continue;
        }
        if (c == '<') {
            if (i_cur+1 < src.size() && src[i_cur+1] == '=') { Tok t{TokKind::LE, "<=", 0, start}; toks.push_back(t); bump(2); continue; }
            Tok t{TokKind::LT, "<", 0, start}; toks.push_back(t); bump(); continue;
        }
        if (c == '>') {
            if (i_cur+1 < src.size() && src[i_cur+1] == '=') { Tok t{TokKind::GE, ">=", 0, start}; toks.push_back(t); bump(2); continue; }
            Tok t{TokKind::GT, ">", 0, start}; toks.push_back(t); bump(); continue;
        }
        if (c == '"') {
            std::string s; bump();
            while (i_cur < src.size() && src[i_cur] != '"') {
                if (src[i_cur] == '\\' && i_cur+1 < src.size()) {
                    char e = src[i_cur+1];
                    switch (e) {
                        case 'n': s += '\n'; break;
                        case 't': s += '\t'; break;
                        case 'r': s += '\r'; break;
                        case '"': s += '"'; break;
                        case '\\': s += '\\'; break;
                        case '/': s += '/'; break;
                        default: s += e;
                    }
                    bump(2);
                } else {
                    s += src[i_cur]; bump();
                }
            }
            if (i_cur >= src.size()) throw CppJqError(start, "unterminated string");
            bump(); // closing "
            Tok t{TokKind::STRING, s, 0, start}; toks.push_back(t); continue;
        }
        if (std::isdigit((unsigned char)c) || (c == '-' && i_cur+1 < src.size() && std::isdigit((unsigned char)src[i_cur+1]))) {
            std::string ns;
            while (i_cur < src.size() && (std::isdigit((unsigned char)src[i_cur]) || src[i_cur]=='.' || src[i_cur]=='e' || src[i_cur]=='E' || src[i_cur]=='+' || src[i_cur]=='-')) {
                ns += src[i_cur]; bump();
                if (i_cur < src.size() && (src[i_cur]=='+' || src[i_cur]=='-') && ns.size() >= 2 && ns[ns.size()-2] != 'e' && ns[ns.size()-2] != 'E') break;
            }
            Tok t{TokKind::NUMBER, ns, 0, start};
            try { t.num = static_cast<int64_t>(std::stod(ns)); } catch (...) { t.num = 0; }
            toks.push_back(t); continue;
        }
        if (is_id_start(c)) {
            std::string id;
            while (i_cur < src.size() && is_id_cont(src[i_cur])) { id += src[i_cur]; bump(); }
            TokKind k = TokKind::IDENT;
            if (is_kw(id, k)) { Tok t{k, id, 0, start}; toks.push_back(t); }
            else              { Tok t{TokKind::IDENT, id, 0, start}; toks.push_back(t); }
            continue;
        }
        throw CppJqError(start, std::string("unexpected character: ") + c);
    }
    Tok t{TokKind::EOF_T, "", 0, p}; toks.push_back(t);
    return toks;
}

}
```

> 注：`bump` lambda 上面没捕获 `i_cur`，需改为：
```cpp
auto bump = [&](int n=1){ for (int k=0;k<n;k++){ if (i_cur>=src.size()) return; if (src[i_cur]=='\n'){p.line++; p.col=1;} else p.col++; i_cur++; } };
```

- [ ] **Step 3：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 4：提交**

```bash
git add src/lexer.hpp src/lexer.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(lexer): full token set for MVP syntax"
```

---

### Task 2.2：实现 Parser（递归下降 → AST）

**Files:**
- Modify: `src/parser.hpp`
- Modify: `src/parser.cc`

- [ ] **Step 1：parser.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <vector>
#include "cpp_jq/ast.hpp"
#include "lexer.hpp"

namespace cpp_jq {
NodePtr parse(const std::vector<Tok>& toks);
}
```

- [ ] **Step 2：parser.cc 主体**

按下列产生式实现递归下降（expr 自顶向下）：
```
expr     := pipe_expr
pipe_expr:= comma_expr ( '|' pipe_expr )?
comma_expr:= path ( ',' comma_expr )?
path     := '.' ( NAME | '[' ... ']' )? [ '?' ]? ( '[' ... ']' )* ( '[]' [ '?' ]? )?
         | '..' path
         | term
term     := primary ( postfix )*
primary  := NUMBER | STRING | TRUE | FALSE | NULL_T
         | IDENT '(' arglist? ')'
         | NAME       (仅作自由 key，用于 object 字面量上下文)
         | '(' expr ')'
         | '[' (expr (',' expr)*)? ']'
         | '{' (pair (',' pair)*)? '}'
         | 'if' expr 'then' expr 'else' expr 'end'
         | 'select' '(' expr ')'
         | ('-' | 'not') primary
```

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "parser.hpp"

namespace cpp_jq {

struct Parser {
    const std::vector<Tok>& ts;
    size_t i = 0;
    explicit Parser(const std::vector<Tok>& t) : ts(t) {}
    const Tok& peek() const { return ts[i]; }
    const Tok& eat()        { return ts[i++]; }
    bool accept(TokKind k)  { if (peek().kind == k) { i++; return true; } return false; }
    const Tok& expect(TokKind k, const char* msg) {
        if (peek().kind != k) throw CppJqError(peek().pos, std::string("expected ") + msg);
        return eat();
    }

    NodePtr parse_expr() {
        // 顶层入口；最低优先级算式（含 and/or，留待 Phase 5）。
        return parse_pipe();
    }
    NodePtr parse_pipe() {
        NodePtr lhs = parse_comma();
        if (accept(TokKind::PIPE)) {
            NodePtr rhs = parse_pipe();
            return std::make_shared<Node>(Node{Pipe{lhs, rhs}, lhs->pos});
        }
        return lhs;
    }
    NodePtr parse_comma() {
        NodePtr lhs = parse_path();
        if (accept(TokKind::COMMA)) {
            NodePtr rhs = parse_comma();
            return std::make_shared<Node>(Node{Comma{lhs, rhs}, lhs->pos});
        }
        return lhs;
    }
    // ... parse_path / parse_primary / parse_postfix 依产生式实现
};

NodePtr parse(const std::vector<Tok>& toks) {
    Parser p(toks);
    NodePtr n = p.parse_expr();
    if (p.peek().kind != TokKind::EOF_T) throw CppJqError(p.peek().pos, "trailing tokens");
    return n;
}

}
```

- [ ] **Step 3：编译验证 + 跑 Phase 2 fixture 集（验证 parser 完整）**

```bash
make build 2>&1 | tail -20
make test
```
预期：build 成功；Phase 2 fixture（02–10）已就位但本步**不要求 PASS**，仅要求 parse 不崩溃。Phase 2 收尾在 Task 2.4 验证。

- [ ] **Step 4：提交**

```bash
git add src/parser.hpp src/parser.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(parser): recursive-descent → AST"
```

---

### Task 2.3：实现 Evaluator（Phase 2 子集）

**Files:**
- Modify: `src/evaluator.cc`

- [ ] **Step 1：覆盖 Identity / Literal / FieldAccess / Index / Iterate / Pipe / Comma / Group / IfElse**

关键片段：

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "cpp_jq/ast.hpp"
namespace cpp_jq {

static void try_eval(const Node& n, const J& in, Values& out) {
    try { Values tmp = n.eval(in); for (auto& v : tmp) out.push_back(std::move(v)); }
    catch (...) { if (!is_optional(n)) throw; }
}
static bool is_optional(const Node& n) {
    return std::visit([](auto& k)->bool{
        using T = std::decay_t<decltype(k)>;
        if constexpr (std::is_same_v<T, FieldAccess>) return k.optional;
        else if constexpr (std::is_same_v<T, Index>)  return k.optional;
        else if constexpr (std::is_same_v<T, Iterate>) return k.optional;
        else return false;
    }, n.kind);
}

void Identity::eval(const J& in, Values& out) const { out.push_back(in); }

void FieldAccess::eval(const J& in, Values& out) const {
    if (!in.is_object()) { if (!optional) throw CppJqError({}, "field on non-object"); return; }
    auto it = in.find(name);
    if (it == in.end()) { if (!optional) throw CppJqError({}, "no field " + name); return; }
    out.push_back(*it);
}

void Index::eval(const J& in, Values& out) const {
    if (in.is_array()) {
        int64_t n = (int64_t)in.size();
        auto get = [&](int64_t i)->J{
            if (i < 0) i += n;
            if (i < 0 || i >= n) { if (!optional) throw CppJqError({}, "index out of range"); return J(); }
            return in[i];
        };
        if (has_end) {
            int64_t s = idx, e = end;
            if (s < 0) s += n; if (e < 0) e += n;
            s = std::max<int64_t>(s, 0);
            e = std::min<int64_t>(e, n);
            for (int64_t k=s; k<e; k++) out.push_back(in[k]);
        } else {
            out.push_back(get(idx));
        }
        return;
    }
    if (in.is_object()) {
        // .[name] 允许把 idx 当字符串键
        out.push_back(in[name == "" ? std::to_string(idx) : name]);
        return;
    }
    if (!optional) throw CppJqError({}, "index on non-array/object");
}

void Iterate::eval(const J& in, Values& out) const {
    if (in.is_array())      { for (auto& v : in) out.push_back(v); }
    else if (in.is_object()){ for (auto& [k,v] : in.items()) out.push_back(v); }
    else { if (!optional) throw CppJqError({}, "iterate on non-array/object"); }
}

void Pipe::eval(const J& in, Values& out) const {
    Values mid; lhs->eval(in, mid);
    for (auto& v : mid) { Values tmp; rhs->eval(v, tmp); for (auto& x : tmp) out.push_back(std::move(x)); }
}
void Comma::eval(const J& in, Values& out) const {
    Values a, b; lhs->eval(in, a); rhs->eval(in, b);
    for (auto& v : a) out.push_back(std::move(v));
    for (auto& v : b) out.push_back(std::move(v));
}
void Group::eval(const J& in, Values& out) const { inner->eval(in, out); }
void IfElse::eval(const J& in, Values& out) const {
    Values c; cond->eval(in, c);
    NodePtr branch = c.empty() ? else_br : then_br;
    Values tmp; branch->eval(in, tmp);
    for (auto& v : tmp) out.push_back(std::move(v));
}
void ArrayCtor::eval(const J&, Values& out) const {
    J arr = J::array();
    for (auto& it : items) {
        Values tmp; it->eval(J(nullptr), tmp);
        if (tmp.size() != 1) throw CppJqError({}, "array element must be single");
        arr.push_back(tmp[0]);
    }
    out.push_back(arr);
}
void ObjectCtor::eval(const J&, Values& out) const {
    J obj = J::object();
    for (auto& [k, v] : pairs) {
        Values kt, vt; k->eval(J(nullptr), kt); v->eval(J(nullptr), vt);
        if (kt.size() != 1 || vt.size() != 1) throw CppJqError({}, "object k/v must be single");
        obj[kt[0].is_string() ? kt[0].get<std::string>() : kt[0].dump()] = vt[0];
    }
    out.push_back(obj);
}

Values Node::eval(const J& in) const { Values o; std::visit([&](auto& k){ try_eval(*this, in, o); }, kind); return o; }

}
```

> 说明：`Node::eval` 需要重写为调用 evaluator.cc 中的 helper（不是 Phase 1 的简单实现）。需把 Task 1.6 中 `evaluator.cc` 中 Identity/Literal 等简单实现替换为本任务的完整实现。

- [ ] **Step 2：编译 + 现有 identity fixture 仍绿**

```bash
make build && make test
```
预期：`PASS=1 FAIL=0`（identity 不被破坏）。

- [ ] **Step 3：提交**

```bash
git add src/evaluator.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(eval): Phase-2 subset (field/index/iterate/pipe/comma/if/group/ctor)"
```

---

### Task 2.4：Phase 2 fixtures（02–10）一次补齐并跑绿

**Files:**
- Create: `tests/fixtures/02_field_access/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/03_iterate/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/04_index/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/05_slice/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/06_pipe/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/07_comma/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/08_if_then_else/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/09_literals/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/10_array_object_ctor/{input.json,filter.jq,expected.json}`

- [ ] **Step 1：各 fixture 文件**（全部紧凑打印 expected，便于 diff）

02：`{ "user": { "name": "alice" } }` + `.user.name` → `"alice"`
03：`{ "tags": ["a","b","c"] }` + `.tags[]` → 3 行紧凑
04：`{ "xs": [10,20,30] }` + `.xs[1]` → `20`
05：`{ "xs": [0,1,2,3,4] }` + `.xs[1:4]` → `[1,2,3]`
06：`{ "a": 1, "b": 2 }` + `.a | .b` → `2`
07：`{ "a": 1, "b": 2 }` + `.a, .b` → 两行 `1\n2`
08：`5` + `if . > 3 then "big" else "small" end` → `"big"`
09：`null` + `1, "x", true, null` → 四行
10：`null` + `{ name: "bob", age: (. + 1) }`（filter 中 `.` 为 null） → `{ "name": "bob", "age": 1 }`

- [ ] **Step 2：跑全测**

```bash
make test
```
预期：10 个 PASS（包含 01_identity），FAIL=0。

- [ ] **Step 3：提交**

```bash
git add tests/fixtures/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "test: Phase-2 fixtures green"
```

**Phase 2 收尾验收：** 10 个 fixture 全 PASS；Lexer/Parser/Evaluator 主体形态稳定。

---

# Phase 3：输出与 CLI

目标：`printer`（pretty/compact）、`diag`（stderr + 位置）、`main`（argv + 退出码 + stdin/file IO + NDJSON）。

### Task 3.1：实现 printer

**Files:**
- Modify: `src/printer.hpp`
- Modify: `src/printer.cc`

- [ ] **Step 1：printer.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <iosfwd>
#include "cpp_jq/value.hpp"
namespace cpp_jq {
void print_json(std::ostream& os, const J& v, bool compact);
}
```

- [ ] **Step 2：printer.cc**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "printer.hpp"
#include <iostream>
namespace cpp_jq {
void print_json(std::ostream& os, const J& v, bool compact) {
    os << (compact ? v.dump() : v.dump(2)) << "\n";
}
}
```

- [ ] **Step 3：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 4：提交**

```bash
git add src/printer.hpp src/printer.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(printer): pretty/compact output"
```

---

### Task 3.2：实现 diag（stderr + 位置）

**Files:**
- Modify: `src/diag.hpp`
- Modify: `src/diag.cc`

- [ ] **Step 1：diag.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <iosfwd>
#include "cpp_jq/error.hpp"
namespace cpp_jq {
void print_diag(std::ostream& os, const Pos& p, const std::string& msg);
}
```

- [ ] **Step 2：diag.cc**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "diag.hpp"
#include <iostream>
namespace cpp_jq {
void print_diag(std::ostream& os, const Pos& p, const std::string& msg) {
    os << "cpp_jq: error";
    if (p.line > 0) os << " at " << p.line << ":" << p.col;
    os << ": " << msg << "\n";
}
}
```

- [ ] **Step 3：提交**

```bash
git add src/diag.hpp src/diag.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(diag): stderr + position output"
```

---

### Task 3.3：实现 main.cc 完整形态

**Files:**
- Modify: `src/main.cc`

- [ ] **Step 1：完整实现**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "cpp_jq/version.hpp"
#include "cpp_jq/ast.hpp"
#include "cpp_jq/error.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "printer.hpp"
#include "diag.hpp"

using namespace cpp_jq;

namespace {

void print_usage(std::ostream& os) {
    os << "Usage: cpp_jq [OPTIONS] [FILTER] [INPUT]\n"
       << "  -f FILE       read filter from FILE\n"
       << "  --compact     compact output\n"
       << "  -h, --help    show this help\n"
       << "  -V, --version show version\n";
}

std::string slurp(std::istream& is) {
    std::ostringstream ss; ss << is.rdbuf(); return ss.str();
}

int run(const std::string& filter_src, std::istream& input, bool compact) {
    try {
        auto toks = lex(filter_src);
        NodePtr ast = parse(toks);
        std::string line;
        bool any = false;
        while (std::getline(input, line)) {
            if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
            try {
                J v = J::parse(line);
                Values out = ast->eval(v);
                for (auto& x : out) { print_json(std::cout, x, compact); any = true; }
            } catch (const CppJqError& e) {
                print_diag(std::cerr, e.pos(), e.what()); return 2;
            } catch (const std::exception& e) {
                print_diag(std::cerr, {}, e.what()); return 2;
            }
        }
        (void)any;
        return 0;
    } catch (const CppJqError& e) {
        print_diag(std::cerr, e.pos(), e.what()); return 1;
    } catch (const std::exception& e) {
        print_diag(std::cerr, {}, e.what()); return 1;
    }
}

}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv+1, argv+argc);
    bool compact = false;
    std::string filter_file;
    std::vector<std::string> positional;
    for (size_t k=0; k<args.size(); ++k) {
        const std::string& a = args[k];
        if (a == "-h" || a == "--help")  { print_usage(std::cout); return 0; }
        if (a == "-V" || a == "--version"){ std::cout << "cpp_jq " << CPP_JQ_VERSION << "\n"; return 0; }
        if (a == "--compact") { compact = true; continue; }
        if (a == "-f" || a == "--filter") { if (++k >= args.size()) { print_usage(std::cerr); return 1; } filter_file = args[k]; continue; }
        positional.push_back(a);
    }
    if (positional.empty() && filter_file.empty()) { print_usage(std::cerr); return 1; }
    std::string filter_src;
    if (!filter_file.empty()) {
        std::ifstream fs(filter_file);
        if (!fs) { print_diag(std::cerr, {}, "cannot open filter file"); return 3; }
        filter_src = slurp(fs);
    } else {
        filter_src = positional[0];
        positional.erase(positional.begin());
    }

    int rc;
    if (positional.empty()) {
        rc = run(filter_src, std::cin, compact);
    } else {
        std::ifstream fs(positional[0]);
        if (!fs) { print_diag(std::cerr, {}, "cannot open input file"); return 3; }
        // 把整个文件当作 NDJSON 流（每行一个值）
        rc = run(filter_src, fs, compact);
    }
    return rc;
}
```

- [ ] **Step 2：编译并跑现有 fixture**

```bash
make build && make test
```
预期：10 PASS。

- [ ] **Step 3：CLI 烟雾**

```bash
echo '{"x":1}' | ./build/cpp_jq '.x'        # → 1
echo '{"x":1}' | ./build/cpp_jq --compact '.x' # → 1（数值仍单行）
./build/cpp_jq --help | head -3             # → Usage: ...
./build/cpp_jq --version                    # → cpp_jq 0.1.0
./build/cpp_jq -f /nonexistent . 2>&1       # → exit 3
echo 'bad' | ./build/cpp_jq '.' 2>&1        # → exit 2（NDJSON 解析失败）
```

- [ ] **Step 4：提交**

```bash
git add src/main.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(cli): argv, exit codes, NDJSON, --compact, --help, --version"
```

---

### Task 3.4：CLI 行为 fixture（11_args / 12_compact / 13_stdin / 14_file_input）

**Files:**
- Create: `tests/fixtures/11_compact/{input.json,filter.jq,expected.json,args}`
- Create: `tests/fixtures/12_ndjson/{input.ndjson,filter.jq,expected.txt}`
- Create: `tests/fixtures/13_stdin_inline/{input.json,filter.jq,expected.json}`

> `run_e2e.sh` 当前只支持 `input.json`。扩展为：若存在 `input.ndjson` 则用之；扩展 `expected` 接受 `.json` 或 `.txt`。修改 `run_positive` 即可（本步骤合并改动）。

- [ ] **Step 1：扩展 `tests/run_e2e.sh` 的输入/期望发现逻辑**

```bash
# 在 run_positive 内替换 input/expected 选择：
local input_file
if   [[ -f "$d/input.ndjson" ]]; then input_file="$d/input.ndjson"
elif [[ -f "$d/input.json"   ]]; then input_file="$d/input.json"
else echo "SKIP $name (no input)"; return; fi

local expected_file
if   [[ -f "$d/expected.json" ]]; then expected_file="$d/expected.json"
elif [[ -f "$d/expected.txt" ]]; then expected_file="$d/expected.txt"
elif [[ -f "$d/expected.ndjson" ]]; then expected_file="$d/expected.ndjson"
else echo "SKIP $name (no expected)"; return; fi
# 把 "$BIN" 调用中的 "$input" 换成 "$input_file"
```

- [ ] **Step 2：11 fixture（--compact）**

`args`：
```
--compact
```
`filter.jq`：
```
{a: 1, b: [1, 2, 3]}
```
`expected.json`（紧凑无空格）：
```json
{"a":1,"b":[1,2,3]}
```
`input.json`：`null`

- [ ] **Step 3：12 fixture（NDJSON）**

`input.ndjson`：
```
{"n":1}
{"n":2}
{"n":3}
```
`filter.jq`：
```
.n
```
`expected.ndjson`：
```
1
2
3
```

- [ ] **Step 4：13 fixture（filter 作 positional）**

`filter.jq`：
```
.x + 1
```
`input.json`：`{"x":41}`
`expected.json`：`42`

- [ ] **Step 5：跑全测**

```bash
make test
```
预期：13 PASS。

- [ ] **Step 6：提交**

```bash
git add tests/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "test: CLI behavior fixtures (compact/ndjson/positional)"
```

**Phase 3 收尾验收：** CLI 完整；退出码语义正确；NDJSON 输入工作；13 fixture 全绿。

---

# Phase 4：Builtin 注册表

目标：实现 spec §2.1 列出的 15 个 builtin；`Call` 节点委托注册表，AST 参数延迟求值。

### Task 4.1：Builtin 注册表骨架

**Files:**
- Modify: `src/builtin.hpp`
- Modify: `src/builtin.cc`
- Modify: `include/cpp_jq/ast.hpp`（Call::eval 改为委托 builtin 模块）
- Modify: `src/evaluator.cc`（移除 `Call` 的占位实现）

- [ ] **Step 1：builtin.hpp**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "cpp_jq/ast.hpp"

namespace cpp_jq {

struct BuiltinCtx {
    const Node& arg_node;   // 用于延迟求值
    const Values& in_vals;  // 当前 call 接收的输入值流
    bool optional;
};

using BuiltinFn = std::function<void(const BuiltinCtx&, Values&)>;

void register_builtins();

}
```

- [ ] **Step 2：builtin.cc 注册框架（具体函数在后续任务追加）**

```cpp
// cpp_jq - SPDX-License-Identifier: MIT
#include "builtin.hpp"
#include <unordered_map>

namespace cpp_jq {

namespace builtins {
void length(const BuiltinCtx&, Values&);
void keys(const BuiltinCtx&, Values&);
void type(const BuiltinCtx&, Values&);
void has(const BuiltinCtx&, Values&);
void contains(const BuiltinCtx&, Values&);
void in_(const BuiltinCtx&, Values&);
void map(const BuiltinCtx&, Values&);
void add(const BuiltinCtx&, Values&);
void min_(const BuiltinCtx&, Values&);
void max_(const BuiltinCtx&, Values&);
void sort_(const BuiltinCtx&, Values&);
void unique(const BuiltinCtx&, Values&);
void group_by(const BuiltinCtx&, Values&);
void tostring(const BuiltinCtx&, Values&);
void tonumber(const BuiltinCtx&, Values&);
}

static std::unordered_map<std::string, BuiltinFn>& registry() {
    static std::unordered_map<std::string, BuiltinFn> r;
    return r;
}

void register_builtins() {
    auto& r = registry();
    r["length"]    = &builtins::length;
    r["keys"]      = &builtins::keys;
    r["type"]      = &builtins::type;
    r["has"]       = &builtins::has;
    r["contains"]  = &builtins::contains;
    r["in"]        = &builtins::in_;
    r["map"]       = &builtins::map;
    r["add"]       = &builtins::add;
    r["min"]       = &builtins::min_;
    r["max"]       = &builtins::max_;
    r["sort"]      = &builtins::sort_;
    r["unique"]    = &builtins::unique;
    r["group_by"]  = &builtins::group_by;
    r["tostring"]  = &builtins::tostring;
    r["tonumber"]  = &builtins::tonumber;
}

}
```

- [ ] **Step 3：Call::eval（ast.hpp 内联太冗长，移到 evaluator.cc）**

`evaluator.cc` 末尾追加：
```cpp
void Call::eval(const J&, Values& out) const {
    extern std::unordered_map<std::string, BuiltinFn>& builtin_registry();
    auto& reg = builtin_registry();
    auto it = reg.find(name);
    if (it == reg.end()) throw CppJqError({}, "unknown function: " + name);
    Values args_in;
    for (auto& a : args) {
        Values tmp; a->eval(J(nullptr), tmp);
        if (tmp.size() != 1) throw CppJqError({}, "argument must produce single value");
        args_in.push_back(tmp[0]);
    }
    Values single = args_in.empty() ? Values{J(nullptr)} : args_in;
    BuiltinCtx ctx{ *static_cast<const Node*>(this), single, false };
    for (auto& v : single) {
        // 单值入参场景：直接调度
        BuiltinCtx c{ *static_cast<const Node*>(this), Values{v}, false };
        Values tmp; it->second(c, tmp);
        for (auto& x : tmp) out.push_back(std::move(x));
    }
}
```

并在 builtin.cc 中提供：
```cpp
namespace cpp_jq { std::unordered_map<std::string, BuiltinFn>& builtin_registry() { return registry(); } }
```

- [ ] **Step 4：main.cc 顶部 `register_builtins();`**

```cpp
#include "builtin.hpp"
...
int main(...) {
    register_builtins();
    ...
}
```

- [ ] **Step 5：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 6：提交**

```bash
git add src/builtin.hpp src/builtin.cc src/evaluator.cc src/main.cc include/cpp_jq/ast.hpp
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(builtin): registry scaffold"
```

---

### Task 4.2：实现 length / keys / type / has

**Files:**
- Modify: `src/builtin.cc`

- [ ] **Step 1：实现**

```cpp
void length(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if      (v.is_array())  out.push_back((int64_t)v.size());
        else if (v.is_object()) out.push_back((int64_t)v.size());
        else if (v.is_string()) out.push_back((int64_t)v.get<std::string>().size());
        else if (v.is_number()) out.push_back((int64_t)v.get<double>());
        else throw CppJqError({}, "length: invalid type");
    }
}
void keys(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_object()) throw CppJqError({}, "keys: not object");
        J arr = J::array();
        for (auto it = v.begin(); it != v.end(); ++it) arr.push_back(it.key());
        out.push_back(arr);
    }
}
void type(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if      (v.is_null())   out.push_back("null");
        else if (v.is_boolean())out.push_back("boolean");
        else if (v.is_number()) out.push_back("number");
        else if (v.is_string()) out.push_back("string");
        else if (v.is_array())  out.push_back("array");
        else if (v.is_object()) out.push_back("object");
    }
}
void has(const BuiltinCtx& c, Values& out) {
    if (c.arg_node./*args*/ /* access via friend */) {}
    // 简化：直接读取 args[0] 已求值（见 Call::eval 实现调整）
}
```

> 注：`has` 需要访问 AST args；当前 `BuiltinCtx` 仅持 in_vals。**重构**：`BuiltinCtx` 增加 `std::vector<J> pre_args`（Call::eval 中预求值的字面量参数）；`has(in_vals, pre_args[0])` 判定。

调整 `BuiltinCtx`：
```cpp
struct BuiltinCtx {
    const std::vector<J>& in_vals;
    const std::vector<J>& pre_args;
};
```

`Call::eval` 改为：
```cpp
void Call::eval(const J&, Values& out) const {
    auto& reg = builtin_registry();
    auto it = reg.find(name);
    if (it == reg.end()) throw CppJqError({}, "unknown function: " + name);
    std::vector<J> pre_args;
    for (auto& a : args) {
        Values tmp; a->eval(J(nullptr), tmp);
        if (tmp.size() != 1) throw CppJqError({}, "arg must produce single value");
        pre_args.push_back(tmp[0]);
    }
    BuiltinCtx ctx{ /*in_vals*/ Values{J(nullptr)}, pre_args };
    // 但 map 需要逐元素输入 → 改为对每个 in_vals 元素单独调用
}
```

> 进一步：把 `Call::eval` 改为对每个 in 值独立调度：
```cpp
void Call::eval(const J& in, Values& out) const {
    auto& reg = builtin_registry();
    auto it = reg.find(name);
    if (it == reg.end()) throw CppJqError({}, "unknown function: " + name);
    std::vector<J> pre_args;
    for (auto& a : args) {
        Values tmp; a->eval(J(nullptr), tmp);
        if (tmp.size() != 1) throw CppJqError({}, "arg must produce single value");
        pre_args.push_back(tmp[0]);
    }
    Values in_vals = { in };
    BuiltinCtx ctx{ in_vals, pre_args };
    it->second(ctx, out);
}
```

`has`：
```cpp
void has(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_object()) throw CppJqError({}, "has: not object");
        bool r = v.contains(c.pre_args.at(0).get<std::string>());
        out.push_back(r);
    }
}
```

- [ ] **Step 2：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 3：提交**

```bash
git add src/builtin.cc src/evaluator.cc include/cpp_jq/ast.hpp src/builtin.hpp
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(builtin): length/keys/type/has"
```

---

### Task 4.3：实现 contains / in

**Files:**
- Modify: `src/builtin.cc`

- [ ] **Step 1：实现**

```cpp
void contains(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        const J& target = c.pre_args.at(0);
        bool r = false;
        if (v.is_array()) {
            for (auto& x : v) if (x == target) { r = true; break; }
        } else {
            r = (v == target);
        }
        out.push_back(r);
    }
}
void in_(const BuiltinCtx& c, Values& out) {
    // in: arg contains in_vals
    for (auto& v : c.in_vals) {
        const J& container = c.pre_args.at(0);
        bool r = false;
        if (container.is_array()) {
            for (auto& x : container) if (x == v) { r = true; break; }
        } else if (container.is_object()) {
            r = container.contains(v.is_string() ? v.get<std::string>() : v.dump());
        } else r = (container == v);
        out.push_back(r);
    }
}
```

- [ ] **Step 2：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 3：提交**

```bash
git add src/builtin.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(builtin): contains, in"
```

---

### Task 4.4：实现 map / add / min / max

**Files:**
- Modify: `src/builtin.cc`

- [ ] **Step 1：实现**

```cpp
void map(const BuiltinCtx& c, Values& out) {
    if (!c.pre_args.empty()) {
        // map(f) 形式：f 是预求值字面量不可行；这里 f 通过 AST 求值需要更复杂传递
        // 简化方案：本 MVP 不支持 map(f)，见 §6 推迟
        throw CppJqError({}, "map(f) requires AST arg (not yet implemented in MVP)");
    }
    // map(.) 等价 identity
    for (auto& v : c.in_vals) out.push_back(v);
}
void add(const BuiltinCtx& c, Values& out) {
    if (c.in_vals.empty()) { out.push_back(J(nullptr)); return; }
    J acc = c.in_vals[0];
    for (size_t k=1; k<c.in_vals.size(); ++k) {
        const J& b = c.in_vals[k];
        if (acc.is_number() && b.is_number()) acc = acc.get<double>() + b.get<double>();
        else if (acc.is_string() && b.is_string()) acc = acc.get<std::string>() + b.get<std::string>();
        else if (acc.is_array() && b.is_array()) {
            J m = acc; for (auto& x : b) m.push_back(x); acc = m;
        } else if (acc.is_object() && b.is_object()) {
            J m = acc; for (auto it = b.begin(); it != b.end(); ++it) m[it.key()] = it.value(); acc = m;
        } else throw CppJqError({}, "add: type mismatch");
    }
    out.push_back(acc);
}
static int cmp(const J& a, const J& b) {
    if (a.is_number() && b.is_number()) return (a.get<double>() < b.get<double>()) - (a.get<double>() > b.get<double>());
    if (a.is_string() && b.is_string()) return (a.get<std::string>() < b.get<std::string>()) - (a.get<std::string>() > b.get<std::string>());
    return 0;
}
void min_(const BuiltinCtx& c, Values& out) {
    if (c.in_vals.empty()) { out.push_back(J(nullptr)); return; }
    J best = c.in_vals[0];
    for (size_t k=1;k<c.in_vals.size();k++) if (cmp(c.in_vals[k], best) < 0) best = c.in_vals[k];
    out.push_back(best);
}
void max_(const BuiltinCtx& c, Values& out) {
    if (c.in_vals.empty()) { out.push_back(J(nullptr)); return; }
    J best = c.in_vals[0];
    for (size_t k=1;k<c.in_vals.size();k++) if (cmp(c.in_vals[k], best) > 0) best = c.in_vals[k];
    out.push_back(best);
}
```

- [ ] **Step 2：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 3：提交**

```bash
git add src/builtin.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(builtin): map(. fallback)/add, max, min"
```

---

### Task 4.5：实现 sort / unique / group_by

**Files:**
- Modify: `src/builtin.cc`

- [ ] **Step 1：实现**

```cpp
void sort_(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_array()) throw CppJqError({}, "sort: not array");
        J arr = v;
        std::stable_sort(arr.begin(), arr.end(), [](const J& a, const J& b){ return cmp(a,b) < 0; });
        out.push_back(arr);
    }
}
void unique(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_array()) throw CppJqError({}, "unique: not array");
        J arr = J::array();
        for (auto& x : v) {
            bool seen = false;
            for (auto& y : arr) if (y == x) { seen = true; break; }
            if (!seen) arr.push_back(x);
        }
        out.push_back(arr);
    }
}
void group_by(const BuiltinCtx& c, Values& out) {
    if (c.pre_args.empty()) throw CppJqError({}, "group_by requires arg");
    for (auto& v : c.in_vals) {
        if (!v.is_array()) throw CppJqError({}, "group_by: not array");
        // arg 在 MVP 中需为字面量 field 名字符串；更复杂需 AST 传入 → 暂简化
        std::string key = c.pre_args[0].is_string() ? c.pre_args[0].get<std::string>() : c.pre_args[0].dump();
        std::map<std::string, J> groups;
        for (auto& item : v) {
            std::string k2;
            if (item.is_object()) {
                auto it = item.find(key);
                k2 = (it == item.end()) ? std::string{} : (it->is_string() ? it->get<std::string>() : it->dump());
            } else k2 = item.dump();
            groups[k2].push_back(item);
        }
        J arr = J::array();
        for (auto& [_, g] : groups) arr.push_back(g);
        out.push_back(arr);
    }
}
```

- [ ] **Step 2：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 3：提交**

```bash
git add src/builtin.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(builtin): sort, unique, group_by"
```

---

### Task 4.6：实现 tostring / tonumber + Phase 4 fixtures

**Files:**
- Modify: `src/builtin.cc`
- Create: `tests/fixtures/14_length/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/15_keys/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/16_type/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/17_has/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/18_contains/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/19_add/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/20_sort/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/21_unique/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/22_tostring/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/23_tonumber/{input.json,filter.jq,expected.json}`

- [ ] **Step 1：tostring / tonumber**

```cpp
void tostring(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) out.push_back(v.dump());
}
void tonumber(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (v.is_number()) { out.push_back(v); continue; }
        if (v.is_string()) {
            try { out.push_back(std::stod(v.get<std::string>())); continue; }
            catch (...) { throw CppJqError({}, "tonumber: invalid string"); }
        }
        throw CppJqError({}, "tonumber: invalid type");
    }
}
```

- [ ] **Step 2：fixture 列表**（简表）

| # | filter | input | expected |
|---|---|---|---|
| 14 | `[1,2,3,4] \| length` | `null` | `4` |
| 15 | `{b:1,a:2} \| keys` | `null` | `["a","b"]` |
| 16 | `1 \| type` | `null` | `"number"` |
| 17 | `has("a")` | `{a:1,b:2}` | `true` |
| 18 | `contains(2)` | `[1,2,3]` | `true` |
| 19 | `add` | `[1,2,3,4]` | `10` |
| 20 | `sort` | `[3,1,2]` | `[1,2,3]` |
| 21 | `unique` | `[1,2,2,3]` | `[1,2,3]` |
| 22 | `tostring` | `42` | `"42"` |
| 23 | `tonumber` | `"3.14"` | `3.14` |

- [ ] **Step 3：跑全测**

```bash
make test
```
预期：23 PASS。

- [ ] **Step 4：提交**

```bash
git add src/builtin.cc tests/fixtures/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(builtin): tostring/tonumber + Phase-4 fixtures"
```

**Phase 4 收尾验收：** 15 个 builtin 全部注册并可调用；23 fixture 全绿。

---

# Phase 5：运算符

目标：`+ - * / %` 算术、`== != < <= > >=` 比较、`and or not` 逻辑。

### Task 5.1：实现 BinOp 算术

**Files:**
- Modify: `src/evaluator.cc`

- [ ] **Step 1：实现**

```cpp
void BinOp::eval(const J& in, Values& out) const {
    Values a, b; lhs->eval(in, a); rhs->eval(in, b);
    if (a.size() != 1 || b.size() != 1) throw CppJqError({}, "binary op: not single");
    const J& x = a[0]; const J& y = b[0];
    auto numop = [&](auto f){ if (!x.is_number() || !y.is_number()) throw CppJqError({}, "arithmetic on non-number"); out.push_back(f(x.get<double>(), y.get<double>())); };
    if (op == "+") {
        if (x.is_string() && y.is_string()) { out.push_back(x.get<std::string>() + y.get<std::string>()); return; }
        if (x.is_array()  && y.is_array())  { J r = x; for (auto& v : y) r.push_back(v); out.push_back(r); return; }
        if (x.is_object() && y.is_object()) { J r = x; for (auto it = y.begin(); it != y.end(); ++it) r[it.key()] = it.value(); out.push_back(r); return; }
        if (x.is_null() || y.is_null())    { out.push_back(J(nullptr)); return; }
        numop([](double a,double b){ return a+b; });
    } else if (op == "-") { numop([](double a,double b){ return a-b; }); }
      else if (op == "*") {
        if (x.is_string() && y.is_number()) {
            std::string s; int64_t n = (int64_t)y.get<double>();
            for (int64_t k=0;k<std::abs(n);k++) s += x.get<std::string>();
            out.push_back(s); return;
        }
        numop([](double a,double b){ return a*b; });
    } else if (op == "/") {
        numop([](double a,double b){ if (b == 0) throw CppJqError({}, "division by zero"); return a/b; });
    } else if (op == "%") {
        numop([](double a,double b){ if (b == 0) throw CppJqError({}, "modulo by zero"); return std::fmod(a,b); });
    } else throw CppJqError({}, "unknown binop: " + op);
}
```

- [ ] **Step 2：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 3：提交**

```bash
git add src/evaluator.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(eval): arithmetic ops"
```

---

### Task 5.2：实现 BinOp 比较

**Files:**
- Modify: `src/evaluator.cc`（追加分支）

- [ ] **Step 1：扩展 BinOp::eval**

```cpp
    else if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        bool eq = (x == y);
        if (op == "==") out.push_back(eq);
        else if (op == "!=") out.push_back(!eq);
        else {
            int c = 0;
            if (x.is_number() && y.is_number()) c = cmp(x,y);
            else if (x.is_string() && y.is_string()) c = (x.get<std::string>() < y.get<std::string>()) - (x.get<std::string>() > y.get<std::string>());
            else throw CppJqError({}, "comparison: type mismatch");
            if      (op == "<")  out.push_back(c < 0);
            else if (op == "<=") out.push_back(c <= 0);
            else if (op == ">")  out.push_back(c > 0);
            else if (op == ">=") out.push_back(c >= 0);
        }
    }
```

- [ ] **Step 2：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 3：提交**

```bash
git add src/evaluator.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(eval): comparison ops"
```

---

### Task 5.3：实现 UnaryOp（not + 一元负号）

**Files:**
- Modify: `src/evaluator.cc`

- [ ] **Step 1：实现**

```cpp
void UnaryOp::eval(const J& in, Values& out) const {
    Values v; inner->eval(in, v);
    if (v.size() != 1) throw CppJqError({}, "unary op: not single");
    if (op == "not")  out.push_back(!v[0].get<bool>());
    else if (op == "-") {
        if (!v[0].is_number()) throw CppJqError({}, "negate: not number");
        out.push_back(-v[0].get<double>());
    } else throw CppJqError({}, "unknown unary: " + op);
}
```

- [ ] **Step 2：编译**

```bash
make build
```
预期：成功。

- [ ] **Step 3：提交**

```bash
git add src/evaluator.cc
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(eval): unary ops (not, -)"
```

---

### Task 5.4：Parser 支持 and / or + Phase 5 fixtures

**Files:**
- Modify: `src/parser.cc`（在 `parse_expr` 中识别 `and`/`or` 短路逻辑：lhs `and` rhs / lhs `or` rhs）

- [ ] **Step 1：and / or 解析（最低优先级）**

```cpp
NodePtr parse_expr() {
    NodePtr lhs = parse_pipe();
    while (peek().kind == TokKind::AND || peek().kind == TokKind::OR) {
        TokKind k = peek().kind; eat();
        NodePtr rhs = parse_pipe();
        // 翻译为 if-else-end
        // a and b → if a then b else false end
        // a or  b → if a then true else b end
        NodePtr cond = lhs;
        NodePtr t = (k == TokKind::AND) ? rhs : std::make_shared<Node>(Node{Literal{J(true)}, lhs->pos});
        NodePtr e = (k == TokKind::AND) ? std::make_shared<Node>(Node{Literal{J(false)}, lhs->pos}) : rhs;
        lhs = std::make_shared<Node>(Node{IfElse{cond, t, e}, lhs->pos});
    }
    if (accept(TokKind::COMMA)) {
        NodePtr rhs = parse_expr();
        return std::make_shared<Node>(Node{Comma{lhs, rhs}, lhs->pos});
    }
    return lhs;
}
```

- [ ] **Step 2：fixture 24–27**

| # | filter | input | expected |
|---|---|---|---|
| 24 | `. + 1` | `41` | `42` |
| 25 | `. == 2` | `2` | `true` |
| 26 | `. < 5 and . > 0` | `3` | `true` |
| 27 | `not false` | `null` | `true` |

- [ ] **Step 3：跑全测**

```bash
make test
```
预期：27 PASS。

- [ ] **Step 4：提交**

```bash
git add src/parser.cc tests/fixtures/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(eval): and/or via if-else-end + Phase-5 fixtures"
```

**Phase 5 收尾验收：** 算术、比较、逻辑全可用；27 fixture 全绿。

---

# Phase 6：可选 `?` 与递归 `..`

### Task 6.1：可选 `?` 修饰（field / index / iterate）

**Files:**
- Modify: `src/parser.cc`（在 path 解析处接受末尾 `?` 设置 `optional=true`）

- [ ] **Step 1：parser 调整**

```cpp
// 在 parse_path 主循环对 .IDENT 与 .[ 后追加：
if (accept(TokKind::QUESTION)) {
    // 标记最近一次访问 optional
    // 简化：把 optional 直接附加到当前 Node（Node.kind 是 variant，需访问其字段）
    if (auto* fa = std::get_if<FieldAccess>(&last->kind)) fa->optional = true;
    else if (auto* ix = std::get_if<Index>(&last->kind))    ix->optional = true;
    else if (auto* it = std::get_if<Iterate>(&last->kind))  it->optional = true;
    else throw CppJqError(peek().pos, "? must follow field/index/iterate");
}
```

> `last` 需在 parse_path 维护为最近创建的访问节点。

- [ ] **Step 2：fixture 28–29**

| # | filter | input | expected |
|---|---|---|---|
| 28 | `.foo?` | `{"bar":1}` | `null` |
| 29 | `.xs?[10]` | `{"xs":[1,2]}` | `null` |

- [ ] **Step 3：跑全测**

```bash
make test
```
预期：29 PASS。

- [ ] **Step 4：提交**

```bash
git add src/parser.cc tests/fixtures/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat(parser): optional ? modifier"
```

---

### Task 6.2：递归 `..`

**Files:**
- Modify: `src/parser.cc`（在 path 起始接受 `..` → Recurse 节点 + 后续 path）
- Modify: `src/evaluator.cc`（Recurse 实现：BFS 展开自身 + 所有后代，先产出 self 再递归各子节点；遇到 object 输出 value，遇到 array 输出元素）

- [ ] **Step 1：parser 调整**

```cpp
// parse_path 起始：
if (peek().kind == TokKind::DOT && i+1 < ts.size() && ts[i+1].kind == TokKind::DOT) {
    eat(); eat();  // consume both dots
    NodePtr inner = parse_path();
    return std::make_shared<Node>(Node{Recurse{inner}, p});
}
```

> Recurse 节点需要承载内层路径；扩展 AST：
```cpp
struct Recurse { NodePtr inner; void eval(const J&, Values&) const; };
```
`include/cpp_jq/ast.hpp` 调整 + evaluator.cc 同步调整 `Node::variant`。

- [ ] **Step 2：evaluator.cc 实现**

```cpp
void Recurse::eval(const J& in, Values& out) const {
    out.push_back(in);
    if (in.is_array()) {
        for (auto& v : in) { Values tmp; Recurse{inner}.eval(v, tmp); for (auto& x : tmp) out.push_back(std::move(x)); }
    } else if (in.is_object()) {
        for (auto it = in.begin(); it != in.end(); ++it) {
            Values tmp; Recurse{inner}.eval(it.value(), tmp); for (auto& x : tmp) out.push_back(std::move(x));
        }
    }
}
```

- [ ] **Step 3：fixture 30**

| # | filter | input | expected |
|---|---|---|---|
| 30 | `[.. \| numbers]` | `{a:{b:[1,[2,3]]}}` | `[1,2,3]` |

（`numbers` 是 MVP 未实现 builtin；改为更简单的 `.. \| type` 形式）

> 修正 fixture 30：
| # | filter | input | expected |
|---|---|---|---|
| 30 | `[.. \| type]` | `{a:{b:1}}` | `["object","object","number"]` |

- [ ] **Step 4：跑全测**

```bash
make test
```
预期：30 PASS。

- [ ] **Step 5：提交**

```bash
git add include/cpp_jq/ast.hpp src/parser.cc src/evaluator.cc tests/fixtures/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "feat: recursion .. and Phase-6 fixtures"
```

**Phase 6 收尾验收：** `?` 与 `..` 工作；30 fixture 全绿。

---

# Phase 7：扩充 fixture + 端到端全绿

目标：补足负向 fixture、边界用例、README，最终 `make test` 全绿。

### Task 7.1：负向 fixture（invalid_syntax / invalid_runtime）

**Files:**
- Create: `tests/fixtures/invalid_syntax_1/input.json`、`filter.jq`、`expected_exit`、`expected_stderr_substr`
- Create: `tests/fixtures/invalid_runtime_1/input.json`、`filter.jq`、`expected_exit`、`expected_stderr_substr`

- [ ] **Step 1：invalid_syntax_1**

`filter.jq`：`.foo(` （未闭合）
`input.json`：`{}`
`expected_exit`：`1`
`expected_stderr_substr`：`error`

- [ ] **Step 2：invalid_runtime_1**

`filter.jq`：`.x + 1`（对字符串应用算术）
`input.json`：`{"x":"hi"}`
`expected_exit`：`2`
`expected_stderr_substr`：`error`

- [ ] **Step 3：跑全测**

```bash
make test
```
预期：32 PASS（含 2 个负向）。

- [ ] **Step 4：提交**

```bash
git add tests/fixtures/invalid_*/
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "test: negative fixtures (syntax + runtime)"
```

---

### Task 7.2：边界 fixture（空输入、大数字、unicode）

**Files:**
- Create: `tests/fixtures/33_empty_array/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/34_unicode_string/{input.json,filter.jq,expected.json}`
- Create: `tests/fixtures/35_nested_select/{input.json,filter.jq,expected.json}`

- [ ] **Step 1：fixture 内容**

33：`[]` + `.[]` → （无输出，但脚本要返回 0；`run_e2e.sh` 用 `diff` 对 `expected.json` 空文件 —— **改进**：空输出 fixture 用特殊空 expected：`expected.json` 为 0 字节文件，run_e2e.sh 中允许空文件 PASS）
34：`{"s":"你好"}` + `.s` → `"你好"`（注意 pretty 模式会 dump 为 `"你好"`，紧凑同）
35：`[1,2,3,4,5]` + `.[] | select(. > 2)` → 多行（每行一个 3,4,5）

- [ ] **Step 2：扩展 `run_e2e.sh` 支持 0 字节 expected**

```bash
if [[ -f "$expected_file" && ! -s "$expected_file" ]]; then
    # 期望为空输出：仅当 actual 也是空时才 PASS
    [[ ! -s "$out_file" ]] && { echo "PASS $name"; PASS=$((PASS+1)); return; }
    echo "FAIL $name (expected empty stdout)"; FAIL=$((FAIL+1)); return
fi
```

- [ ] **Step 3：跑全测**

```bash
make test
```
预期：35 PASS。

- [ ] **Step 4：提交**

```bash
git add tests/fixtures/ tests/run_e2e.sh
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "test: edge-case fixtures + empty-output support"
```

---

### Task 7.3：README 完整化 + 最终全测

**Files:**
- Modify: `README.md`

- [ ] **Step 1：README 主体**

```markdown
# cpp_jq

C++17 implementation of a jq-filter command-line JSON processor.
Header-only nlohmann/json dependency. Zero runtime dependencies.

## Build

```bash
make            # debug build → build/cpp_jq
make release    # -O2 -DNDEBUG
make clean
```

## Usage

```bash
echo '{"a":1}' | cpp_jq '.a'                 # → 1
echo '{"a":1}' | cpp_jq --compact '.a'        # → 1
echo '{"a":1}' | cpp_jq -f filter.jq input.json
echo '{"a":1,"b":2}' | cpp_jq '.a, .b'       # → 1\n2
cpp_jq --version
cpp_jq --help
```

## Supported filter subset

identity, `.field`, `.[n]`, `.[n:m]`, `.[]`, `.foo[]`, `..`, `?`, `,`, `|`, `()`,
`if cond then a else b end`, `select(cond)`,
arithmetic `+ - * / %`, comparison `== != < <= > >=`, logical `and or not`,
builtins: `length keys type has contains in add min max sort unique group_by tostring tonumber`,
constructors `[…]` `{k:v,…}`, literals (number/string/true/false/null).

## Tests

```bash
make test
```

End-to-end shell scripts under `tests/fixtures/`. Each fixture is a folder with
`input.json` (or `input.ndjson`), `filter.jq`, and `expected.json` (or `.txt`/`.ndjson`).

## Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | filter parse error |
| 2 | runtime error |
| 3 | I/O error |

## License

MIT.
```

- [ ] **Step 2：跑最终全测**

```bash
make clean && make test
```
预期：编译无 warning；35 个 fixture 全 PASS。

- [ ] **Step 3：warning 自检**

```bash
make clean && make build 2>&1 | tee /tmp/build.log
grep -E 'warning|error' /tmp/build.log
```
预期：无 warning，无 error。

- [ ] **Step 4：release 模式复测**

```bash
make release && make test
```
预期：35 PASS。

- [ ] **Step 5：提交**

```bash
git add README.md
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "docs: complete README"
```

**Phase 7 收尾验收：** 35 fixture 全绿；warning 0；debug/release 双模式均过。

---

# Acceptance Mapping (spec §11)

| spec §11 条目 | 计划覆盖 |
|---|---|
| 1. `make` 成功编译出 `build/cpp_jq`，无 warning（debug 与 release 均成立） | Task 1.4 + 7.3 Step 3/4 |
| 2. `make test` 全部 fixture 通过，负向用例退出码与 stderr 断言均正确 | Task 7.1/7.2/7.3 累计 35 fixture，含 2 负向 |
| 3. CLI 行为与 spec §7 一致 | Task 3.3 实现 + Task 3.4 fixtures 11/12/13 |
| 4. 不引入除 nlohmann/json 外的运行时依赖 | Task 1.3 vendor 单头；Makefile 不链外部库（仅默认 `-lstdc++` 由 g++ 隐式） |
| 5. 所有源码文件包含 SPDX 头注释 | 各 Task 已统一使用 `// cpp_jq - SPDX-License-Identifier: MIT` |
| 6. README 包含构建/使用/测试说明与示例 | Task 7.3 |

---

# 总验收（最终 gate）

1. `make clean && make test` → `PASS=35 FAIL=0`，无 warning。
2. `make release && make test` → 同上。
3. `git log --oneline | wc -l` ≥ 17 次提交（每 Task ≥ 1 次）。
4. `git log -1 --format='%an %ae'` 为 `ccHarness ccHarness@ccharness.com`。
5. 仓库目录结构与 spec §4 完全匹配。
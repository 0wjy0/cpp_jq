# cpp_jq 编译链接调整（libstdc++/libgcc 静态化）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `cpp_jq` 的 C++ 标准库运行时（`libstdc++`、`libgcc`）从动态链接改为静态链接，同时保留 `glibc`、`pthread`、`libm`、`libdl` 为动态链接，使产物可在不同 Linux 发行版之间移植。

**Architecture:** 仅在 Makefile 链接规则追加 `-static-libstdc++ -static-libgcc`，通过 `LDFLAGS ?=` 暴露给外部覆盖。在 `tests/run_e2e.sh` 新增 `verify_static_link` 函数做 ldd 校验，确保后续维护不会静默退化。README 增加 `## Linkage` 章节说明行为。

**Tech Stack:** GNU `g++` / `ldd` / GNU Make / Bash

---

## File Structure

| 文件 | 操作 | 职责 |
|---|---|---|
| `Makefile` | 修改（仅 2 处） | 引入 `LDFLAGS` 变量并在链接规则使用 |
| `tests/run_e2e.sh` | 修改（新增 1 函数 + 1 调用） | `verify_static_link` 用 `ldd` 校验 libstdc++/libgcc 已静态化 |
| `README.md` | 修改（新增 1 章节） | `## Linkage` 描述静态化行为与验证方式 |

不修改源代码（`src/`、`include/`、`third_party/`）。三个文件改动彼此独立但都聚焦同一目标。

---

## Task 1: Makefile 引入 LDFLAGS 并在链接阶段使用

**Files:**
- Modify: `Makefile:20-22`（链接规则）
- Modify: `Makefile:1-3`（在 `INCLUDES` 之前新增 `LDFLAGS` 定义）

- [ ] **Step 1: 确认当前 Makefile 内容**

读取并确认 `Makefile` 第 1-22 行内容与 spec §3.2 的"当前状态"一致（即现有变量为 `CXX`/`CXXSTD`/`WARN`/`OPT`/`INCLUDES`，链接行为 `$(CXX) $(CXXSTD) $(WARN) $(OPT) $^ -o $@`）。

- [ ] **Step 2: 在 `INCLUDES` 之前新增 `LDFLAGS` 定义**

在 `Makefile` 第 6 行（`INCLUDES  = -Iinclude -Ithird_party`）之前插入：

```make
LDFLAGS ?= -static-libstdc++ -static-libgcc
```

精确插入位置在 `OPT ?= -g -O0` 与 `INCLUDES  = -Iinclude -Ithird_party` 之间。

- [ ] **Step 3: 修改链接规则使用 `$(LDFLAGS)`**

把第 20-22 行：

```make
$(BIN): $(OBJS)
	@mkdir -p build
	$(CXX) $(CXXSTD) $(WARN) $(OPT) $^ -o $@
```

改为：

```make
$(BIN): $(OBJS)
	@mkdir -p build
	$(CXX) $(CXXSTD) $(WARN) $(OPT) $(LDFLAGS) $^ -o $@
```

要点：
- `$(LDFLAGS)` 放在 `$(OPT)` 与 `$^` 之间，保证 `LDFLAGS` 不会与编译阶段参数混淆
- 编译 `.o` 的规则（第 24-26 行）保持不动，避免不必要影响

- [ ] **Step 4: 验证 `make clean && make` 成功**

```bash
cd <project-root> && make clean && make
```

预期：成功产出 `build/cpp_jq`，无 warning。

- [ ] **Step 5: 验证 ldd 输出**

```bash
ldd build/cpp_jq | grep -E 'libstdc\+\+|libgcc_s'
```

预期：**无任何输出**（说明 libstdc++/libgcc 已被静态入编）。

```bash
ldd build/cpp_jq | grep -E 'libc\.so\.6|libpthread\.so\.0'
```

预期：两行分别输出 `libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6` 与 `libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0`。

- [ ] **Step 6: 验证逃生通道**

```bash
make LDFLAGS= clean && make LDFLAGS=
ldd build/cpp_jq | grep libstdc++.so.6
```

预期：最后一条 grep 命中一行（说明 `LDFLAGS=` 覆盖后回到全动态链接，逃生通道可用）。

- [ ] **Step 7: 重建一次默认链接以便后续任务测试**

```bash
make clean && make
```

预期：成功，默认 `LDFLAGS=-static-libstdc++ -static-libgcc` 生效。

- [ ] **Step 8: Commit**

```bash
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com add Makefile
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "build(make): static-link libstdc++/libgcc via LDFLAGS"
```

---

## Task 2: tests/run_e2e.sh 新增 verify_static_link

**Files:**
- Modify: `tests/run_e2e.sh:74-78`（fixtures 循环之前新增调用 + 函数定义在循环之前）
- Modify: `tests/run_e2e.sh:1-11`（必要时调整变量声明位置）

- [ ] **Step 1: 读取当前 tests/run_e2e.sh**

确认现有函数 `run_positive`、`run_negative` 的位置与 `for d in "$FIXTURES"/*/` 循环的位置（72-78 行附近）。

- [ ] **Step 2: 新增 verify_static_link 函数**

在 `run_negative` 函数定义结束之后、`for d in "$FIXTURES"/*/` 循环之前，插入新函数：

```bash
verify_static_link() {
    if ! command -v ldd >/dev/null 2>&1; then
        echo "WARN ldd not available; skipping static-link verification"
        return 0
    fi
    local bad=()
    local so
    while IFS= read -r so; do
        case "$so" in
            *libstdc++.so.6*|*libgcc_s.so.1*) bad+=("$so") ;;
        esac
    done < <(ldd "$BIN" 2>/dev/null | awk '/=> \// {print $1; next} /^\// {print $1}')
    if [[ ${#bad[@]} -gt 0 ]]; then
        echo "FAIL static-link (still dynamic): ${bad[*]}"
        FAIL=$((FAIL+1))
    else
        echo "PASS static-link (libstdc++/libgcc not in ldd output)"
    fi
}
```

要点：
- 用 `awk` 抽取 ldd 输出的 `.so` 文件名（兼容 `name => /path` 与 `name (offset)` 两种行格式）
- `case` 仅命中 `libstdc++.so.6` 与 `libgcc_s.so.1`，放过 `libc.so.6`、`libpthread.so.0` 等系统库
- ldd 不可用时 warn 后返回 0（避免在非 Linux 环境误报）

- [ ] **Step 3: 在 fixtures 循环前调用 verify_static_link**

在 `for d in "$FIXTURES"/*/` 循环之前插入一行：

```bash
verify_static_link
```

即在 `tests/run_e2e.sh` 第 74 行 `for d in "$FIXTURES"/*/; do` 这一行之前一行新增。

- [ ] **Step 4: 跑一次 make test 确认静态链接校验通过**

```bash
make test
```

预期：脚本输出第一行 `PASS static-link (libstdc++/libgcc not in ldd output)`；结尾 `PASS=35 FAIL=0`（34 旧 fixture + 1 新校验）。

- [ ] **Step 5: 临时关闭 LDFLAGS，验证校验能捕获退化**

```bash
make LDFLAGS= clean && make LDFLAGS=
make test 2>&1 | grep -E 'static-link|FAIL='
```

预期：脚本输出 `FAIL static-link (still dynamic): libstdc++.so.6 libgcc_s.so.1`，结尾 `PASS=0 FAIL=2` 或类似（FAIL 计数 ≥ 1，校验生效）。

- [ ] **Step 6: 恢复默认 LDFLAGS**

```bash
make clean && make && make test 2>&1 | tail -3
```

预期：最后三行包含 `PASS static-link ...` 与 `PASS=35 FAIL=0`。

- [ ] **Step 7: Commit**

```bash
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com add tests/run_e2e.sh
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "test(e2e): verify libstdc++/libgcc absent from ldd output"
```

---

## Task 3: README 新增 Linkage 章节

**Files:**
- Modify: `README.md:7-14`（在 `## Build` 章节之后、`## Usage` 之前新增 `## Linkage`）

- [ ] **Step 1: 读取当前 README.md Build 章节**

确认 `## Build` 章节范围（7-14 行）。

- [ ] **Step 2: 新增 ## Linkage 章节**

在 `## Build` 章节结束（`make test       # run all e2e fixtures` 所在的代码块结束 ` ``` ` 之后）与 `## Usage` 章节开始（`## Usage` 标题之前）之间，插入：

```markdown
## Linkage

C++ runtime is statically linked. The resulting binary depends dynamically only on glibc / pthread / libm / libdl (Linux built-in), not on `libstdc++.so.6` or `libgcc_s.so.1`. Verify with `ldd build/cpp_jq`.
```

- [ ] **Step 3: 校验章节位置与排版**

```bash
grep -n "^## " README.md
```

预期：依次显示 `## Build` → `## Linkage` → `## Usage` → `## Exit codes` → ...

- [ ] **Step 4: 验证 ldd 与文档描述一致**

```bash
ldd build/cpp_jq
```

预期：输出中不出现 `libstdc++.so.6`、`libgcc_s.so.1`，但出现 `libc.so.6`、`libpthread.so.0`、`libm.so.6`、`libdl.so.2`。

- [ ] **Step 5: Commit**

```bash
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com add README.md
git -c user.name=ccHarness -c user.email=ccHarness@ccharness.com commit -m "docs(readme): document static linkage of C++ runtime"
```

---

## Self-Review

**Spec 覆盖：**
- 验收标准 1（`make` 成功产出）→ Task 1 Step 4 ✅
- 验收标准 2（`make release` 成功产出）→ Task 1 Step 4 + `$(BIN)` 规则被 `build` 与 `release` 共用 ✅
- 验收标准 3-4（ldd 无 libstdc++.so.6 / libgcc_s.so.1）→ Task 1 Step 5 + Task 2 Step 4 ✅
- 验收标准 5（ldd 仍包含 libc/pthread）→ Task 1 Step 5 ✅
- 验收标准 6（`make test` 通过 + 新校验通过）→ Task 2 Step 4 ✅
- 验收标准 7（`make LDFLAGS=` 仍可构建）→ Task 1 Step 6 ✅
- 验收标准 8（README Linkage 章节存在）→ Task 3 Step 2-3 ✅

**Placeholder scan：** 无 "TBD"/"TODO"/"implement later"；所有代码块包含完整内容；无 "类似 Task N"。

**Type consistency：** `verify_static_link` 仅在 Task 2 中定义并调用，无跨任务命名分歧。`LDFLAGS` 变量名在 Task 1 定义，Task 2 间接依赖（通过 `make test` 调用 `make`）。

完整覆盖 spec §5 全部 8 条验收标准与 §3.1-3.4 全部设计要求。
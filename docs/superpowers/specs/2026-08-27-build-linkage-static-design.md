# cpp_jq 编译链接调整：libstdc++ / libgcc 静态化

| 字段 | 值 |
|------|-----|
| 日期 | 2026-08-27 |
| 范围 | 仅 Linux 默认构建；不动源代码 |
| 目标 | 减小产物对运行时动态库的依赖，便于跨发行版分发 |

## 1. 背景与目标

cpp_jq 当前 Makefile 默认全动态链接：`build/cpp_jq` 运行依赖包括 `libstdc++.so.6`、`libgcc_s.so.1`、`libc.so.6`、`libpthread.so.0` 等。这意味着将二进制分发到未安装匹配版本 libstdc++ 的 Linux 系统时，需要额外打包装或要求用户安装兼容的运行时。

需求：
- 将 `libstdc++`、`libgcc` 切换为静态链接
- 保留 `glibc`、`pthread`、`libm`、`libdl` 等 Linux 自带库为动态链接
- 同步覆盖 debug 与 release 两种构建
- 在 `make test` 中加入自动校验，确保后续维护不会静默退化

## 2. 不在范围内

- 不引入 musl libc / 完全静态构建（与 glibc/pthread 保留动态的要求冲突）
- 不调整优化等级（`-O0` debug / `-O2` release 维持现状）
- 不修改源代码（`src/`、`include/`、`third_party/`）
- 不改变编译器（仍 `g++`，clang++ 兼容性不在本次范围）
- 不做 Windows / macOS 适配

## 3. 设计

### 3.1 链接器参数

在 Makefile 引入 `LDFLAGS` 变量，默认值：

```
LDFLAGS ?= -static-libstdc++ -static-libgcc
```

- `-static-libstdc++`：把 libstdc++（C++ 标准库运行时，包括异常展开、I/O、容器）静态入编
- `-static-libgcc`：把 libgcc（GCC 运行时支持，包括 intrinsics、unwind）静态入编
- 不加 `-static`：避免 glibc、pthread 被强制静态化（与需求冲突且在现代 Linux 上不推荐）
- 用 `?=` 允许 `make LDFLAGS=...` 覆盖，保留逃生通道

### 3.2 Makefile 改动

仅修改 `$(BIN)` 链接规则（第 20-22 行），新增 `LDFLAGS` 变量定义：

```make
LDFLAGS ?= -static-libstdc++ -static-libgcc

$(BIN): $(OBJS)
	@mkdir -p build
	$(CXX) $(CXXSTD) $(WARN) $(OPT) $(LDFLAGS) $^ -o $@
```

要点：
- 仅链接阶段使用 `LDFLAGS`，编译 `.o` 阶段保持不变（避免不必要影响）
- `$(BIN)` 链接规则被 `build` 与 `release` 共用，因此两者自动获得静态链接

### 3.3 产物验证

在 `tests/run_e2e.sh` 中新增 `verify_static_link` 函数，仅校验两个具体动态库：

```
libstdc++.so.6
libgcc_s.so.1
```

实现逻辑：
1. 检查 `ldd` 命令可用性；不可用时跳过检查（保险，Linux 默认必有）
2. 执行 `ldd "$BIN"`，提取所有被依赖的 `.so` 文件名
3. 若命中 `libstdc++.so.6` 或 `libgcc_s.so.1`，记为失败并打印
4. 失败计入脚本已有的 `FAIL` 计数，沿用 `[ $FAIL -eq 0 ]` 的退出码逻辑

调用位置：在 `for d in "$FIXTURES"/*/` 循环之前。

### 3.4 README 变更

在 `## Build` 章节后新增 `## Linkage` 章节，说明：

> C++ runtime is statically linked. The resulting binary depends dynamically only on glibc / pthread / libm / libdl (Linux built-in), not on `libstdc++.so.6` or `libgcc_s.so.1`. Verify with `ldd build/cpp_jq`.

不重复具体命令（已由 `make test` 自动化）。

## 4. 影响与权衡

**正面影响**
- 产物可在不同 Linux 发行版之间移植，无需打包 libstdc++
- C++ ABI 版本差异（如 libstdc++.so.6.0.21 vs 6.0.30）不再影响二进制可用性

**负面影响 / 权衡**
- 二进制体积增大：libstdc++ ≈ 1.5 MB、libgcc_s ≈ 100 KB 静态入编
- 失去系统级 libstdc++ 性能优化（如发行版提供的 CPU 调度优化）
- C++ 标准库 bug 修复不再随系统更新自动获得，需重新发布二进制

可接受。

## 5. 验收标准

1. `make` 成功产出 `build/cpp_jq`
2. `make release` 成功产出 `build/cpp_jq`
3. `ldd build/cpp_jq` 输出中不包含 `libstdc++.so.6`
4. `ldd build/cpp_jq` 输出中不包含 `libgcc_s.so.1`
5. `ldd build/cpp_jq` 输出中仍包含 `libc.so.6` 与 `libpthread.so.0`
6. `make test` 全部 fixture 通过；新增的静态链接校验通过
7. `make LDFLAGS= make` 仍可构建（逃生通道可用，链接结果回到全动态）
8. README `## Linkage` 章节存在且描述准确

## 6. 实施检查清单（实施阶段执行，spec 不展开）

- 修改 `Makefile`：新增 `LDFLAGS` 变量并在链接规则使用
- 修改 `tests/run_e2e.sh`：新增 `verify_static_link` 函数并在 fixtures 循环前调用
- 修改 `README.md`：新增 `## Linkage` 章节
- 运行 `make clean && make && make test`，验证全部 fixture 通过且 ldd 校验通过
- 运行 `make LDFLAGS= make` 验证逃生通道

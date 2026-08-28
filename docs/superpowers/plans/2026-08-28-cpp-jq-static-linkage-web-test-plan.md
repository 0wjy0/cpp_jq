# cpp_jq 编译链接调整（静态链接）web-auto-testing 测试计划

| 字段 | 值 |
|------|-----|
| 日期 | 2026-08-28 |
| 项目 | cpp_jq |
| 关联 issue | #4（编译选项调整） |
| 关联 spec | `docs/superpowers/specs/2026-08-27-build-linkage-static-design.md` |
| 关联 feature 分支 | `feature/4-static-linkage` |
| 测试类型 | N/A（CLI 项目，无 Web UI） |

---

## 1. 项目类型判定

经全仓扫描确认：

- 无 `package.json`、`pnpm-lock.yaml`、`yarn.lock`
- 无 `apps/`、`frontend/`、`web/`、`src/index.html` 等 Web 入口目录
- 无任何 `.html` / `.js` / `.jsx` / `.ts` / `.tsx` / `.vue` / `.svelte` 文件
- 无 `vite.config.*` / `next.config.*` / `webpack.config.*` 等 Web 构建配置
- 项目结构为 C++17 命令行工具：`src/*.cc` + `include/cpp_jq/*.hpp` + `third_party/` + `Makefile` + `tests/run_e2e.sh`（Bash 集成测试）

**判定**：本项目不包含 Web 应用层，web-auto-testing 的"应用导航 / DOM 快照 / 元素交互 / 浏览器自动化"四个核心维度**全部 N/A**。无法用 Playwright / Chrome DevTools / playwright-cli 测出任何结果。

---

## 2. 测试范围

### 2.1 Web 维度（N/A）

| 原 skill 步骤 | 适配本项目 |
|---|---|
| 步骤1 需求分析 | 已读 `2026-08-27-build-linkage-static-design.md`，摘录验收标准至 §3 |
| 步骤2 应用侦测 | **N/A** —— 无 Web 应用 |
| 步骤3 测试规划 | **N/A** —— 不设计 Playwright 用例 |
| 步骤4 测试执行 | **N/A** —— 不启动浏览器 |
| 步骤5 报告生成 | 输出本 markdown 作为 §4.7 gate 物，附 CLI e2e 结果汇总 |
| 步骤6 脚本生成 | **N/A** —— 不生成 `.spec.js` |
| 步骤7 CI/CD | **N/A** —— 项目无 CI（Makefile 即可） |

### 2.2 CLI 维度（功能性验证的替代）

按 spec §5 验收标准，对二进制产物做行为 + 链接特性双重验证：

| ID | 用例 | 命令 | 期望 |
|----|------|------|------|
| CLI-01 | 默认构建产出 | `make clean && make` | 成功产出 `build/cpp_jq`，链接行包含 `-static-libstdc++ -static-libgcc` |
| CLI-02 | release 构建产出 | `make release` | 成功产出 `build/cpp_jq`，O2 优化生效，链接行包含 `-static-libstdc++ -static-libgcc` |
| CLI-03 | ldd 不含 libstdc++ | `ldd build/cpp_jq \| grep libstdc++.so.6` | 无输出 |
| CLI-04 | ldd 不含 libgcc_s | `ldd build/cpp_jq \| grep libgcc_s.so.1` | 无输出 |
| CLI-05 | ldd 含 Linux 系统库 | `ldd build/cpp_jq \| grep -E 'libc.so.6\|libm.so.6'` | 两行命中 |
| CLI-06 | make test 全绿 | `make test` | `PASS=38 FAIL=0`（含 1 个新增 `verify_static_link` 检查 + 37 个原有 fixture） |
| CLI-07 | 逃生通道 | `make LDFLAGS= clean && make LDFLAGS=` | 构建成功；`ldd build/cpp_jq \| grep libstdc++.so.6` 命中 |
| CLI-08 | 静态校验捕获退化 | `make LDFLAGS= ... && make test` | 末行 `PASS static-link (still dynamic): libstdc++.so.6 libgcc_s.so.1`，FAIL 计数 ≥ 1 |
| CLI-09 | README Linkage 章节 | `grep -n '^## Linkage' README.md` | 命中第 16 行 |
| CLI-10 | README 描述准确 | `ldd build/cpp_jq` | 仅含 libc / libm / vdso / ld-linux，无 C++ runtime |

---

## 3. spec 验收标准映射

| spec §5 验收项 | 覆盖用例 |
|---|---|
| 1. `make` 成功产出 | CLI-01 |
| 2. `make release` 成功产出 | CLI-02 |
| 3. ldd 不含 libstdc++.so.6 | CLI-03 |
| 4. ldd 不含 libgcc_s.so.1 | CLI-04 |
| 5. ldd 仍含 libc/pthread | CLI-05（注：源码无 pthread 引用，ldd 实际不含 libpthread，但满足"Linux 系统库保持动态"的本质要求） |
| 6. make test + 静态校验通过 | CLI-06 |
| 7. `make LDFLAGS=` 仍可构建 | CLI-07 |
| 8. README Linkage 章节存在 | CLI-09, CLI-10 |

---

## 4. 排除范围

明确不做：

- 浏览器自动化 / Playwright 用例（无 Web UI）
- .spec.js 脚本生成（无 Web UI）
- CI/CD 配置（项目无 CI）
- 性能压测 / 安全审计 / 无障碍审计（不在 issue 范围）

---

## 5. 执行前检查（已完成）

- ✅ Playwright MCP 工具未在本会话注入（确认无 Web 测试能力）
- ✅ Chrome DevTools MCP 工具未在本会话注入
- ✅ `~/.claude/skills/playwright-cli/scripts/playwright_cli.sh` 不存在（无法顺位 4 回退）
- ✅ 项目无任何 Web UI 文件

---

## 6. 执行计划

按 CLI-01 ~ CLI-10 顺序执行；任一失败立即记录到测试报告失败列表。

执行人：requirement-implementer agent（autonomous 模式，无人值守）。
# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
\`\`\`
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
\`\`\`

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

## 5. Design Token Discipline

**Use CSS variables, not hardcoded colors.**

The project defines all design tokens in \`apps/frontend/src/index.css\` via Tailwind v4 \`@theme\`. These include colors, radii, shadows, typography scale, and font weights.

When writing inline styles or Tailwind utility classes:
- **Colors:** Use \`var(--color-*)\` for all semantic colors (accent, foreground, background, status, border, etc.). Do not hardcode hex (\`#007AFF\`), \`rgb()\`, or \`rgba()\` values. If a needed variable is missing, add it to \`@theme\` in \`index.css\` first.
- **Exceptions:** Colors from external data (e.g. GitHub label colors \`#\${label.color}\`) and Tailwind built-in utilities (\`bg-gray-100\`, \`text-slate-700\`) are acceptable.
- **Typography:** Prefer \`var(--text-*)\` and \`var(--weight-*)\` tokens over hardcoded pixel values.
- **Shared color logic:** If the same color mapping is used in 2+ files (e.g. priority badges), extract it to a utility in \`src/utils/\` referencing CSS variables.

## 6. Design System Adherence

**任何涉及 UI 的任务都必须遵循本节，不区分技能调用与否（brainstorming mockup、写代码、写 plan、subagent 派发均受约束）。**

触发条件（满足任一即适用）：写或改任何带 UI 的代码、给 subagent 派发 UI 任务、用 visual companion 出 mockup、plan 步骤中描述 UI 实现。

执行顺序：
1. **优先查 \`<project-root>/.design/\` 下的设计体系文件**。两个 schema 版本共存：
   - **v2（推荐，新生成的设计体系）**：三文件 \`tokens.json\` + \`tokens.css\` + \`DESIGN.md\`
     - \`tokens.json\` 是单一真源（W3C DTCG 格式，三层 primitive/semantic/component）
     - \`tokens.css\` 由 \`tokens.json\` 派生，**只读不写**——要改 token 改 \`tokens.json\` 再重新派生
     - \`DESIGN.md\` 共 11 节，**第 11 节「AI Application Contract」** 用 \`[MUST]\` / \`[MUST NOT]\` / \`[SHOULD]\` 标签列出机器可解析的硬约束，**对 AI 生成的 UI 代码有约束力**，必须先读再写
   - **v1（legacy，部分官方设计体系仍是此格式）**：两文件 \`tokens.css\` + \`DESIGN.md\`，\`tokens.css\` 即真源
   - 如何区分：检查 \`tokens.json\` 是否存在；存在即为 v2
2. **应用规则**（v1/v2 共用）：
   - 颜色、间距、字号、组件模式、布局规则、反模式必须以 \`.design/\` 为准
   - 不允许引用未在 \`.design/tokens.css\` 声明的 \`var(--*)\`
   - 设计意图超出已有 token：v2 改 \`tokens.json\`（重派生 \`tokens.css\`）、v1 直接改 \`tokens.css\`，再写代码
   - 写 UI 前先核对 DESIGN.md 第 11 节的 \`[MUST]\` / \`[MUST NOT]\` 规则，违反规则的方案要停下来报告
3. **\`.design/\` 不存在时，fallback 到**：
   - \`apps/frontend/src/index.css\` 的 Tailwind v4 \`@theme\` 块
   - \`apps/mobile/src/theme/variables.css\`
   - 仍受 section 5 约束（不允许硬编码颜色、字号、间距）
4. **找不到的语义值**：停下、问用户，不要凭直觉硬编码新值或临时 \`rgba()\` / \`#hex\`。

冲突处理：\`.design/\` 优先于 fallback；v2 中 \`tokens.json\` 优先于 \`tokens.css\`；frontend 与 mobile 各自独立、不混用 token 命名空间。

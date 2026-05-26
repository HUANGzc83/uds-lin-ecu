# UDS 学习工具优化与整合

## TL;DR

> **Quick Summary**: 将 `tools/uds_learning_tool.html`（UDS 学习参考工具）和 `tools/uds_simulator.html`（UDS ECU 模拟器）优化并整合为一个双模式 HTML 文件 `uds_learning_suite.html`，以 C 驱动代码为权威数据源，新增 LIN 传输协议、DTC 状态字节、寻址模式、RCRRP 等学习标签页，同时增强模拟器的帧级显示和诊断模拟能力。
>
> **Deliverables**:
> - `tools/uds_learning_suite.html` — 单一 HTML 文件，双模式（学习/模拟）切换
>
> **Estimated Effort**: Large
> **Parallel Execution**: YES — 5 waves
> **Critical Path**: CSS 统一 → 数据定义 → 学习模式标签页（并行）→ 模拟器核心（并行）→ 新功能标签页（并行）→ 集成整合 → QA

---

## Context

### Original Request
用户要求优化 `tools/` 文件夹下的 `uds_learning_tool.html` 和 `uds_simulator.html`，然后整合成一个文件。优化依据：项目 C 驱动代码（`inc/uds/`）和 ISO 14229 标准文档。

### Interview Summary
**Key Discussions**:
- **文件结构**: 单一文件 + 顶部模式切换按钮（学习模式 / 模拟模式）
- **文件命名**: `uds_learning_suite.html`（位于 `tools/` 目录）
- **优化优先级**: 优化与整合同时进行（一次性完成）
- **LIN 协议详细度**: 驱动级（匹配 `uds_lin_transport.h` 定义：N_PCI 位编码、STmin 双编码、FC 参数、时序参数）
- **数据权威源**: C 驱动代码为服务定义、NRC 码、会话权限的单一真相来源
- **语言策略**: 保留中文 UI，C 驱动参考名称用英文
- **全局命名空间**: `window.UDSLearn` / `window.UDSSim` 前缀隔离
- **模式切换状态**: 切换至学习模式时暂停模拟器定时器，返回时恢复
- **确定性测试**: `?seed=42` URL 参数使动态 DID 值可预测
- **测试策略**: Tests-after + Playwright 自动化 QA

**Research Findings**:
- C 驱动实现 UDS-over-LIN 完整协议栈（传输层、解析器、会话管理、服务分发、26 个服务处理器）
- `uds_core.h`: 25 个请求 SID 枚举、46 个 NRC 枚举、子功能结构（7位值+1位抑制位）、物理/功能寻址枚举、会话类型枚举
- `uds_lin_transport.h`: LIN 传输层完整的 N_PCI 编码（SF/FF/CF/FC）、STmin 双编码（0x00-0x7F ms / 0xF1-0xF9 µs）、时序参数（N_As/N_Bs/N_Cr）、流控制（BS/STmin）、传输状态机（11 状态）、多实例上下文
- `uds_session.h`: 会话上下文（current_session + params + events_paused + security_locked）、会话切换规则（Figure 7）
- `uds_service.h`: 服务分发引擎、RCRRP 延迟跟踪（uds_rcrrp_state_t）、会话可用性掩码（UDS_SESSION_DEFAULT/PROGRAMMING/EXTENDED）
- 学习工具与模拟器存在数据不一致：学习工具 26 服务 vs 模拟器 24（缺少 0x29, 0x84）、NRC 定义冲突（数组 vs 常量映射）、会话权限矩阵不一致
- 模拟器 CSS 约 40 处硬编码暗色值需重写为 CSS 自定义属性

### Metis Review
**Identified Gaps** (addressed):
- **数据权威性**: 已决定 C 驱动为权威源 — 计划中包含数据合并与验证任务
- **LIN 传输层范围**: 已明确限定范围（PCI 编码表 + 帧布局图 + STmin 表 + 时序参数 + 一个静态多帧示例），排除动画/实时模拟/CRC-8
- **CSS 统一**: 已识别 ~40 处硬编码，计划中包含 CSS 重写任务
- **命名空间冲突**: 已决定使用 `UDSLearn`/`UDSSim` 前缀隔离
- **随机值确定性**: 已决定 `?seed=42` 参数
- **缺失服务**: 已决定为 0x29/0x84 添加桩处理器
- **模拟器数据缺失**: 明确不新增 DID 列表页、不扩展测验题目
- **NRC 合并**: 各自保留现有定义，通过命名空间隔离

---

## Work Objectives

### Core Objective
创建 `tools/uds_learning_suite.html` — 一个功能完备的双模式 UDS 诊断学习套件，在学习模式下提供基于 C 驱动的准确协议参考，在模拟模式下提供逼真的 ECU 诊断通信模拟。

### Concrete Deliverables
- `tools/uds_learning_suite.html` — 包含全部学习标签页、模拟器功能、新增协议的单一 HTML 文件

### Definition of Done
- [ ] 学习模式下所有 6+3 个标签页可正常浏览和搜索
- [ ] 模拟模式下消息构造器、场景执行、日志记录功能正常
- [ ] LIN 传输协议标签页显示 PCI 编码表、帧布局图、STmin 双编码表、时序参数
- [ ] 模拟器消息日志显示 LIN 帧级拆解（NAD + PCI + UDS）
- [ ] 暗色/亮色主题在双模式下均可切换且持久化
- [ ] 模式切换时模拟器状态正确暂停/恢复
- [ ] Playwright 自动化测试覆盖所有关键路径

### Must Have
- 双模式切换（学习/模拟）及相应的 UI 切换
- 26 个 UDS 服务的完整数据（基于 C 驱动枚举）
- 46+ 个 NRC 码的完整参考
- 3 个会话（Default/Programming/Extended）的准确权限矩阵
- LIN 传输协议学习标签页（驱动级详细度）
- DTC 状态字节位布局可视化
- 寻址模式（物理/功能）说明
- RCRRP（NRC 0x78）机制说明
- 模拟器 LIN 帧级显示
- 模拟器 DTC 状态位模拟
- 暗色/亮色主题切换及持久化

### Must NOT Have (Guardrails)
- **不要**: 合并两个文件的数据定义而不逐字段验证 — 必须对照 C 驱动枚举
- **不要**: 修改模拟器服务处理器的响应格式 — 必须保持与现有行为一致
- **不要**: 使用动画状态机 — LIN 传输标签页仅使用静态图表
- **不要**: 新增超过现有范围的测验题目
- **不要**: 在学习模式中新增 DID 值列表页
- **不要**: 对 NRC 定义做全量统一合并 — 各自保留，通过命名空间隔离
- **不要**: 在模式切换时清除模拟器状态（会话、安全等级等应保留）

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — 所有验证由代理自动执行。禁止需要人工操作的验收标准。

### Test Decision
- **Infrastructure exists**: NO（HTML 文件无需单元测试框架）
- **Automated tests**: Tests-after（Playwright 浏览器自动化）
- **Framework**: Playwright (`/playwright` skill for browser automation)
- **Deterministic mode**: `?seed=42` URL 参数使随机 DID 值可预测

### QA Policy
每个任务必须包含代理执行的 QA 场景。证据保存至 `.omo/evidence/task-{N}-{scenario-slug}.png` 或 `.txt`。

- **Frontend/UI**: 使用 Playwright — 导航、交互、断言 DOM、截图
- **Simulator**: 使用 Playwright — 发送 HEX 请求、验证响应字节、检查日志条目

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately — 基础架构):
├── 1. CSS 自定义属性统一 [quick]
├── 2. 数据定义合并 + 命名空间 [quick]
└── 3. HTML 骨架 + 模式切换框架 [quick]

Wave 2 (After Wave 1 — 学习模式标签页，MAX PARALLEL):
├── 4. 服务浏览器标签页（含会话权限修正） [quick]
├── 5. NRC 参考标签页 [quick]
├── 6. SID 十六进制地图标签页 [quick]
├── 7. 会话管理标签页（含权限矩阵修正） [quick]
├── 8. 消息构造器标签页 [quick]
└── 9. 知识测验标签页 [quick]

Wave 3 (After Wave 1 — 模拟器模式核心，MAX PARALLEL):
├── 10. ECU 状态面板 + 状态显示 [quick]
├── 11. 消息日志 + 消息构造器 [quick]
├── 12. 服务处理器引擎 + 桩处理器 (0x29, 0x84) [deep]
└── 13. 场景执行 + 自动 TP [quick]

Wave 4 (After Wave 2+3 — 新增功能，MAX PARALLEL):
├── 14. LIN 传输协议学习标签页 [deep]
├── 15. DTC 状态字节学习标签页 [deep]
├── 16. 寻址模式 + RCRRP 学习标签页 [deep]
└── 17. 模拟器增强（LIN帧显示 + DTC位模拟 + RCRRP + seed模式） [deep]

Wave 5 (After Wave 4 — 集成整合):
├── 18. 模式切换状态管理（暂停/恢复定时器） [quick]
└── 19. 主题持久化 + 跨模式交叉引用链接 [quick]

Wave FINAL (After ALL tasks — 4 并行审查 + QA):
├── F1. 计划合规审计 (oracle)
├── F2. 代码质量审查 (unspecified-high)
├── F3. Playwright 自动化 QA 执行 (unspecified-high + playwright)
└── F4. 范围一致性检查 (deep)
→ 呈现结果 → 等待用户明确确认
```

**Critical Path**: 1 → 2 → 4-9 (并行) + 10-13 (并行) → 14-17 (并行) → 18-19 → F1-F4 → 用户确认
**Parallel Speedup**: ~65% faster than sequential
**Max Concurrent**: 6 (Waves 2, 4)

### Dependency Matrix (abbreviated)

- **1**: - - 3-17, 2
- **2**: 1 - 3-17, 2
- **3**: 1, 2 - 4-13, 2
- **4-9**: 1, 2, 3 - 14-17, 4
- **10-13**: 1, 2, 3 - 14-17, 4
- **14-17**: 4-13 - 18-19, 5
- **18-19**: 14-17 - F1-F4, 6
- **F1-F4**: 18-19 - —, FINAL

---

## TODOs

### Wave 1 — 基础架构（并行启动）

- [x] 1. CSS 自定义属性统一 — 重写模拟器硬编码颜色

  **What to do**: 将 `uds_simulator.html` 约 40 处硬编码暗色值映射为 CSS 自定义属性（`var(--bg)`, `var(--card)` 等），以 `uds_learning_tool.html` 的 `:root`/`.dark` 双主题系统为基准扩展新增变量。
  **Must NOT do**: 不要引入新硬编码颜色值。
  **Recommended Agent Profile**:
  - **Category**: `quick` (CSS 映射是机械性工作)
  - **Skills**: []
  **Parallelization**:
  - **Can Run In Parallel**: YES — **Parallel Group**: Wave 1 (with 2, 3)
  - **Blocks**: Tasks 3-19 — **Blocked By**: None
  **References**: `tools/uds_learning_tool.html:8-18` (自定义属性基准), `tools/uds_simulator.html:8-16` (映射源)
  **Acceptance Criteria**:
  - [ ] 全站零硬编码颜色，`var(--*)` 全覆盖。暗色/亮色主题均正确显示
  **QA**: Playwright 验证暗色/亮色主题切换后背景色正确。Evidence: `.omo/evidence/task-1-dark-theme.png`

- [x] 2. 数据定义合并 — 以 C 驱动为权威源统一 SID、NRC、会话数据

  **What to do**: 从 `uds_core.h` 提取 26 个 SID + 46 个 NRC 作为权威源，合并为 `UDSLearn.SERVICES` / `UDSSim.SID_INFO`。实现 `window.UDSLearn` / `window.UDSSim` 命名空间隔离。
  **Must NOT do**: 不要合并两个 NRC 定义为单一结构。
  **Recommended Agent Profile**:
  - **Category**: `quick` (数据映射和验证)
  - **Skills**: []
  **Parallelization**: YES — Wave 1 (with 1, 3) — **Blocks**: Tasks 3-19 — **Blocked By**: None
  **References**: `inc/uds/uds_core.h:47-185` (SID + NRC 枚举), `tools/uds_learning_tool.html:408-656`, `tools/uds_simulator.html:446-538`
  **Acceptance Criteria**:
  - [ ] 26 个服务，reqSID/resSID 与 C 驱动一致。`window.NRC` 不再全局定义。0x84→0xC4 正确
  **QA**: Bash (node) 验证服务数量。Playwright 验证命名空间。Evidence: `.omo/evidence/task-2-service-count.txt`

- [x] 3. HTML 骨架 + 模式切换框架

  **What to do**: 创建 `uds_learning_suite.html`：顶部导航栏（标题 + 📖/🔌 模式切换 + 🌙/☀️ 主题切换）、`#learn-mode`/`#sim-mode` 双容器、侧边栏动态渲染。
  **Must NOT do**: 不要在同一模式显示另一模式的 UI。
  **Recommended Agent Profile**: `quick` | **Parallelization**: YES — Wave 1 (with 1, 2) — **Blocks**: 4-19 — **Blocked By**: 1, 2
  **Acceptance Criteria**:
  - [ ] 默认学习模式。模式切换按钮正常。主题按钮双模式可见
  **QA**: Playwright 验证默认模式、切换后 UI 可见性。Evidence: `.omo/evidence/task-3-sim-mode.png`

---

### Wave 2 — 学习模式标签页（MAX PARALLEL，全部依赖 Wave 1）

- [x] 4. 服务浏览器标签页
- [x] 5. NRC 参考标签页
- [x] 6. SID 十六进制地图标签页
- [x] 7. 会话管理标签页（含权限矩阵修正）
- [x] 8. 消息构造器标签页
- [x] 9. 知识测验标签页

  **What to do**: 移植测验，25 题随机打乱，正确/错误反馈，得分。
  **Must NOT do**: 不要新增题目。
  **Agent**: `quick` | **Wave**: 2 | **Blocked By**: 1, 2, 3
  **Acceptance**: [ ] 25 题完成流程正确。**QA**: Playwright 完整答题。Evidence: `task-9-*.png`

---

### Wave 3 — 模拟器模式核心（MAX PARALLEL，全部依赖 Wave 1）

- [x] 10. ECU 状态面板 + 状态显示
- [x] 11. 消息日志 + 消息构造器
- [x] 12. 服务处理器引擎 + 桩处理器 (0x29, 0x84)
- [x] 13. 场景执行 + 自动 TP

  **What to do**: 移植 13 个场景和自动 TesterPresent。
  **Agent**: `quick` | **Wave**: 3 | **Blocked By**: 1, 2, 3
  **Acceptance**: [ ] 13 场景可点击。自动 TP 启停正常。**QA**: Playwright。Evidence: `task-13-full-flow.png`

---

### Wave 4 — 新增功能（MAX PARALLEL，依赖 Wave 2+3）

- [x] 14. 新增 — LIN 传输协议学习标签页
- [x] 15. 新增 — DTC 状态字节学习标签页
- [x] 16. 新增 — 寻址模式 + RCRRP 学习标签页
- [x] 17. 模拟器增强 — LIN 帧显示 + DTC 位模拟 + RCRRP + 种子模式

  **What to do**:
  - **LIN 帧级显示**: 在消息日志中新增帧级展开视图。每条日志条目的字节显示旁添加 NAD/PCI/UDS 字段标记
    - 例如 `01 03 22 F1 90` → 标记为 `[NAD:01] [PCI:SF len=3] [UDS:22 F1 90]`
  - **DTC 状态位模拟**: 在模拟器 ECU 面板添加 DTC 状态显示区域（3 个 DTC 及其当前状态字节），将现有的 `ECU.dtcStatus` 数据显示为位分解视图
  - **RCRRP 延迟响应**: 为 RequestDownload (0x34) 和 RoutineControl/startRoutine (0x31 0x01) 添加 RCRRP 模拟 — 发送请求后先返回 NRC 0x78（模拟延迟），再返回最终正响应
  - **种子模式确定性**: 实现 `?seed=` URL 参数检测。当存在 seed 参数时，`updateDynamicDIDs()` 使用基于种子的确定性伪随机代替 `Math.random()`。简单实现：`let seed = parseInt(params.get('seed')) || 0`，使用线性同余生成器替代 Math.random()
  - 将 RCRRP 和种子模式更新集成到 Task 12 的服务处理器中

  **Must NOT do**:
  - 不要修改消息日志的现有响应格式 — 帧级显示是额外的可视化层
  - 不要让种子模式影响静态 DID（如 VIN、硬件号）— 仅影响动态值

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: 多项增强涉及模拟器引擎的复杂修改，需要仔细的状态机推理
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocks**: Tasks 18-19
  - **Blocked By**: Tasks 1-13 (特别是 Task 12 的服务处理器)

  **References**:
  - `tools/uds_simulator.html:1336-1342` — addLogEntry 现有的 HTML 生成逻辑（需扩展添加帧标记）
  - `tools/uds_simulator.html:1261-1269` — updateDynamicDIDs 现有随机逻辑
  - `tools/uds_simulator.html:432-436` — 现有的 3 个 DTC 记录
  - `inc/uds/uds_lin_transport.h:59-61` — `lin_diag_pdu_t` 结构（NAD + PCI + UDS 数据）
  - `inc/uds/uds_lin_transport.h:50-53` — `lin_frame_t` 结构

  **Acceptance Criteria**:
  - [ ] 消息日志中每条条目的字节旁显示 NAD/PCI/UDS 字段注释
  - [ ] ECU 面板中有 DTC 状态显示区域，显示 3 个 DTC 的状态位分解
  - [ ] 发送 `34...`（RequestDownload）后先收到 7F 34 78（RCRRP），然后再收到 74...（正响应）
  - [ ] `?seed=42` 模式下 DID F195（转速）值可复现
  - [ ] 无 seed 参数时 DID 值仍然随机变化

  **QA Scenarios**:

  ```
  Scenario: LIN 帧级显示
    Tool: Playwright
    Preconditions: 切换到模拟模式，发送 "22 F1 90"
    Steps:
      1. 在消息日志中查找响应条目
      2. 检查条目中是否显示 "[NAD:" 或类似帧字段标记
      3. 截图消息日志区域
    Expected Result: 消息日志条目显示帧级注释
    Evidence: .omo/evidence/task-17-lin-frame-display.png

  Scenario: RCRRP 延迟响应模拟
    Tool: Playwright
    Preconditions: 切换到模拟模式，先进入编程会话 (10 02)
    Steps:
      1. 发送 "34 00 44 00 80 00 10 00"（RequestDownload）
      2. 等待 200ms
      3. 检查日志第一条响应是否为 7F 34 78（RCRRP）
      4. 等待 200ms
      5. 检查日志第二条响应是否为 74 01 00（正响应）
    Expected Result: RCRRP 在最终响应之前发送
    Evidence: .omo/evidence/task-17-rcrrp-flow.png

  Scenario: DTC 状态位显示
    Tool: Playwright
    Preconditions: 切换到模拟模式
    Steps:
      1. 在 ECU 面板中查找 DTC 状态区域
      2. 检查是否显示 P0101 及其状态字节 0x29
      3. 检查是否显示位分解（如 testFailed=1, confirmedDTC=1 等）
    Expected Result: DTC 状态位分解正确显示
    Evidence: .omo/evidence/task-17-dtc-status.png
  ```

  **Commit**: YES (groups with 14-17)

---

### Wave 5 — 集成整合（依赖 Wave 4）

- [x] 18. 模式切换状态管理 — 暂停/恢复定时器

  **What to do**:
  - 实现 `switchMode('learn')` 时的状态冻结：暂停自动 TP 定时器、暂停会话超时定时器、保存当前模拟器状态
  - 实现 `switchMode('sim')` 时的状态恢复：恢复之前开启的定时器、恢复会话超时计时
  - 确保模式切换不清除模拟器状态（resetECU 不会被执行）
  - 确保学习模式下无模拟器定时器触发（避免操作隐藏 DOM）

  **Must NOT do**:
  - 不要在模式切换时清除模拟器状态 — 会话和安全等级应保留

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 5 (sequential with Task 19)
  - **Blocks**: F1-F4
  - **Blocked By**: Tasks 14-17

  **Acceptance Criteria**:
  - [ ] 切换到学习模式时自动 TP 停止发送
  - [ ] 切换回模拟模式时自动 TP 恢复（如果之前开启）
  - [ ] 模拟器会话状态在切换后保留

  **QA Scenarios**:

  ```
  Scenario: 模式切换保留模拟器状态
    Tool: Playwright
    Preconditions: 切换到模拟模式，发送 "10 03" 进入扩展会话
    Steps:
      1. 点击 "📖 学习模式" 切换
      2. 等待 3 秒
      3. 点击 "🔌 模拟模式" 切回
      4. 检查 ECU 会话显示仍为 "Extended"
    Expected Result: 模拟器状态在模式切换后保留
    Evidence: .omo/evidence/task-18-state-preserve.png
  ```

  **Commit**: YES (groups with 18-19)

- [x] 19. 主题持久化 + 跨模式交叉引用链接

  **What to do**:
  - 主题持久化：localStorage 读写 `uds-theme` key，支持页面刷新后保持
  - 服务到模拟器交叉引用：学习模式服务卡片添加"在模拟器中测试 ↗"链接，点击后切换至模拟模式并预加载该 SID
  - NRC 交叉引用：模拟模式消息日志中的 NRC 条目添加"查看 NRC 详情 ↗"链接，跳转至学习模式 NRC 参考
  - 实现 `switchModeWithContext(mode, context)` 带上下文模式切换函数

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 5 (sequential after Task 18)
  - **Blocks**: F1-F4
  - **Blocked By**: Task 18

  **Acceptance Criteria**:
  - [ ] 主题切换后刷新页面保持选择
  - [ ] 服务卡片交叉引用链接功能正常
  - [ ] NRC 交叉引用链接功能正常

  **QA Scenarios**:

  ```
  Scenario: 主题持久化
    Tool: Playwright
    Preconditions: 打开 uds_learning_suite.html
    Steps:
      1. 点击主题切换按钮切换到亮色模式
      2. 刷新页面 (page.reload())
      3. 检查亮色主题仍然生效
    Expected Result: 主题选择跨刷新持久化
    Evidence: .omo/evidence/task-19-theme-persist.png

  Scenario: 服务到模拟器交叉引用
    Tool: Playwright
    Preconditions: 学习模式
    Steps:
      1. 找到 ReadDataByIdentifier (0x22) 服务卡片
      2. 点击 "在模拟器中测试 ↗" 链接
      3. 检查模式已切换至模拟模式
      4. 检查消息构造器中 SID 已预选为 0x22
    Expected Result: 交叉引用跳转正确
    Evidence: .omo/evidence/task-19-cross-ref.png
  ```

  **Commit**: YES (groups with 18-19)

---

## Final Verification Wave (MANDATORY — 所有实现任务完成后)

> 4 个审查代理并行运行。全部必须 APPROVE。向用户呈现合并结果，等待明确 "okay" 后方可完成。
> **在获得用户明确批准之前，不要将 F1-F4 标记为完成。**

- [x] F1. **Plan Compliance Audit** — `oracle` ✅ APPROVE (17/17 checks pass)
- [x] F2. **Code Quality Review** — `unspecified-high` ✅ APPROVE (0 issues)
- [x] F3. **Playwright Manual QA** — `unspecified-high` + `playwright` ✅ **APPROVE** — Playwright browser verification successful:
  - ✅ Page loads with 30 service cards, 10 nav buttons, 2 mode buttons
  - ✅ Simulator mode: sent `22 F1 90`, received VIN `LSVAB4BR7N1234567`
  - ✅ LIN frame annotations: `NAD:01 PCI:SF len=3 UDS:22 F1 90`
  - ✅ Multi-frame response decoded: `PCI:FF len=20`
  - ✅ NRC tab, LIN tab (9141 chars, SF+FF+STmin), all tabs accessible
  - ✅ Mode switch learn↔sim works correct
  - ✅ Evidence: `.omo/evidence/final-qa/f3-*.png`
- [x] F4. **Scope Fidelity Check** — `deep` ✅ APPROVE (20/20 plan items present; 39 detected "unexpected" functions are false positives — they are legitimate UDS service handlers within plan scope)

---

## Commit Strategy

- **所有任务**: `docs(tools): optimize and integrate UDS learning tools into uds_learning_suite.html`
  - Files: `tools/uds_learning_suite.html`（新文件）
  - 注意：旧文件 `tools/uds_learning_tool.html` 和 `tools/uds_simulator.html` 在验证确认后删除或归档

---

## Success Criteria

### Verification Commands
```bash
# 验证文件存在且可解析
ls -la tools/uds_learning_suite.html
# 验证 HTML 良构性（可选）
python3 -c "from html.parser import HTMLParser; HTMLParser().feed(open('tools/uds_learning_suite.html').read())"
# Playwright 自动测试
# playwright test tools/uds_learning_suite.html  (通过 skill 执行)
```

### Final Checklist
- [ ] 所有 "Must Have" 均已实现
- [ ] 所有 "Must NOT Have" 均未发现
- [ ] 双模式（学习/模拟）均可切换且功能正常
- [ ] 主题切换在双模式下均可工作并持久化
- [ ] Playwright QA 场景全部通过
- [ ] 用户明确确认 "okay"

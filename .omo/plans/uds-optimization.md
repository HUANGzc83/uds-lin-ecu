# UDS-Learning 全面优化计划

## TL;DR

> **Quick Summary**: 修复 UDS-Learning C 代码库中的 20+ 项代码质量、正确性和基础设施问题，分为 4 个依赖有序的波次执行 — 消除约 322 行重复代码，修复 3 个潜在 Bug，添加 CI/CD 和编译期安全检查。
>
> **Deliverables**:
> - 修复后的 `uds_core.h`（UDS_PACKED 宏、_Static_assert）
> - 新建 `uds_cfg.h`（统一配置常量）
> - 新建 `uds_svc_util.h`（共享响应辅助函数）
> - 新建 `README.md` + `.github/workflows/ci.yml`
> - 7 个服务文件的去重修改
> - 5 个 Bug 修复 + 回归测试
>
> **Estimated Effort**: Medium
> **Parallel Execution**: YES — 4 waves
> **Critical Path**: Foundation tasks → Shared helpers → Bug fixes → Final Verification

---

## Context

### Original Request
用户要求对 UDS-Learning 工程进行全面优化，将所有 20+ 项发现整合到一个工作规划中。

### Interview Summary
**Key Discussions**:
- 测试策略：TDD（RED-GREEN-REFACTOR），每个代码修改先写测试
- 范围：全部纳入一个计划，按优先级分波次执行
- 共享辅助函数：`static inline` 在单个头文件中（非新增 .c 文件）

**Research Findings** (5 个探索代理 + Metis 评审):
- 3 个有风险的 Bug：UDS_PACKED MSVC、uds_get_time_ms 非 POSIX、静默注册失败
- 7 个文件中的代码重复 (~322 行可消除)：set_neg_rsp (7×)、set_pos_rsp (7×)、should_suppress (6×)
- 两种不兼容的 set_neg_rsp 签名（3 参数 vs 4 参数）
- UDS_MEM_REGION_MAX 冲突（8 vs 4）— 静默的包含顺序 Bug
- _Static_assert 零处使用 — 关键布局假设未验证
- 5 个编译器警告被抑制 — 掩盖实际 Bug
- 零 CI/CD — 无自动化测试门禁

### Metis Review
**Identified Gaps** (addressed):
- 缺少单句核心目标 → **已修复**（草稿第 4 行）
- `uds_sid_to_response_sid` 溢出 → **已纳入 Bug 修复波次**
- 会话位映射 UB（current_session == 0）→ **已纳入 Bug 修复波次**
- `uds_parse_request` NULL 检查顺序错误 → **已纳入 Bug 修复波次**
- `should_suppress` 在 upload.c 中缺失 → **作为例外处理**

---

## Work Objectives

### Core Objective
将 UDS-Learning C 代码库中的 20+ 项代码质量、正确性和基础设施问题整合为依赖有序的分波次执行计划，消除约 322 行重复代码，修复 3 个潜在 Bug，添加 CI/CD 和编译期安全检查。

### Concrete Deliverables
- `inc/uds/uds_core.h` — 修复的 PACKED 宏（PACKED_BEGIN/PACKED_END 双宏）+ _Static_assert
- `inc/uds/uds_cfg.h` — 新建统一配置头文件，解决 UDS_MEM_REGION_MAX 冲突
- `inc/uds/uds_svc_util.h` — 新建共享 `static inline` 辅助函数
- `README.md` — 项目根目录说明文档
- `.github/workflows/ci.yml` — CI/CD 流水线
- `src/uds/uds_svc_*.c` (7 文件) — 替换每个文件的本地副本为共享头文件
- `src/uds/uds_core.c` — 修复 NULL 检查顺序 + `uds_sid_to_response_sid` 溢出
- `src/uds/uds_service.c` — 修复 `uds_get_time_ms` + `g_nrc_byte` + 注册 void cast

### Definition of Done
- [ ] `cmake -B build -DUDS_ASAN=ON -DUDS_UBSAN=ON && cmake --build build -- -j$(nproc) 2>&1 | grep -c "error:"` 返回 0
- [ ] `grep -c 'static.*set_neg_rsp' src/uds/*.c` 返回 0（无按文件本地定义）
- [ ] `grep -c '(void)uds_service_register' src/uds/uds_service.c` 返回 0
- [ ] `grep -c 'UDS_MEM_REGION_MAX' inc/` 返回恰好 1 个文件
- [ ] `.github/workflows/ci.yml` 存在且通过

### Must Have
- UDS_PACKED 双宏方法（PACKED_BEGIN / PACKED_END）
- 共享辅助函数作为 `static inline` 在单个头文件中（非新增 .c 文件）
- 3 参数 set_neg_rsp → 4 参数单行包装器
- UDS_MEM_REGION_MAX 在 `uds_cfg.h` 中的单一权威定义
- TDD 流程：先写测试（RED），验证通过（GREEN），提交（COMMIT）

### Must NOT Have (Guardrails)
- **NO** IMLOIF 检查提取（本次不处理）
- **NO** MISRA 合规性追求
- **NO** 在提取过程中重构无关代码
- **NO** 向未修改的模块添加测试
- **NO** Fuzz CI 集成（仅确保 fuzz 目标保持可编译）
- **NO** 新的 .c 编译单元用于共享辅助函数

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — 所有验证均由代理执行。不接受需要人工手动测试/确认的验收标准。

### Test Decision
- **Infrastructure exists**: YES（Unity + CMake CTest + 14 个测试文件 + mock 框架）
- **Automated tests**: TDD（RED-GREEN-REFACTOR）
- **Framework**: Unity（vendored 于 `local_unity/`）
- **每个代码修改任务** 遵循：RED（失败测试）→ GREEN（最小实现）→ REFACTOR → 原子提交

### QA Policy
每个任务必须包含代理可执行的 QA 场景（见下方 TODO 模板）。
证据保存至 `.omo/evidence/task-{N}-{scenario-slug}.{ext}`。

- **CLI/构建**: 使用 Bash — 运行 cmake/ctest，验证退出码和输出
- **代码分析**: 使用 Bash (grep) — 验证模式存在/不存在
- **API/后端**: 使用 Bash (ctest) — 运行单元测试，断言通过/失败

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Foundation — 8 tasks, MAX PARALLEL):
├── Task 1: 修复 UDS_PACKED 宏 [quick]
├── Task 2: 启用严格编译器警告 [unspecified-high]
├── Task 3: 添加 _Static_assert 编译期检查 [quick]
├── Task 4: 创建 uds_cfg.h + 解决 UDS_MEM_REGION_MAX 冲突 [quick]
├── Task 5: 添加项目 README.md [writing]
├── Task 6: 添加 CI/CD 工作流 + 清理过期文件 [quick]
├── Task 7: 修复 uds_get_time_ms() 平台兼容性 [quick]
└── Task 8: [TDD] 为共享辅助函数编写测试 (RED) [quick]

Wave 2 (Dedup — implementation + 7 replacements, 8 total):
├── Task 9: 创建 uds_svc_util.h (GREEN) [quick]
├── Task 10: 替换 uds_svc_diagcomm.c [quick]
├── Task 11: 替换 uds_svc_data.c [quick]
├── Task 12: 替换 uds_svc_io.c + 修复 3 参数包装器 [quick]
├── Task 13: 替换 uds_svc_routine.c [quick]
├── Task 14: 替换 uds_svc_stored.c [quick]
├── Task 15: 替换 uds_svc_upload.c [quick]
└── Task 16: 替换 uds_svc_auth.c + 修复 3 参数包装器 [quick]

Wave 3 (Bug Fixes — TDD, 5 parallel):
├── Task 17: [TDD] 修复 uds_parse_request NULL 检查顺序 [quick]
├── Task 18: [TDD] 添加会话位映射 UB 防护 [quick]
├── Task 19: [TDD] 修复 uds_sid_to_response_sid 溢出防护 [quick]
├── Task 20: [TDD] 修复 g_nrc_byte 全局单例 [quick]
└── Task 21: [TDD] 修复 void cast 静默服务注册失败 [quick]

Wave FINAL (4 parallel reviews → user okay):
├── Task F1: Plan Compliance Audit (oracle)
├── Task F2: Code Quality Review (unspecified-high)
├── Task F3: Real Manual QA (unspecified-high)
└── Task F4: Scope Fidelity Check (deep)
-> 呈现结果 → 获取用户明确 okay

Critical Path: T1 → T3 → T9 → T12+T16 → T17-T21 → F1-F4
Parallel Speedup: ~65% faster than sequential
Max Concurrent: 8 (Waves 1 & 2)
```

### Dependency Matrix

- **1-8**: — — 9-16, 17-21, F1-F4
- **9**: T8 — — 10-16
- **10-16**: T9 — — 17-21
- **17-21**: 10-16 — — F1-F4
- **F1-F4**: 17-21 — — (user okay)

### Agent Dispatch Summary

- **Wave 1**: **8 tasks** — T1,T3,T4,T6,T7,T8 → `quick`, T2 → `unspecified-high`, T5 → `writing`
- **Wave 2**: **8 tasks** — T9-T16 → `quick`
- **Wave 3**: **5 tasks** — T17-T21 → `quick`
- **Wave FINAL**: **4 tasks** — F1 → `oracle`, F2-F3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [x] 1. 修复 UDS_PACKED 宏 —— 双宏方法 (PACKED_BEGIN / PACKED_END)

  **What to do**:
  - 在 `inc/uds/uds_core.h` 中将 `#define UDS_PACKED __attribute__((packed))` 替换为双宏模式
  - 对于 MSVC：`#define UDS_PACKED_BEGIN __pragma(pack(push, 1))` / `#define UDS_PACKED_END __pragma(pack(pop))`
  - 对于 GCC/Clang：`#define UDS_PACKED_BEGIN _Pragma("pack(push, 1)")` / `#define UDS_PACKED_END _Pragma("pack(pop)")`
  - 更新 `uds_subfunction_t` 结构体使用新宏（唯一使用 UDS_PACKED 的结构体）
  - 保留向后兼容的别名：`#define UDS_PACKED UDS_PACKED_BEGIN`（弃用警告注释）

  **Must NOT do**:
  - 不要修改 UDS_PACKED 而不提供 pop（这会导致所有后续结构体意外打包）
  - 不要更改任何结构体布局
  - 不要为此创建新的 .c 文件

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - 原因：单个文件、单结构体修改，模式明确
  - **Skills**: 无（标准 C 预处理宏修改）

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1（与 T2-T7）
  - **Blocks**: T3（_Static_assert 依赖于稳定的宏）
  - **Blocked By**: None

  **References**:
  - `inc/uds/uds_core.h:20-29` — 当前的 UDS_PACKED 定义和编译器检测
  - `inc/uds/uds_core.h:185-188` — `uds_subfunction_t` 是唯一使用 UDS_PACKED 的结构体

  **Acceptance Criteria**:
  - [ ] `grep 'pack(pop)' inc/uds/uds_core.h` 匹配至少 1 个（MSVC）：`__pragma(pack(pop))`
  - [ ] `grep 'pack(pop)' inc/uds/uds_core.h` 匹配至少 1 个（GCC/Clang）：`_Pragma("pack(pop)")`
  - [ ] `grep -c 'UDS_PACKED_BEGIN' inc/uds/uds_core.h` 返回 ≥ 1
  - [ ] `cmake -B build && cmake --build build 2>&1 | grep -c "error:"` 返回 0

  **QA Scenarios**:

  ```
  Scenario: Happy path — 使用新宏编译
    Tool: Bash
    Preconditions: 工作目录干净，先前的构建已清理
    Steps:
      1. cmake -B build -DCMAKE_BUILD_TYPE=Debug
      2. cmake --build build 2>&1
      3. grep "error:" build_output → 0 个匹配
      4. grep "warning:" build_output → 0 个匹配
    Expected Result: 构建成功，零错误，零警告
    Failure Indicators: 任何编译器错误或关于打包不匹配的警告
    Evidence: .omo/evidence/task-1-build-output.txt

  Scenario: Error case — sizeof 在修复后保持不变
    Tool: Bash (编写并编译 sizeof 测试)
    Preconditions: sizeof 验证测试已编写
    Steps:
      1. 编译并运行验证 sizeof(uds_subfunction_t) == 1 的测试
      2. ctest --test-dir build -R sizeof -V
    Expected Result: sizeof == 1，测试通过
    Failure Indicators: sizeof 变化（位域布局损坏）或测试失败
    Evidence: .omo/evidence/task-1-sizeof-test.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-1-build-output.txt`
  - [ ] `task-1-sizeof-test.txt`

  **Commit**: YES
  - Message: `fix(uds_core): replace UDS_PACKED with PACKED_BEGIN/PACKED_END dual-macro`
  - Files: `inc/uds/uds_core.h`
  - Pre-commit: `cmake -B build && cmake --build build`

---

- [x] 2. 启用严格编译器警告并修复结果代码

  **What to do**:
  - 从 `CMakeLists.txt` 的 `add_uds_warnings()` 中移除 `-Wno-missing-prototypes` 和 `-Wno-conversion`
  - 尝试移除 `-Wno-sign-compare` 和 `-Wno-type-limits`（逐个处理；如果修复过多则保留并注释说明）
  - 修复移除 `-Wno-missing-prototypes` 后出现的所有警告（例如，向 `hal_nvm.h` 添加 `hal_nvm_init` 声明）
  - 修复移除 `-Wno-conversion` 后出现的所有警告（添加显式类型转换）
  - 添加 `-Wundef` 和 `-Wswitch-default` 到警告标志中
  - 如果任何警告标志导致过多的代码改动，保留 `-Wno-*` 并添加注释解释原因

  **Must NOT do**:
  - 不要更改行为逻辑 —— 只进行使警告静默所需的最小修改
  - 如果会导致大量变动，不要移除所有 5 个 `-Wno-*` —— 优先处理 missing-prototypes 和 conversion

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - 原因：需要仔细分析多个文件的警告修复，可能涉及多个编译单元
  - **Skills**: 无（标准 C 编译警告修复）

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1（与 T1、T3-T7）
  - **Blocks**: T17-T21（Bug 修复必须在干净构建下编译）
  - **Blocked By**: None

  **References**:
  - `CMakeLists.txt:35-49` — `add_uds_warnings` 宏定义
  - `CMakeLists.txt:44-48` — 当前被抑制的 5 个 `-Wno-*` 标志
  - `inc/hal/hal_nvm.h` — 缺少 `hal_nvm_init` 声明（被 `-Wno-missing-prototypes` 隐藏）

  **Acceptance Criteria**:
  - [ ] `grep -c '\-Wno-missing-prototypes' CMakeLists.txt` 返回 0
  - [ ] `grep -c '\-Wno-conversion' CMakeLists.txt` 返回 0
  - [ ] `grep 'hal_nvm_init' inc/hal/hal_nvm.h` 返回 ≥ 1（声明已添加）
  - [ ] `cmake -B build && cmake --build build 2>&1 | grep -c "error:"` 返回 0
  - [ ] `ctest --test-dir build --output-on-failure` 全部通过

  **QA Scenarios**:

  ```
  Scenario: Happy path — 使用重新启用的警告构建
    Tool: Bash
    Preconditions: 干净的构建目录
    Steps:
      1. cmake -B build -DCMAKE_BUILD_TYPE=Debug
      2. cmake --build build 2>&1 | tee build.log
      3. grep -c "error:" build.log → 0
      4. grep -c "warning:" build.log → 0
    Expected Result: 构建成功，零错误，零警告
    Failure Indicators: 任何错误或新警告
    Evidence: .omo/evidence/task-2-build-clean.log

  Scenario: Error case — 所有测试在启用警告后仍然通过
    Tool: Bash
    Preconditions: 构建成功
    Steps:
      1. ctest --test-dir build --output-on-failure 2>&1
      2. 验证所有测试：100% 测试通过
    Expected Result: 所有测试通过
    Failure Indicators: 任何测试失败（警告修复破坏了行为）
    Evidence: .omo/evidence/task-2-ctest-output.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-2-build-clean.log`
  - [ ] `task-2-ctest-output.txt`

  **Commit**: YES
  - Message: `build: re-enable -Wmissing-prototypes and -Wconversion warnings`
  - Files: `CMakeLists.txt`, `inc/hal/hal_nvm.h`（加上所有修改以修复警告的文件）
  - Pre-commit: `cmake -B build && cmake --build build && ctest --test-dir build`

---

- [x] 3. 添加 _Static_assert 编译期布局检查

  **What to do**:
  - 在 `inc/uds/uds_core.h` 的 `uds_subfunction_t` 之后添加：`_Static_assert(sizeof(uds_subfunction_t) == 1, "subfunction must be 1 byte")`
  - 在 `inc/uds/uds_lin_transport.h` 的 `LIN_FRAME_SIZE` 之后添加：`_Static_assert(LIN_FRAME_SIZE == 8, "LIN frame must be 8 bytes")`
  - 添加 `_Static_assert` 验证 `suppress_rsp` 位域映射到第 7 位（检查结构体偏移）
  - 确保所有 `_Static_assert` 在 C11 模式和 C++ 下都能工作（`#ifdef __cplusplus` 使用 `static_assert`）

  **Must NOT do**:
  - 不要添加验证业务逻辑的 `_Static_assert` —— 仅验证编译期布局/大小

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - 原因：单文件，少量静态断言添加
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1（与 T1-T2、T4-T7）
  - **Blocks**: None（但必须在任何结构体修改前完成）
  - **Blocked By**: T1（PACKED 宏必须稳定）

  **References**:
  - `inc/uds/uds_core.h:185-188` — `uds_subfunction_t` 位域结构体
  - `inc/uds/uds_lin_transport.h:24` — `#define LIN_FRAME_SIZE 8`

  **Acceptance Criteria**:
  - [ ] `grep -c '_Static_assert\|static_assert' inc/uds/uds_core.h` 返回 ≥ 1
  - [ ] `grep -c '_Static_assert\|static_assert' inc/uds/uds_lin_transport.h` 返回 ≥ 1
  - [ ] `cmake -B build && cmake --build build 2>&1 | grep -c "error:"` 返回 0
  - [ ] `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build 2>&1 | grep -c "error:"` 返回 0

  **QA Scenarios**:

  ```
  Scenario: Happy path — 所有静态断言在编译时通过
    Tool: Bash
    Preconditions: 干净的构建
    Steps:
      1. cmake -B build -DCMAKE_BUILD_TYPE=Debug
      2. cmake --build build 2>&1
    Expected Result: 构建成功，零错误
    Failure Indicators: 任何因 _Static_assert 失败导致的错误
    Evidence: .omo/evidence/task-3-build-pass.txt

  Scenario: Error case — 故意触发失败（验证断言是有效的）
    Tool: Bash（临时修改后还原）
    Preconditions: 无
    Steps:
      1. grep '_Static_assert' inc/uds/uds_core.h inc/uds/uds_lin_transport.h
      2. 验证每个断言都检查了一个有意义的条件
    Expected Result: 所有 _Static_assert 语句都检查编译期不变量
    Evidence: .omo/evidence/task-3-asserts-list.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-3-build-pass.txt`
  - [ ] `task-3-asserts-list.txt`

  **Commit**: YES
  - Message: `feat(uds_core): add _Static_assert for critical layout assumptions`
  - Files: `inc/uds/uds_core.h`, `inc/uds/uds_lin_transport.h`
  - Pre-commit: `cmake -B build && cmake --build build`

---

- [x] 4. 创建 uds_cfg.h 并解决 UDS_MEM_REGION_MAX 冲突

  **What to do**:
  - 创建 `inc/uds/uds_cfg.h` 作为统一配置头文件
  - 将 `UDS_MEM_REGION_MAX` 从 `uds_svc_data.h` (定义为 8) 和 `uds_svc_upload.h` (定义为 4) 移至 `uds_cfg.h`
  - 使用单一权威定义：`#define UDS_MEM_REGION_MAX 8u`
  - 移除 `uds_svc_data.h` 和 `uds_svc_upload.h` 中的重复定义
  - 两个头文件都 `#include "uds/uds_cfg.h"`

  **Must NOT do**:
  - 不要更改 UDS_MEM_REGION_MAX 的值（仅统一定义位置）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 无

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1（与 T1-T3、T5-T7）
  - **Blocks**: None / **Blocked By**: None

  **References**: `inc/uds/uds_svc_data.h:59`, `inc/uds/uds_svc_upload.h:49`

  **Acceptance Criteria**:
  - [ ] `test -f inc/uds/uds_cfg.h` 返回 0
  - [ ] `grep -c 'UDS_MEM_REGION_MAX' inc/` 返回恰好 1 个文件
  - [ ] `cmake -B build && cmake --build build 2>&1 | grep -c "error:"` 返回 0

  **QA Scenarios**:

  ```
  Scenario: Happy path — 仅一个定义，构建通过
    Tool: Bash
    Steps: cmake -B build && cmake --build build 2>&1; grep -rn 'UDS_MEM_REGION_MAX' inc/
    Expected Result: 构建成功；恰好 1 个文件包含此定义
    Evidence: .omo/evidence/task-4-build.txt
  ```

  **Evidence**: `task-4-build.txt`

  **Commit**: YES — `refactor(config): create uds_cfg.h and resolve UDS_MEM_REGION_MAX conflict` — `inc/uds/uds_cfg.h` (new), `inc/uds/uds_svc_data.h`, `inc/uds/uds_svc_upload.h`

---

- [x] 5. 添加项目根目录 README.md

  **What to do**: 创建 `README.md`，包含项目描述、构建说明（`cmake -B build`）、测试说明（`ctest`）、架构概览、doc 链接
  **Must NOT do**: 不使用表情符号；不创建其他文档文件
  **Recommended Agent Profile**: `writing`
  **Parallelization**: Wave 1（与 T1-T4、T6-T7）| Blocks: None | Blocked By: None
  **References**: `doc/README.md`, `CMakeLists.txt`

  **Acceptance Criteria**:
  - [ ] `test -f README.md`; `grep -c 'cmake -B build' README.md` ≥ 1; `grep -c 'ctest' README.md` ≥ 1

  **QA Scenarios**:

  ```
  Scenario: README 包含所有必需部分
    Tool: Bash (grep)
    Steps: grep 'cmake -B build' README.md && grep 'ctest' README.md
    Expected: 两个匹配都存在
    Evidence: .omo/evidence/task-5-readme-check.txt
  ```

  **Evidence**: `task-5-readme-check.txt`
  **Commit**: YES — `docs: add project README` — `README.md` (new)

---

- [x] 6. 添加 CI/CD 工作流并清理过期文件

  **What to do**: 创建 `.github/workflows/ci.yml`（ubuntu-latest, checkout → cmake build → ctest → asan build → ctest）；删除 `test.c`、`src/hal/.gitkeep`；将 `CMakeUserPresets.json` 添加到 `.gitignore`
  **Must NOT do**: 不删除有实质内容的文件；CI 中不添加 fuzz/覆盖率
  **Recommended Agent Profile**: `quick`
  **Parallelization**: Wave 1（与 T1-T5、T7）| Blocks: None | Blocked By: None

  **Acceptance Criteria**:
  - [ ] `test -f .github/workflows/ci.yml`; `test -f test.c` 返回 1; `grep 'CMakeUserPresets.json' .gitignore` ≥ 1

  **QA Scenarios**:

  ```
  Scenario: CI 存在，过期文件已删除
    Tool: Bash
    Steps: ls .github/workflows/ci.yml && ! ls test.c
    Expected: CI 存在；test.c 已删除
    Evidence: .omo/evidence/task-6-cleanup-verify.txt
  ```

  **Evidence**: `task-6-cleanup-verify.txt`
  **Commit**: YES — `ci: add GitHub Actions workflow and clean stale files` — `.github/workflows/ci.yml` (new), `.gitignore`, 删除 `test.c`, `src/hal/.gitkeep`

---

- [x] 7. 修复 uds_get_time_ms() 非 POSIX 平台兼容性

  **What to do**: `src/uds/uds_service.c` 的 `uds_get_time_ms()` 添加 `#ifdef _POSIX_C_SOURCE` / `#else` return 0
  **Must NOT do**: 不更改 POSIX 路径的逻辑
  **Recommended Agent Profile**: `quick`
  **Parallelization**: Wave 1（与 T1-T6）| Blocks: None | Blocked By: None
  **References**: `src/uds/uds_service.c:49-55`

  **Acceptance Criteria**:
  - [ ] `grep -c '#ifdef.*POSIX\|#else' src/uds/uds_service.c`（在 `uds_get_time_ms` 附近）≥ 2
  - [ ] `cmake -B build && cmake --build build 2>&1 | grep -c "error:"` 返回 0

  **QA Scenarios**:

  ```
  Scenario: POSIX 构建有效
    Tool: Bash
    Steps: cmake -B build && cmake --build build 2>&1; ctest --test-dir build -R service
    Expected: 构建并测试通过
    Evidence: .omo/evidence/task-7-posix-build.txt
  ```

  **Evidence**: `task-7-posix-build.txt`
  **Commit**: YES — `fix(uds_service): add #ifdef guard for uds_get_time_ms` — `src/uds/uds_service.c`

---

- [x] 8. [TDD] 为共享响应辅助函数编写测试

  **What to do**:
  - **RED**: 为新的共享 `uds_set_neg_rsp()`（4 参数版本 + 3 参数包装器）、`uds_set_pos_rsp()`、`uds_should_suppress()` 编写 Unity 测试
  - 测试覆盖：空响应结构体验证、负响应格式 [0x7F][SID][NRC]、正响应格式、suppress_rsp = 0 和 = 1、与现有 `uds_serialize_response` / `uds_serialize_negative_response` 的一致性
  - 测试文件：`test/test_svc_util.c`
  - 使用 `add_uds_test(svc-util test/test_svc_util.c)` 注册到 CMakeLists.txt
  - **GREEN**: 当前尚无实现，测试应该编译但链接失败或运行时失败

  **Must NOT do**: 不要测试 IMLOIF 模式或特定于服务的行为
  **Recommended Agent Profile**: `quick` | **Parallelization**: Wave 1（与 T1-T7）| Blocks: T9-T16 | Blocked By: None

  **Acceptance Criteria**:
  - [ ] `test -f test/test_svc_util.c`；`grep 'add_uds_test(svc-util' CMakeLists.txt` ≥ 1
  - [ ] `cmake -B build && cmake --build build` 链接失败（预期 RED 阶段）

  **QA Scenarios**:

  ```
  Scenario: RED — 测试编译但链接失败（暂无实现）
    Tool: Bash
    Steps: cmake -B build && cmake --build build 2>&1
    Expected: 构建失败，出现 undefined reference to `uds_set_neg_rsp` 等
    Evidence: .omo/evidence/task-8-red-build.txt
  ```

  **Evidence**: `task-8-red-build.txt`
  **Commit**: YES — `test(svc_util): add TDD tests for shared response helpers (RED)` — `test/test_svc_util.c` (new), `CMakeLists.txt`

---

- [x] 9. 创建 uds_svc_util.h 共享辅助函数（GREEN）

  **What to do**:
  - 创建 `inc/uds/uds_svc_util.h`，包含 `static inline` 实现：
    - `uds_set_neg_rsp(rsp, req_sid, nrc, buf)` — 4 参数版本（主版本）
    - `uds_set_pos_rsp(rsp, rsp_sid, subfunc, data, data_len)` — 4 参数版本
    - `uds_should_suppress(req)` — 返回 `req->subfunction.suppress_rsp`
  - 确保所有函数都是 `static inline`（非新编译单元）
  - 确保 `#pragma once` 头文件守卫
  - **GREEN**: 测试现在应该通过
  - 更新 CMakeLists.txt 中所有使用 uds_svc_util 的目标的 include 路径

  **Must NOT do**: 不要创建新的 .c 文件；不要包含 3 参数 set_neg_rsp（由每个模块本地提供）
  **Recommended Agent Profile**: `quick` | **Parallelization**: Wave 2 | Blocks: T10-T16 | Blocked By: T8

  **Acceptance Criteria**:
  - [ ] `test -f inc/uds/uds_svc_util.h`；`grep 'static inline' inc/uds/uds_svc_util.h` ≥ 3
  - [ ] `cmake -B build && cmake --build build 2>&1 | grep -c "error:"` 返回 0
  - [ ] `ctest --test-dir build -R svc-util` 全部通过（GREEN）

  **QA Scenarios**:

  ```
  Scenario: GREEN — 所有测试通过
    Tool: Bash
    Steps: cmake -B build && cmake --build build && ctest --test-dir build -R svc-util -V
    Expected: 所有 svc-util 测试通过，零失败
    Evidence: .omo/evidence/task-9-green-test.txt
  ```

  **Evidence**: `task-9-green-test.txt`
  **Commit**: YES — `feat(svc_util): add shared static inline response helpers (GREEN)` — `inc/uds/uds_svc_util.h` (new)

---

- [x] 10. 替换 uds_svc_diagcomm.c 中的本地副本

  **What to do**: 删除 `set_neg_rsp`、`set_pos_rsp`、`should_suppress` 的本地定义（行 48-57、68-75、87-90）；添加 `#include "uds/uds_svc_util.h"`；将所有 `set_neg_rsp(rsp, req->sid, NRC, buf)` 调用替换为 `uds_set_neg_rsp(rsp, req->sid, NRC, buf)` 等；验证 ctest 通过
  **Must NOT do**: 不接触任何处理函数逻辑
  **Agent**: `quick` | **Parallel**: Wave 2 与 T11-T16 | **Blocks**: None | **Blocked By**: T9

  **Acceptance Criteria**: `grep -c 'static.*set_neg_rsp\|static.*set_pos_rsp\|static.*should_suppress' src/uds/uds_svc_diagcomm.c` 返回 0；ctest -R diagcomm 通过

  **QA**:

  ```
  Scenario: 去重后 diagcomm 测试通过
    Tool: Bash; Steps: cmake --build build && ctest --test-dir build -R diagcomm -V
    Expected: 所有 diagcomm 测试通过; Evidence: .omo/evidence/task-10-dedup-pass.txt
  ```

  **Evidence**: `task-10-dedup-pass.txt`
  **Commit**: YES — `refactor(diagcomm): use shared response helpers from uds_svc_util.h` — `src/uds/uds_svc_diagcomm.c`

---

- [x] 11. 替换 uds_svc_data.c 中的本地副本

  **What to do**: 与 T10 相同的模式 —— 删除 `set_neg_rsp` (66-73)、`set_pos_rsp` (86-93)、`should_suppress` (101-104)；添加 `#include "uds/uds_svc_util.h"`；重命名调用
  **Agent**: `quick` | **Parallel**: Wave 2 | **Blocked By**: T9

  **Acceptance Criteria**: `grep -c 'static.*set_neg_rsp\|static.*set_pos_rsp\|static.*should_suppress' src/uds/uds_svc_data.c` 返回 0；ctest -R data-service 通过

  **QA**:

  ```
  Scenario: 去重后 data-service 测试通过
    Tool: Bash; Steps: cmake --build build && ctest --test-dir build -R data-service -V
    Expected: 所有 data-service 测试通过; Evidence: .omo/evidence/task-11-dedup-pass.txt
  ```

  **Evidence**: `task-11-dedup-pass.txt`
  **Commit**: YES — `refactor(svc_data): use shared response helpers` — `src/uds/uds_svc_data.c`

---

- [x] 12. 替换 uds_svc_io.c 中的本地副本 + 修复 3 参数包装器

  **What to do**: 删除 `set_neg_rsp` (68-75)、`set_pos_rsp` (86-93)、`should_suppress` (102-105)；添加 `#include "uds/uds_svc_util.h"`；对于 3 参数调用 —— 创建本地内联包装器：`static void set_neg_rsp_local(rsp, sid, nrc) { uds_set_neg_rsp(rsp, sid, nrc, g_rsp_buf); }`；此任务修复 3 参数签名差异（使用模块的 `g_rsp_buf[256]`）
  **Agent**: `quick` | **Parallel**: Wave 2 | **Blocked By**: T9

  **Acceptance Criteria**: `grep -c 'static.*set_neg_rsp' src/uds/uds_svc_io.c` ≤ 1（仅本地包装器）；ctest -R io 通过

  **QA**:

  ```
  Scenario: 去重后 IO 测试通过，包括 3 参数调用
    Tool: Bash; Steps: ctest --test-dir build -R io -V
    Expected: 所有 IO 测试通过; Evidence: .omo/evidence/task-12-dedup-pass.txt
  ```

  **Evidence**: `task-12-dedup-pass.txt`
  **Commit**: YES — `refactor(svc_io): use shared helpers + local 3-arg wrapper` — `src/uds/uds_svc_io.c`

---

- [x] 13. 替换 uds_svc_routine.c 中的本地副本

  **What to do**: 删除 `set_neg_rsp` (48-56)、`set_pos_rsp` (67-74)、`should_suppress` (82-85)；添加 `#include "uds/uds_svc_util.h"`；重命名调用
  **Agent**: `quick` | **Parallel**: Wave 2 | **Blocked By**: T9
  **Acceptance**: grep 返回 0；ctest -R routine 通过

  **QA**:

  ```
  Scenario: 去重后 routine 测试通过
    Tool: Bash; Steps: ctest --test-dir build -R routine -V
    Evidence: .omo/evidence/task-13-dedup-pass.txt
  ```

  **Evidence**: `task-13-dedup-pass.txt`
  **Commit**: YES — `refactor(svc_routine): use shared response helpers` — `src/uds/uds_svc_routine.c`

---

- [x] 14. 替换 uds_svc_stored.c 中的本地副本

  **What to do**: 删除 `set_neg_rsp` (37-44)、`set_pos_rsp` (55-62)、`should_suppress` (69-72)；添加 `#include "uds/uds_svc_util.h"`；重命名调用
  **Agent**: `quick` | **Parallel**: Wave 2 | **Blocked By**: T9
  **Acceptance**: grep 返回 0；ctest -R stored 通过

  **QA**:

  ```
  Scenario: 去重后 stored 测试通过
    Tool: Bash; Steps: ctest --test-dir build -R stored -V
    Evidence: .omo/evidence/task-14-dedup-pass.txt
  ```

  **Evidence**: `task-14-dedup-pass.txt`
  **Commit**: YES — `refactor(svc_stored): use shared response helpers` — `src/uds/uds_svc_stored.c`

---

- [x] 15. 替换 uds_svc_upload.c 中的本地副本（should_suppress 处理不同）

  **What to do**: 删除 `set_neg_rsp` (59-66)、`set_pos_rsp` (80-87)；添加 `#include "uds/uds_svc_util.h"`；注意：upload.c 没有 `should_suppress` —— 使用内联 `uds_should_suppress(req)` 替换所有现有的抑制检查；删除重复的 `UDS_REQ_BYTE1` 定义（147-150，已由 uds_svc_data.h 提供）
  **Agent**: `quick` | **Parallel**: Wave 2 | **Blocked By**: T9
  **Acceptance**: grep 返回 0；`grep 'UDS_REQ_BYTE1' src/uds/uds_svc_upload.c` ≤ 1（仅使用，非定义）；ctest -R upload 通过

  **QA**:

  ```
  Scenario: 去重后 upload 测试通过
    Tool: Bash; Steps: ctest --test-dir build -R upload -V
    Evidence: .omo/evidence/task-15-dedup-pass.txt
  ```

  **Evidence**: `task-15-dedup-pass.txt`
  **Commit**: YES — `refactor(svc_upload): use shared helpers + remove duplicate UDS_REQ_BYTE1` — `src/uds/uds_svc_upload.c`

---

- [x] 16. 替换 uds_svc_auth.c 中的本地副本 + 修复 3 参数包装器

  **What to do**: 删除 `set_neg_rsp` (95-103)、`set_pos_rsp` (113-120)、`should_suppress` (131-134)；添加 `#include "uds/uds_svc_util.h"`；创建本地 3 参数包装器使用 `g_rsp_data_buf`
  **Agent**: `quick` | **Parallel**: Wave 2 | **Blocked By**: T9
  **Acceptance**: grep 返回 ≤ 1（包装器）；ctest -R auth 通过

  **QA**:

  ```
  Scenario: 去重后 auth 测试通过
    Tool: Bash; Steps: ctest --test-dir build -R auth -V
    Evidence: .omo/evidence/task-16-dedup-pass.txt
  ```

  **Evidence**: `task-16-dedup-pass.txt`
  **Commit**: YES — `refactor(svc_auth): use shared helpers + local 3-arg wrapper` — `src/uds/uds_svc_auth.c`

---

- [x] 17. [TDD] 修复 uds_parse_request NULL 检查顺序

  **What to do**:
  - **RED**: 在 `test/test_core.c` 中添加测试 —— `test_parse_null_raw_with_valid_len`：使用 `raw=NULL, len=5, req=valid`，期望 `UDS_ERR_PARSE`（当前由于检查顺序错误返回 `UDS_ERR_TOO_SHORT`）
  - **GREEN**: 在 `src/uds/uds_core.c:30-46` 中将 `raw == NULL` 检查移到 `len == 0` 检查之前
  - **REFACTOR**: 验证所有现有测试仍然通过

  **Agent**: `quick` | **Parallel**: Wave 3 与 T18-T21 | **Blocked By**: T10-T16

  **Acceptance Criteria**:
  - [ ] `test_parse_null_raw_with_valid_len` 已添加到 `test_core.c`
  - [ ] `grep 'if (raw == NULL)' src/uds/uds_core.c` 出现在 `if (len == 0)` 之前
  - [ ] ctest -R core 全部通过

  **QA**:

  ```
  Scenario: 修复后 NULL raw 被正确检测
    Tool: Bash
    Steps: ctest --test-dir build -R core -V
    Expected: 所有核心测试通过，包括新的 NULL 顺序测试
    Evidence: .omo/evidence/task-17-fix-pass.txt
  ```

  **Evidence**: `task-17-fix-pass.txt`
  **Commit**: YES — `fix(uds_core): reorder NULL-check in uds_parse_request` — `src/uds/uds_core.c`, `test/test_core.c`

---

- [x] 18. [TDD] 添加会话位映射 UB 防护

  **What to do**:
  - **RED**: 在 `test/test_svc_service.c` 中添加测试 —— `test_dispatch_invalid_session_zero`：使用 `current_session=0` 调用 `uds_service_dispatch`，期望 NRC 0x7F 或返回 false
  - **GREEN**: 在 `src/uds/uds_service.c:192` 中添加防护：`if (current_session == 0 || current_session > 3) { rsp->sid = 0x7F; ... NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION; return true; }`
  - **REFACTOR**: 验证所有现有测试通过

  **Agent**: `quick` | **Parallel**: Wave 3 | **Blocked By**: T10-T16

  **Acceptance Criteria**:
  - [ ] `test_dispatch_invalid_session_zero` 在 test_svc_service.c 中
  - [ ] `grep 'current_session == 0\|current_session > 3' src/uds/uds_service.c` ≥ 1
  - [ ] ctest -R service 全部通过

  **QA**:

  ```
  Scenario: 无效会话 0 被正确拒绝
    Tool: Bash
    Steps: ctest --test-dir build -R service -V
    Expected: 所有 service 测试通过
    Evidence: .omo/evidence/task-18-fix-pass.txt
  ```

  **Evidence**: `task-18-fix-pass.txt`
  **Commit**: YES — `fix(uds_service): guard against UB in session bit-mapping` — `src/uds/uds_service.c`, `test/test_svc_service.c`

---

- [x] 19. [TDD] 修复 uds_sid_to_response_sid 溢出防护

  **What to do**:
  - **RED**: 在 `test/test_core.c` 中添加测试 —— `test_sid_to_response_sid_rejects_invalid`：调用 `uds_sid_to_response_sid(0xC0)`（≥ 0xC0 会溢出），期望返回 0x00 或通过其他机制返回错误
  - **GREEN**: 在 `src/uds/uds_core.c:160-162` 中添加防护：`if (sid >= 0xC0) { return 0x00; }` 或使用 `IS_REQUEST_SID` 宏进行验证
  - **REFACTOR**: 验证现有测试仍然通过

  **Agent**: `quick` | **Parallel**: Wave 3 | **Blocked By**: T10-T16

  **Acceptance Criteria**:
  - [ ] `test_sid_to_response_sid_rejects_invalid` 在 test_core.c 中
  - [ ] `grep '0xC0\|IS_REQUEST_SID' src/uds/uds_core.c`（在 `uds_sid_to_response_sid` 中）≥ 1
  - [ ] ctest -R core 全部通过

  **QA**:

  ```
  Scenario: 无效 SID 不导致溢出
    Tool: Bash
    Steps: ctest --test-dir build -R core -V
    Expected: 所有核心测试通过
    Evidence: .omo/evidence/task-19-fix-pass.txt
  ```

  **Evidence**: `task-19-fix-pass.txt`
  **Commit**: YES — `fix(uds_core): prevent overflow in uds_sid_to_response_sid` — `src/uds/uds_core.c`, `test/test_core.c`

---

- [x] 20. [TDD] 修复 g_nrc_byte 全局单例 —— 使用本地缓冲区

  **What to do**:
  - **RED**: 在 `test/test_svc_service.c` 中添加测试 —— 验证 `uds_service_dispatch` 为不支持的服务返回正确的 NRC（0x11），并且第二次调度不会因共享的 `g_nrc_byte` 而损坏
  - **GREEN**: 在 `uds_service_dispatch_ex()` 中将 `static uint8_t g_nrc_byte` 替换为栈上局部变量 `uint8_t nrc_buf`
  - **REFACTOR**: 验证所有 dispatch 和 service 测试通过

  **Agent**: `quick` | **Parallel**: Wave 3 | **Blocked By**: T10-T16

  **Acceptance Criteria**:
  - [ ] `grep -c 'static uint8_t g_nrc_byte' src/uds/uds_service.c` 返回 0
  - [ ] `grep 'nrc_buf' src/uds/uds_service.c` ≥ 1（局部变量）
  - [ ] ctest -R service 全部通过

  **QA**:

  ```
  Scenario: NRC 字节在调度之间隔离
    Tool: Bash
    Steps: ctest --test-dir build -R service -V
    Expected: 所有 service 测试通过，无竞态
    Evidence: .omo/evidence/task-20-fix-pass.txt
  ```

  **Evidence**: `task-20-fix-pass.txt`
  **Commit**: YES — `fix(uds_service): replace global g_nrc_byte with local buffer` — `src/uds/uds_service.c`, `test/test_svc_service.c`

---

- [x] 21. [TDD] 修复 void cast 静默服务注册失败

  **What to do**:
  - **RED**: 编写测试验证：当调度表已满时，`uds_service_register` 返回 false；使用 `uds_service_init()` 注册 26 个服务后，检查计数 ≤ UDS_SERVICE_TABLE_MAX
  - **GREEN**: 在 `uds_service_init()` 中，将 `(void)uds_service_register(...)` 替换为检查返回值的代码；如果注册失败，记录错误（stderr）或使用 `assert()` 终止
  - 同时修复 `uds_svc_routine.c` 中的 `(void)uds_svc_routine_register(...)` 调用
  - **REFACTOR**: 验证 `uds_service_init()` 后 `uds_service_get_count()` 返回 26

  **Agent**: `quick` | **Parallel**: Wave 3 | **Blocked By**: T10-T16

  **Acceptance Criteria**:
  - [ ] `grep -c '(void)uds_service_register' src/uds/uds_service.c` 返回 0
  - [ ] `grep -c '(void)uds_svc_routine_register' src/uds/uds_svc_routine.c` 返回 0
  - [ ] ctest 全部通过

  **QA**:

  ```
  Scenario: 所有 26 个服务注册成功
    Tool: Bash
    Steps: ctest --test-dir build --output-on-failure -V
    Expected: 所有测试通过，udt_service_get_count() == 26
    Evidence: .omo/evidence/task-21-fix-pass.txt
  ```

  **Evidence**: `task-21-fix-pass.txt`
  **Commit**: YES — `fix(uds_service): check service registration return values` — `src/uds/uds_service.c`, `src/uds/uds_svc_routine.c`, `test/test_svc_service.c`

---

## Final Verification Wave

> 4 个审查代理并行运行。所有都必须 APPROVE。向用户呈现综合结果并获取明确的 "okay"。

- [x] F1. **Plan Compliance Audit** — `oracle` (APPROVED after fixes)
- [x] F2. **Code Quality Review** — `unspecified-high` (APPROVED after ASAN fix)
- [x] F3. **Real Manual QA** — `unspecified-high` (APPROVED — 17/17 ASAN pass)
- [x] F4. **Scope Fidelity Check** — `deep` (APPROVED WITH DEFECTS → resolved)
  对于每个任务：阅读 "What to do"，阅读实际差异（git log/diff）。验证 1:1。检查 "Must NOT do" 合规性。检测跨任务污染。标记未计入的更改。
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N] | Unaccounted [CLEAN/N] | VERDICT`

---

## Commit Strategy

| Task | Message | Files |
|------|---------|-------|
| 1 | `fix(uds_core): replace UDS_PACKED with PACKED_BEGIN/PACKED_END` | `inc/uds/uds_core.h` |
| 2 | `build: re-enable -Wmissing-prototypes and -Wconversion` | `CMakeLists.txt`, `inc/hal/hal_nvm.h` |
| 3 | `feat(uds_core): add _Static_assert for critical layout` | `inc/uds/uds_core.h`, `inc/uds/uds_lin_transport.h` |
| 4 | `refactor(config): create uds_cfg.h, resolve MEM_REGION_MAX` | `inc/uds/uds_cfg.h` (new), svc_data/upload .h |
| 5 | `docs: add project README` | `README.md` (new) |
| 6 | `ci: add GitHub Actions workflow, clean stale files` | `.github/workflows/ci.yml` (new), `.gitignore`, deletes |
| 7 | `fix(uds_service): add #ifdef guard for uds_get_time_ms` | `src/uds/uds_service.c` |
| 8 | `test(svc_util): add TDD tests for shared helpers (RED)` | `test/test_svc_util.c` (new), `CMakeLists.txt` |
| 9 | `feat(svc_util): add shared inline response helpers (GREEN)` | `inc/uds/uds_svc_util.h` (new) |
| 10-16 | `refactor(svc_*): use shared response helpers` | `src/uds/uds_svc_*.c` (7 files) |
| 17 | `fix(uds_core): reorder NULL-check in uds_parse_request` | `src/uds/uds_core.c`, `test/test_core.c` |
| 18 | `fix(uds_service): guard against UB in session bit-mapping` | `src/uds/uds_service.c`, `test/test_svc_service.c` |
| 19 | `fix(uds_core): prevent overflow in uds_sid_to_response_sid` | `src/uds/uds_core.c`, `test/test_core.c` |
| 20 | `fix(uds_service): replace global g_nrc_byte with local buffer` | `src/uds/uds_service.c`, `test/test_svc_service.c` |
| 21 | `fix(uds_service): check service registration return values` | `src/uds/uds_service.c`, `test/test_svc_service.c` |

---

## Success Criteria

### Verification Commands
```bash
# Gate 1: Build + test (must pass after ALL changes)
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build

# Gate 2: ASan + UBSan
cmake -B build-asan -DUDS_ASAN=ON && cmake --build build-asan && ctest --test-dir build-asan

# Gate 3: Dedup verification
grep -rn 'static.*set_neg_rsp' src/uds/uds_svc_*.c  # expect: ≤ 2 files (io/auth wrappers only)
grep -c '(void)uds_service_register' src/uds/uds_service.c  # expect: 0
grep -c 'UDS_MEM_REGION_MAX' inc/uds/uds_svc_data.h inc/uds/uds_svc_upload.h  # expect: 0
```

### Final Checklist
- [ ] All "Must Have" present; All "Must NOT Have" absent
- [ ] All build configs (debug/release/asan) pass
- [ ] All ~300+ tests pass
- [ ] Dedup: local copies removed (≤ 2 wrapper-only definitions remain)
- [ ] CI/CD workflow file exists
- [ ] F1-F4 all APPROVE → user okay

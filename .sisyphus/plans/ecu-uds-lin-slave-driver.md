# ECU UDS-on-LIN Slave Driver Implementation

## TL;DR

> **Quick Summary**: Build a complete ECU slave (server) driver implementing ISO 14229-1:2020 UDS protocol stack over LIN bus (ISO 17987/ISO 14229-7, 19200 bps) in C with MCU platform abstraction, including all 26 diagnostic services, full DTC management, session/security control, and PC simulation test harness with TDD workflow.
>
> **Deliverables**:
> - UDS protocol stack source code (C) — all services + core engine
> - LIN transport layer adapter (ISO 17987-3 compliant)
> - MCU HAL abstraction layer (UART/Timer/NVM/Config)
> - Session management + Security access state machines
> - Full DTC management engine (8 status bits, snapshot, extended data)
> - PC simulation test harness (LIN master simulator + UDS tester)
> - CMake build system with TDD test suite
> - Both bare-metal & FreeRTOS adaptation hooks
>
> **Estimated Effort**: XL (20+ tasks, 21 engineering TODOs + 4 review TODOs)
> **Parallel Execution**: YES — 4 waves + final review wave
> **Critical Path**: Task 1 → 4 → 6 → 8 → 9 → 15 → 17 → F1-F4

---

## Context

### Original Request
> "ISO 14229-1-2020.txt 阅读该文件，制作ECU从机驱动代码，用LIN BUS传数据"

### Interview Summary
**Key Decisions**:
- **Language/Platform**: C language + generic MCU platform abstraction (HAL interface)
- **UDS Services**: Complete set — all 26 services from ISO 14229-1:2020
- **LIN Bus**: 19200 bps, LIN 2.x compatible
- **RTOS/Bare-metal**: Both — provide adaptation hooks
- **Deliverable**: Complete source code project (CMake)
- **DTC Management**: Full DTC management (8 status bits + snapshot + extended data)
- **Test Strategy**: TDD (test-first) with PC simulation test environment

**Research Findings**:
- ISO 14229-1:2020 defines application layer services with SID encoding (request SIDs 0x10-0x3E, response SIDs = request + 0x40, negative response = 0x7F + SID + NRC)
- SubFunction byte: bit 7 = suppressPosRspMsgIndicationBit, bits 6-0 = SubFunction value
- Server response behavior per Section 8.7 (Tables 4-7): physical vs functional addressing rules
- LIN diagnostic frames (ISO 17987-3): 8-byte frames, NAD + PCI + UDS payload
- DTC status byte: 8 status bits (testFailed, testFailedThisOpCycle, pendingDTC, confirmedDTC, testFailedSinceLastClear, testFailedSinceThisOpCycle, warningIndicator, testNotCompleted)
- NRC range: 0x00=positiveResponse, 0x01-0x7F=comm related, 0x80-0xFF=conditionsNotCorrect specifics

### Metis Review
**Identified Gaps** (addressed):
- **DID/RID model**: Default config file with placeholder DIDs/RIDs defined — user customizes per-project
- **LIN NAD**: Default 0x01 for slave ECU — configurable via header define
- **Authentication 0x29**: Framework with challenge-response stub (no PKI) + NRC 0x34 support
- **Programming memory model**: Simplified contiguous memory region placeholder
- **Test infrastructure**: CTest + Unity test framework for TDD workflow
- **Endianness**: Little-endian default with configurable macro overrides

### Guardrails Applied (from Metis)
1. Section 8.7 compliance — server response must match Tables 4-7 exactly (physical/functional/SPRMIB)
2. SecurityAccess must implement Annex I state machine (locked/unlocked, delay timer, attempt counter)
3. Session timing: P2Server/P2*Server configurable via uds_cfg.h
4. 0x19 ReadDTCInformation subfunction list must be explicitly controlled
5. LIN error handling: CRC errors, frame timeout, bus sleep recovery
6. Keep-alive bypass per 8.7.6: functionally addressed TesterPresent with SPRMIB=true bypasses busy state
7. Multiple concurrent request handling per 8.7.6

---

## Work Objectives

### Core Objective
Implement a complete UDS (ISO 14229-1:2020) ECU slave driver in C, communicating over LIN bus (ISO 17987, 19200 bps), with hardware abstraction for MCU portability, verified via PC simulation.

### Concrete Deliverables
- `inc/uds/*.h` — All UDS protocol header files (types, services, NRC, session, security, DTC, transport)
- `inc/hal/*.h` — MCU HAL abstraction interface headers
- `src/uds/*.c` — UDS protocol stack implementation (~6000-8000 LOC)
- `src/hal/*.c` — Default stub HAL implementations
- `test/*.c` — TDD test suite (~3000-4000 LOC)
- `test/mock/*.c/h` — Mock HAL implementations
- `test/lin_sim/*.c/h` — LIN master simulator for PC testing
- `sim/main_sim.c` — PC simulation entry point
- `CMakeLists.txt` — Build system

### Definition of Done
- [ ] `cd build && cmake .. && make -j && ctest --output-on-failure` passes all TDD tests
- [ ] All 26 UDS services return correct positive/negative responses per ISO 14229-1 Section 8.7
- [ ] Negative response with NRC 0x7F + SID + NRC for error conditions
- [ ] Session transitions follow Figure 7 state diagram
- [ ] Security access implements Annex I state machine
- [ ] DTC status byte correctly tracks all 8 status transitions
- [ ] LIN transport correctly encodes/decodes NAD + PCI + UDS payload
- [ ] suppressPosRspMsgIndicationBit handled per Tables 4-7
- [ ] Functional addressing NRC suppression per ISO 14229-1 rules

### Must Have
- All 26 UDS services implemented with correct SIDs and response SIDs
- Server response behavior conforming to ISO 14229-1 Section 8.7 (Tables 4-7, 6, 7)
- Negative response framework with full NRC table (Annex A)
- Session state machine (defaultSession 0x01, programmingSession 0x02, extendedDiagnosticSession 0x03)
- Security access seed & key (Annex I state machine)
- DTC management (8 status bits, snapshot records, extended data records)
- LIN transport layer (ISO 17987-3 frame format)
- MCU HAL abstraction (UART/LIN, Timer, NVM)
- TDD test suite in C (Unity test framework)
- CMake build system

### Must NOT Have (Guardrails)
- **No** specific MCU register-level HAL implementation (only abstract interface)
- **No** production bootloader logic
- **No** CAN/CAN-FD transport (LIN only)
- **No** real hardware testing requirements
- **No** PKI/Authentication 0x29 full implementation (stub with NRC 0x34)
- **No** ISO 14229-2 session layer timing engine (P2/P2* are configurable constants)
- **No** security crypto algorithm logic (seed-to-key is user callback)
- **No** excessive abstraction — balance between portability and simplicity
- **No** commented-out dead code or AI-slops (per /ai-slop-remover rules)

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure**: Unity test framework + CTest (via CMake)
- **Automated tests**: TDD (test-first) — each task follows RED→GREEN→REFACTOR
- **Framework**: Unity (micro unit testing framework for embedded C)
- **Runner**: CMake/CTest on PC

### QA Policy
Every task MUST include agent-executed QA scenarios. Evidence saved to `.sisyphus/evidence/task-{N}-{scenario}.{ext}`.

- **PC Tests**: Use Bash to run `ctest` — verify test pass/fail
- **Protocol Verification**: Use Bash to run simulator harness — send UDS request frames, verify response frames
- **Coverage**: Each service task includes BOTH happy-path positive response AND negative response (NRC) scenarios

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Foundation — start immediately, MAX PARALLEL):
├── Task 1: Project scaffolding + CMake build system [quick]
├── Task 2: HAL abstraction interfaces (UART, Timer, NVM, Config) [deep]
├── Task 3: UDS core types & defines (SIDs, NRCs, msg structs, enums) [deep]
└── Task 4: LIN transport layer types & protocol definitions [unspecified-high]

Wave 2 (Core infrastructure — after Wave 1, MAX PARALLEL):
├── Task 5: HAL mock implementations for PC simulation [quick]
├── Task 6: UDS PDU parser/serializer (core protocol engine) [deep]
├── Task 7: Session state machine [deep]
└── Task 8: LIN transport adapter (ISO 17987-3 frame encode/decode) [unspecified-high]

Wave 3a (Service implementations — after Wave 2, MAX PARALLEL):
├── Task 9: Diagnostic & Communication Management services (0x10/0x11/0x27/0x28/0x3E/0x85/0x86/0x87) [deep]
├── Task 10: Data Transmission services (0x22/0x23/0x24/0x2A/0x2C/0x2E/0x3D) [deep]
├── Task 11: Stored Data services (0x14/0x19) [deep]
├── Task 12: IO Control service (0x2F) [deep]
├── Task 13: Routine Control service (0x31) [deep]
└── Task 14: Upload/Download services (0x34/0x35/0x36/0x37/0x38) [deep]

Wave 3b (Infrastructure — after Wave 2, runs alongside 3a):
├── Task 15: DTC state machine engine [deep]
├── Task 16: Security access state machine [unspecified-high]
└── Task 17: DID registry & data storage [unspecified-high]

Wave 4 (Integration — after Waves 3a+3b, SEMI-PARALLEL):
├── Task 18: Master service dispatch (UDS request router) [deep]
├── Task 19: Authentication service 0x29 framework + stubs [unspecified-high]
├── Task 20: LIN master simulator for PC test harness [unspecified-high]
├── Task 21: PC simulation main + integration test runner [unspecified-high]
└── Task 22: Integration E2E tests (full UDS conversation scenarios) [deep]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high + playwright)
└── Task F4: Scope fidelity check (deep)
-> Present results -> Get explicit user okay

Critical Path: Task 1 → 4 → 6 → 8 → 18 → 20 → 22 → F1-F4 → user okay
Max Concurrent: 7 (Wave 3a, 3b — 6 in 3a + 3 in 3b)
```

---

## TODOs

- [x] 1. Project Scaffolding + CMake Build System

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write CMakeLists.txt that attempts to build a test target (Unity framework) and source target (empty for now) — test will fail because no source exists
  - **GREEN**: Create directory structure (`inc/uds/`, `inc/hal/`, `src/uds/`, `src/hal/`, `test/`, `test/mock/`, `test/lin_sim/`, `sim/`), add placeholder files, verify CMake targets configure successfully
  - **REFACTOR**: Organize CMakeLists with proper target separation (library `uds-core`, test targets, simulator executable)
  - Integrate Unity test framework as vendored dependency or FetchContent
  - Set C standard to C11 (`-std=c11`)
  - Add `ctest` integration

  **Must NOT do**:
  - Don't add unnecessary external dependencies beyond Unity
  - Don't create any protocol logic — scaffolding only

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: N/A (simple file/directory creation + CMake)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 1 (with Tasks 2, 3, 4)
  - **Blocks**: Tasks 5-22
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] `mkdir -p build && cd build && cmake ..` succeeds without errors
  - [ ] `cmake --build .` produces no errors (empty targets OK)
  - [ ] Directory structure exists per spec

  **QA Scenarios**:
  ```
  Scenario: Build system configures and builds
    Tool: Bash
    Preconditions: Project directory exists
    Steps:
      1. mkdir -p build && cd build
      2. cmake .. -G "Unix Makefiles"
      3. cmake --build .
    Expected Result: cmake exits 0, build exits 0
    Evidence: .sisyphus/evidence/task-1-build-success.txt

  Scenario: Test target exists (initially no tests)
    Tool: Bash
    Preconditions: Build configured
    Steps:
      1. cd build && cmake --build . --target uds-test
    Expected Result: Target exists, builds without error
    Evidence: .sisyphus/evidence/task-1-test-target.txt
  ```

  **Commit**: YES
  - Message: `build(project): scaffold CMake project with Unity test framework`
  - Files: `CMakeLists.txt`, `inc/` (empty dirs), `src/` (empty dirs), `test/` (empty dirs)

---

- [x] 2. HAL Abstraction Interfaces (UART/LIN, Timer, NVM, Config)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write Unity test that includes hal_uart.h, hal_timer.h, hal_nvm.h, hal_cfg.h and calls each function signature (compilation test) — will fail because headers don't exist
  - **GREEN**: Create 4 header files in `inc/hal/`:
    - `hal_uart.h`: `hal_uart_init(baud)`, `hal_uart_send(buf, len)`, `hal_uart_receive(buf, len, timeout_ms)`, `hal_uart_set_callback(on_rx_cb)`, `hal_uart_enable_irq()`, return `hal_status_t`
    - `hal_timer.h`: `hal_timer_init()`, `hal_timer_start(ms)`, `hal_timer_stop()`, `hal_timer_is_expired()`, `hal_timer_get_remaining_ms()`
    - `hal_nvm.h`: `hal_nvm_read(addr, buf, len)`, `hal_nvm_write(addr, buf, len)`, `hal_nvm_erase(addr, len)`
    - `hal_cfg.h`: MCU-specific config macros (`HAL_ENDIAN_LITTLE`, `HAL_UART_BUFFER_SIZE`, etc.)
    - `hal_common.h`: shared types (`hal_status_t`, `hal_callback_t`)
  - **REFACTOR**: Ensure clean separation — all functions return `hal_status_t` (`HAL_OK=0`, `HAL_ERROR`, `HAL_TIMEOUT`, `HAL_BUSY`)

  **Must NOT do**:
  - Don't implement any MCU-specific register code
  - Don't add protocol logic

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (embedded C interface design)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 1 (with Tasks 1, 3, 4)
  - **Blocks**: Tasks 5, 6, 7, 8
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] `hal_uart.h` compiles standalone with C11 compiler
  - [ ] All function signatures documented with Doxygen-style comments
  - [ ] Return type `hal_status_t` consistently used

  **QA Scenarios**:
  ```
  Scenario: HAL headers compile correctly
    Tool: Bash
    Preconditions: CMake project configured
    Steps:
      1. Create test file that #includes all hal/*.h and calls each function
      2. Compile with gcc -std=c11 -Wall -Werror
    Expected Result: Compilation succeeds with zero warnings
    Evidence: .sisyphus/evidence/task-2-hal-compile.txt
  ```

  **Commit**: YES
  - Message: `feat(hal): define HAL abstraction interfaces for UART, Timer, NVM`
  - Files: `inc/hal/hal_common.h`, `inc/hal/hal_uart.h`, `inc/hal/hal_timer.h`, `inc/hal/hal_nvm.h`, `inc/hal/hal_cfg.h`

---

- [x] 3. UDS Core Types & Defines (SIDs, NRCs, Message Structs, Enums)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write Unity test that validates SID and NRC enum values against ISO 14229-1 spec — test will fail because header doesn't exist
  - **GREEN**: Create `inc/uds/uds_core.h` with:
    - `uds_sid_t` enum: All request SIDs (0x10/DiagnosticSessionControl...0x38/RequestFileTransfer)
    - `uds_response_sid_t` enum: All response SIDs (0x50...0x78)
    - `uds_nrc_t` enum: All NRCs from Annex A (0x00-0xFF)
    - `uds_subfunction_t` struct: `{uint8_t value:7; uint8_t suppress_rsp:1;}`
    - `uds_addressing_t` enum: `{PHYSICAL, FUNCTIONAL}`
    - `uds_session_t` enum: `{DEFAULT_SESSION=0x01, PROGRAMMING_SESSION=0x02, EXTENDED_SESSION=0x03}`
    - `uds_std_return_t` struct: `{uint16_t p2_server_max; uint16_t p2_star_server_max;}`
    - `uds_msg_t` struct: `{uds_addressing_t ta_type; uint8_t nad; uint8_t *data; uint16_t len;}`
    - Helper macros: `IS_REQUEST_SID(x)`, `IS_RESPONSE_SID(x)`, `SID_TO_RESPONSE(sid)`, `SID_FROM_RESPONSE(sid)`
  - **REFACTOR**: Group by functional unit with clear section comments, ensure all hex values match ISO spec

  **Must NOT do**:
  - Don't implement any protocol logic — type definitions only
  - Don't hardcode addresses that should be configurable

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (C header design, data modeling from ISO spec)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 1 (with Tasks 1, 2, 4)
  - **Blocks**: Tasks 6-22
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] All 26 SID request values match ISO 14229-1 Table 2
  - [ ] All response SIDs = request SID + 0x40
  - [ ] All NRC values from Annex A (0x00-0xFF) present
  - [ ] Compilation test passes with -Wall -Werror

  **QA Scenarios**:
  ```
  Scenario: SID values match ISO spec
    Tool: Bash
    Preconditions: uds_core.h exists
    Steps:
      1. Write and compile test that verifies DIAGNOSTIC_SESSION_CONTROL_SID == 0x10
      2. Verify ECU_RESET_SID == 0x11
      3. Verify all 26 SIDs exist and match spec
    Expected Result: All SID values match ISO 14229-1:2020 Table 2
    Evidence: .sisyphus/evidence/task-3-sid-values.txt

  Scenario: Response SID calculation
    Tool: Bash
    Preconditions: uds_core.h exists
    Steps:
      1. Compile test: assert(SID_TO_RESPONSE(0x10) == 0x50)
      2. assert(NEGATIVE_RESPONSE_SID == 0x7F)
    Expected Result: All assertions pass
    Evidence: .sisyphus/evidence/task-3-response-sid.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): define core types, SIDs, NRCs, and message structures`
  - Files: `inc/uds/uds_core.h`

---

- [x] 4. LIN Transport Layer Types & Protocol Definitions

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write Unity test that defines LIN frame structures and transport constants — test verifies expected byte layouts
  - **GREEN**: Create `inc/uds/uds_lin_transport.h` with:
    - `lin_frame_t` struct: 8-byte data array + length
    - `lin_diag_pdu_t` struct: `{uint8_t nad; uint8_t pci; uint8_t *uds_data; uint8_t data_len;}`
    - PCI encoding constants: `LIN_PCI_SF=0x00` (single frame), `LIN_PCI_FF=0x20` (first frame), `LIN_PCI_CF=0x20` (consecutive frame)
    - `LIN_BAUDRATE` default 19200
    - `LIN_NAD_DEFAULT` default 0x01
    - `LIN_FRAME_SIZE` = 8
    - `LIN_SF_MAX_LEN` = 6 (max UDS data bytes in single frame)
    - Diagnostic frame ID constants (`LIN_DIAG_REQUEST_ID`, `LIN_DIAG_RESPONSE_ID`)
    - Transport state enum: `{LIN_TX_IDLE, LIN_TX_SF, LIN_TX_FF, LIN_TX_CF, LIN_RX_IDLE, LIN_RX_SF, LIN_RX_FF, LIN_RX_CF}`
    - LIN error codes: `{LIN_OK, LIN_CRC_ERROR, LIN_TIMEOUT, LIN_BUS_ERROR}`
  - **REFACTOR**: Ensure all constants are macro-definable via compiler flags

  **Must NOT do**:
  - Don't implement frame encode/decode logic (Task 8)
  - Don't add UDS-specific service logic

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: N/A (embedded protocols, LIN specification)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 1 (with Tasks 1, 2, 3)
  - **Blocks**: Task 8
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] All LIN transport constants defined and documented
  - [ ] Struct layouts correct for 8-byte LIN frames
  - [ ] PCI encoding logic matches ISO 17987-3

  **QA Scenarios**:
  ```
  Scenario: LIN frame structure layout
    Tool: Bash
    Preconditions: uds_lin_transport.h exists
    Steps:
      1. Compile test: sizeof(lin_frame_t) == 8
      2. Verify offsets compile correctly
    Expected Result: Size assertions pass
    Evidence: .sisyphus/evidence/task-4-lin-struct.txt
  ```

  **Commit**: YES
  - Message: `feat(lin): define LIN transport types, PCI encoding, and frame structures`
  - Files: `inc/uds/uds_lin_transport.h`

---

- [x] 5. HAL Mock Implementations for PC Simulation

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write Unity test that includes mock headers and verifies mock functions return expected default values
  - **GREEN**: Create `test/mock/mock_uart.c/h`, `test/mock/mock_timer.c/h`, `test/mock/mock_nvm.c/h`
    - `mock_uart.c`: Buffer-based send/receive, configurable delays, callback invocation
    - `mock_timer.c`: Software timer with simulated time advancement (`mock_timer_advance_ms()`)
    - `mock_nvm.c`: RAM-based storage array, all read/write/erase operations
    - Mock verification macros: `MOCK_UART_ASSERT_SENT(data, len)`, `MOCK_UART_ASSERT_RX_COUNT(n)`
  - **REFACTOR**: Add thread-safety macros for RTOS mode (mutex stubs for bare-metal)

  **Must NOT do**:
  - No actual hardware access
  - Don't implement real LIN protocol — just UART-level byte send/receive

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: N/A (C mocks, test doubles)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 2 (with Tasks 6, 7, 8)
  - **Blocks**: Tasks 20, 21, 22
  - **Blocked By**: Task 2

  **Acceptance Criteria**:
  - [ ] Mock UART stores sent bytes for later assertion
  - [ ] Mock UART can inject received bytes
  - [ ] Mock timer supports time advancement in test control
  - [ ] Mock NVM stores data persistently within test scope
  - [ ] All test/use in test session compiles

  **QA Scenarios**:
  ```
  Scenario: Mock UART send/receive roundtrip
    Tool: Bash
    Preconditions: Mock UART implemented
    Steps:
      1. Write test: send 0x10 0x01 via mock_uart_send
      2. Assert mock_uart_get_sent() contains 0x10, 0x01
      3. Inject 0x7F via mock_uart_inject_rx()
      4. Read via mock_uart_receive() and assert value
    Expected Result: All assertions pass
    Evidence: .sisyphus/evidence/task-5-mock-uart.txt

  Scenario: Mock timer advancement
    Tool: Bash
    Preconditions: Mock timer implemented
    Steps:
      1. hal_timer_start(100)
      2. Assert NOT expired
      3. mock_timer_advance_ms(100)
      4. Assert expired
    Expected Result: Timer behavior correct
    Evidence: .sisyphus/evidence/task-5-mock-timer.txt
  ```

  **Commit**: YES
  - Message: `test(mock): add mock HAL implementations for PC simulation testing`
  - Files: `test/mock/mock_uart.c`, `test/mock/mock_uart.h`, `test/mock/mock_timer.c`, `test/mock/mock_timer.h`, `test/mock/mock_nvm.c`, `test/mock/mock_nvm.h`

---

- [x] 6. UDS PDU Parser/Serializer (Core Protocol Engine)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write test cases for:
    - Parsing a valid request PDU → extract SID, SubFunction, data params
    - Serializing a positive response PDU
    - Serializing a negative response PDU (0x7F + SID + NRC)
    - Rejecting malformed messages (too short, wrong format)
    - Correct SID → response SID mapping
  - **GREEN**: Create `src/uds/uds_core.c` + `inc/uds/uds_core.h` extensions:
    - `uds_parse_request(const uint8_t *raw, uint16_t len, uds_request_t *req)` — parse raw bytes into structured request
    - `uds_serialize_response(const uds_response_t *rsp, uint8_t *buf, uint16_t *len)` — serialize response
    - `uds_serialize_negative_response(uint8_t sid, uds_nrc_t nrc, uint8_t *buf, uint16_t *len)` — serialize NRC
    - `uds_is_positive_response(const uint8_t *data)` — check if response[0] != 0x7F
    - `uds_sid_to_response_sid(uint8_t sid)` — request SID → response SID
  - **REFACTOR**: Error-proof parsing (bounds checking on every read)

  **Must NOT do**:
  - Don't implement service-specific handling (dispatch is separate)
  - Don't add transport layer concerns

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (embedded protocol implementation, buffer-oriented C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 2 (with Tasks 5, 7, 8)
  - **Blocks**: Tasks 9-19 (all service implementations)
  - **Blocked By**: Task 3 (needs SID/NRC types)

  **Acceptance Criteria**:
  - [ ] Parse correctly extracts SID from any valid request
  - [ ] Serialize positive response produces correct byte sequence
  - [ ] Serialize negative response produces `[0x7F][SID][NRC]` sequence
  - [ ] Parse rejects messages with data length < 1 with error
  - [ ] All TDD tests pass

  **QA Scenarios**:
  ```
  Scenario: Parse valid DiagnosticSessionControl request
    Tool: Bash (run compiled test)
    Preconditions: uds_core test suite build
    Steps:
      1. Input bytes: {0x10, 0x03} (DiagnosticSessionControl, extendedSession)
      2. Call uds_parse_request()
      3. Assert req->sid == 0x10, req->subfunc == 0x03
    Expected Result: Correct parse
    Evidence: .sisyphus/evidence/task-6-parse.txt

  Scenario: Serialize negative response
    Tool: Bash
    Preconditions: uds_core test suite build
    Steps:
      1. Call uds_serialize_negative_response(0x22, 0x31, buf, &len)
      2. Assert buf[0]==0x7F, buf[1]==0x22, buf[2]==0x31, len==3
    Expected Result: Correct NRC format
    Evidence: .sisyphus/evidence/task-6-nrc.txt

  Scenario: Reject empty request
    Tool: Bash
    Preconditions: uds_core test suite build
    Steps:
      1. Input: len=0
      2. Call uds_parse_request()
      3. Assert return error code
    Expected Result: Parse fails gracefully
    Evidence: .sisyphus/evidence/task-6-empty.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement PDU parser/serializer core protocol engine`
  - Files: `src/uds/uds_core.c`, `inc/uds/uds_core.h` (extended)

---

- [x] 7. Session State Machine

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for:
    - Default to defaultSession on init
    - Transition from defaultSession → programmingSession → extendedSession
    - Transition back to defaultSession resets security (re-locks)
    - Transition between non-default sessions requires re-auth
    - Session parameter record (P2Server_max, P2*Server_max) per session
    - Invalid session type → NRC 0x12 (SFNS)
  - **GREEN**: Create `src/uds/uds_session.c` + `inc/uds/uds_session.h`:
    - `uds_session_state_t` enum: `{DEFAULT_SESSION, PROGRAMMING_SESSION, EXTENDED_SESSION}`
    - `uds_session_context_t` struct: current session, timing params, paused events flag
    - `uds_session_init(ctx)` — initialize to defaultSession
    - `uds_session_switch(ctx, new_session, *nrc)` — attempt session switch, return NRC on failure
    - `uds_session_get_params(ctx)` — return sessionParameterRecord
    - `uds_session_is_supported(session)` — check if session type supported
    - Session transition rules per ISO 14229-1 Figure 7:
      - defaultSession → any: pause events
      - any non-default → default: resume events, re-lock security
      - non-default → non-default: stop events, re-lock, maintain comms/DTC settings
  - **REFACTOR**: Make session parameter table configurable via `uds_cfg.h`

  **Must NOT do**:
  - Don't implement service dispatch (just session management)
  - Don't implement security directly (call security API hooks)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (state machine design, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 2 (with Tasks 5, 6, 8)
  - **Blocks**: Tasks 9, 16, 18
  - **Blocked By**: Task 3 (SID types)

  **Acceptance Criteria**:
  - [ ] Initial state is defaultSession
  - [ ] Supported sessions: default(0x01), programming(0x02), extended(0x03)
  - [ ] Session parameter record contains P2Server_max + P2*Server_max per session
  - [ ] Invalid session returns NRC 0x12
  - [ ] All transition rules from Figure 7 implemented
  - [ ] TDD tests: RED→GREEN passes

  **QA Scenarios**:
  ```
  Scenario: Default session initialization
    Tool: Bash (run test)
    Preconditions: Session module built
    Steps:
      1. uds_session_init(&ctx)
      2. Assert ctx.current_session == DEFAULT_SESSION
    Expected Result: Starts in defaultSession
    Evidence: .sisyphus/evidence/task-7-init.txt

  Scenario: Switch to programming session
    Tool: Bash
    Preconditions: Session initialized
    Steps:
      1. uds_session_switch(&ctx, PROGRAMMING_SESSION, &nrc)
      2. Assert nrc == 0 (success)
      3. Assert ctx.current_session == PROGRAMMING_SESSION
    Expected Result: Session switches, params available
    Evidence: .sisyphus/evidence/task-7-switch.txt

  Scenario: Reject invalid session
    Tool: Bash
    Preconditions: Session initialized
    Steps:
      1. uds_session_switch(&ctx, 0xFF, &nrc)
      2. Assert nrc == 0x12 (SFNS)
      3. Assert ctx.current_session unchanged
    Expected Result: Invalid session returns NRC 0x12
    Evidence: .sisyphus/evidence/task-7-invalid.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement session state machine with Figure 7 transition rules`
  - Files: `src/uds/uds_session.c`, `inc/uds/uds_session.h`

---

- [x] 8. LIN Transport Adapter (ISO 17987-3 Frame Encode/Decode)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for:
    - Encode a UDS message into LIN Single Frame (SF) format (NAD + PCI + UDS data)
    - Encode a UDS message that spans multiple LIN frames (FF + CF)
    - Decode a received LIN frame back into UDS payload
    - Error handling for malformed frames (bad PCI, CRC error, timeout)
    - NAD matching (reject frames not addressed to this slave)
  - **GREEN**: Create `src/uds/uds_lin_transport.c` + extend `inc/uds/uds_lin_transport.h`:
    - `lin_tx_encode(lin_diag_pdu_t *pdu, lin_frame_t *frames, uint8_t *frame_count)` — encode UDS PDU → 1+ LIN frames
    - `lin_rx_decode(lin_frame_t *frame, lin_diag_pdu_t *pdu)` — decode single LIN frame → UDS PDU
    - `lin_transport_reset()` — clear transport state (for bus wake/new session)
    - Multi-frame transport state machine (SF, FF, CF handling)
    - NAD filtering (compare received NAD against configured node address)
    - Timestamp tracking for bus timeout detection
  - **REFACTOR**: Make buffer sizes configurable, support large transfer segmentation

  **Must NOT do**:
  - Don't implement UDS service logic
  - Don't call HAL directly (transport layer is above HAL)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: N/A (embedded transport protocol implementation)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 2 (with Tasks 5, 6, 7)
  - **Blocks**: Tasks 18, 20, 22
  - **Blocked By**: Tasks 3, 4

  **Acceptance Criteria**:
  - [ ] Short UDS message (≤6 bytes) → single SF frame
  - [ ] Long UDS message → FF + CF sequence
  - [ ] NAD mismatch → frame rejected silently
  - [ ] Transport state machine handles multi-frame reassembly
  - [ ] All TDD tests pass

  **QA Scenarios**:
  ```
  Scenario: Encode single-frame UDS request
    Tool: Bash
    Preconditions: LIN transport module built
    Steps:
      1. Create pdu with NAD=0x01, uds_data={0x10, 0x03}, len=2
      2. Call lin_tx_encode() → expect 1 frame
      3. Frame[0]: {0x01, 0x02, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00}
    Expected Result: Valid SF frame with correct PCI length
    Evidence: .sisyphus/evidence/task-8-sf-encode.txt

  Scenario: NAD mismatch rejection
    Tool: Bash
    Preconditions: Transport module configured with NAD=0x01
    Steps:
      1. Receive frame with NAD=0x02
      2. Call lin_rx_decode()
      3. Assert return code indicates NAD mismatch
    Expected Result: Frame rejected, no UDS data extracted
    Evidence: .sisyphus/evidence/task-8-nad-reject.txt
  ```

  **Commit**: YES
  - Message: `feat(lin): implement LIN transport adapter with SF/FF/CF frame handling`
  - Files: `src/uds/uds_lin_transport.c`, `inc/uds/uds_lin_transport.h` (extended)

---

- [x] 9. Diagnostic & Communication Management Services (0x10/0x11/0x27/0x28/0x3E/0x85/0x86/0x87)

  **What to do** (TDD: RED → GREEN → REFACTOR) — implement each service as separate test→code cycle:
  - **RED→GREEN per service** (8 sub-services):
    1. **0x10 DiagnosticSessionControl**: Dispatch to session module, return response with sessionParameterRecord. Handle NRC: 0x12, 0x13, 0x22
    2. **0x11 ECUReset**: Accept resetType SubFunction, return response SID 0x51. Handle NRC: 0x12, 0x13, 0x22, 0x33
    3. **0x27 SecurityAccess**: Implement requestSeed/sendKey flow, call security module. Handle NRC: 0x12, 0x13, 0x24, 0x31, 0x35, 0x36, 0x37
    4. **0x28 CommunicationControl**: controlType + communicationType. Handle NRC: 0x12, 0x13, 0x22, 0x31
    5. **0x3E TesterPresent**: Echo SubFunction, keep session alive. Handle NRC: 0x12, 0x13
    6. **0x85 ControlDTCSetting**: on/off SubFunction. Handle NRC: 0x12, 0x13, 0x22, 0x33
    7. **0x86 ResponseOnEvent**: Event setup/control. Handle NRC: 0x12, 0x13, 0x22, 0x31
    8. **0x87 LinkControl**: baudrate transition. Handle NRC: 0x12, 0x13, 0x22
  - All services follow ISO 14229-1 Section 8.7 response rules (Tables 4-7)
  - SPRMIB (suppressPosRspMsgIndicationBit) handled per spec
  - Functional addressing NRC suppression implemented
  - `src/uds/uds_svc_diagcomm.c` + `inc/uds/uds_svc_diagcomm.h`

  **Must NOT do**:
  - Don't implement DID/data storage (separate module)
  - Don't implement NVM programming logic for security keys (use callback)
  - Leave 0x29 Authentication to Task 19

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (UDS protocol, state machines, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 10-17)
  - **Blocks**: Task 18 (dispatch), Task 22 (integration)
  -   **Blocked By**: Tasks 6, 7, 16

  **Acceptance Criteria**:
  - [ ] Each service returns correct positive response for valid requests
  - [ ] Each service returns correct NRC for each specified error condition
  - [ ] suppressPosRspMsgIndicationBit works per Tables 4-7
  - [ ] Functional addressing suppresses NRC 0x11/0x12/0x31 per spec
  - [ ] TDD tests: per-service RED→GREEN passes

  **QA Scenarios** (per service — shown for 0x10 as representative):
  ```
  Scenario: DiagnosticSessionControl — request extended session (physical)
    Tool: Bash
    Preconditions: Session module initialized
    Steps:
      1. Call service handler with SID=0x10, SubFunction=0x03
      2. Assert response SID = 0x50
      3. Assert response SubFunction = 0x03
      4. Assert sessionParameterRecord present (4 bytes)
    Expected Result: Positive response 0x50 + 0x03 + P2/P2* timings
    Evidence: .sisyphus/evidence/task-9-svc10-pos.txt

  Scenario: DiagnosticSessionControl — unsupported session
    Tool: Bash
    Preconditions: Session module initialized
    Steps:
      1. Call with SID=0x10, SubFunction=0xFF
      2. Assert NRC received
      3. Assert NRC == 0x12 (SFNS)
    Expected Result: Negative response with NRC 0x12
    Evidence: .sisyphus/evidence/task-9-svc10-neg.txt

  Scenario: TesterPresent with SPRMIB=true (functional)
    Tool: Bash
    Preconditions: In non-default session
    Steps:
      1. Call with SID=0x3E, SubFunction=0x80 (SPRMIB=true)
      2. Assert suppressResponse flag set
    Expected Result: No positive response sent
    Evidence: .sisyphus/evidence/task-9-svc3e-suppress.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement diagnostic and communication management services (0x10/11/27/28/3E/85/86/87)`
  - Files: `src/uds/uds_svc_diagcomm.c`, `inc/uds/uds_svc_diagcomm.h`

---

- [x] 10. Data Transmission Services (0x22/0x23/0x24/0x2A/0x2C/0x2E/0x3D)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED→GREEN per service** (7 sub-services):
    1. **0x22 ReadDataByIdentifier**: DID lookup in DID registry, return data. NRC: 0x13, 0x22, 0x31, 0x33
    2. **0x23 ReadMemoryByAddress**: Address+size → read from memory region. NRC: 0x13, 0x22, 0x31, 0x33
    3. **0x24 ReadScalingDataByIdentifier**: DID + scaling info. NRC: 0x13, 0x22, 0x31
    4. **0x2A ReadDataByPeriodicIdentifier**: Schedule-based periodic DID reading. NRC: 0x13, 0x22, 0x31
    5. **0x2C DynamicallyDefineDataIdentifier**: Create dynamic DIDs from source DIDs/memory. NRC: 0x13, 0x22, 0x31, 0x33
    6. **0x2E WriteDataByIdentifier**: DID lookup → validate → write data. NRC: 0x13, 0x22, 0x31, 0x33
    7. **0x3D WriteMemoryByAddress**: Address+size → validate → write memory. NRC: 0x13, 0x22, 0x31, 0x33
  - `src/uds/uds_svc_data.c` + `inc/uds/uds_svc_data.h`
  - Interface with DID registry (Task 17) and DTC module (Task 15)

  **Must NOT do**:
  - Don't implement DID storage (DID registry is separate, Task 17)
  - Don't implement memory programming (upload/download is Task 14)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (UDS protocol services, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9, 11-17)
  - **Blocks**: Task 18 (dispatch)
  - **Blocked By**: Tasks 6, 17 (DID registry)

  **Acceptance Criteria**:
  - [ ] 0x22 returns data for known DIDs, NRC 0x31 for unknown
  - [ ] 0x2E writes data for writeable DIDs
  - [ ] 0x23/0x3D memory operations with address bounds checking
  - [ ] 0x2A periodic scheduling (configurable timer interface)
  - [ ] 0x2C dynamic DID create/delete
  - [ ] All security-gated services check access level

  **QA Scenarios**:
  ```
  Scenario: ReadDataByIdentifier — known DID
    Tool: Bash
    Preconditions: DID registry has DID=0xF190 (VIN, 17 bytes)
    Steps:
      1. Request: SID=0x22, DID=0xF190
      2. Assert response SID = 0x62
      3. Assert response data contains 17-byte VIN
    Expected Result: Positive response with VIN data
    Evidence: .sisyphus/evidence/task-10-rdi-positive.txt

  Scenario: ReadDataByIdentifier — unknown DID
    Tool: Bash
    Preconditions: DID registry initialized
    Steps:
      1. Request: SID=0x22, DID=0xFFFF
      2. Assert NRC received, NRC == 0x31 (ROOR)
    Expected Result: Negative response 0x7F + 0x22 + 0x31
    Evidence: .sisyphus/evidence/task-10-rdi-negative.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement data transmission services (0x22/23/24/2A/2C/2E/3D)`
  - Files: `src/uds/uds_svc_data.c`, `inc/uds/uds_svc_data.h`

---

- [x] 11. Stored Data Services (0x14/0x19 — ClearDiagnosticInformation, ReadDTCInformation)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED→GREEN per service** (2 sub-services):
    1. **0x14 ClearDiagnosticInformation**: Clear DTCs by group (powertrain/chassis/body/all). NRC: 0x13, 0x22, 0x31, 0x33
    2. **0x19 ReadDTCInformation**: Multiple subfunctions:
       - 0x01: reportNumberOfDTCByStatusMask
       - 0x02: reportDTCByStatusMask
       - 0x06: reportDTCExtendedDataRecordByDTCNumber
       - 0x0A: reportSupportedDTC (and others from Table 271)
  - `src/uds/uds_svc_stored.c` + `inc/uds/uds_svc_stored.h`
  - Interface with DTC state machine (Task 15)

  **Must NOT do**:
  - Don't duplicate DTC state machine logic (call Task 15 API)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (UDS DTC services, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9, 10, 12-17)
  - **Blocks**: Task 18 (dispatch)
  - **Blocked By**: Tasks 6, 15 (DTC engine)

  **Acceptance Criteria**:
  - [ ] 0x14 clears DTCs by group (0x000000-0xFFFFFF mask support)
  - [ ] 0x19 subfunction 0x02 returns DTC list by status mask
  - [ ] 0x19 subfunction 0x06 returns extended data records
  - [ ] NRC 0x31 for invalid group/subfunction

  **QA Scenarios**:
  ```
  Scenario: ReadDTCInformation by status mask
    Tool: Bash
    Preconditions: DTC engine has 3 DTCs stored with various status
    Steps:
      1. Request: SID=0x19, SF=0x02, statusMask=0x39
      2. Assert response SID = 0x59
      3. Assert response lists DTCs matching mask
    Expected Result: Correct DTC list
    Evidence: .sisyphus/evidence/task-11-dtc-list.txt

  Scenario: Clear diagnostic information (all groups)
    Tool: Bash
    Preconditions: DTC engine has stored DTCs
    Steps:
      1. Request: SID=0x14, groupOfDTC={0xFFFFFF}
      2. Assert response SID = 0x54
      3. Verify all DTCs cleared from engine
    Expected Result: All DTCs cleared
    Evidence: .sisyphus/evidence/task-11-dtc-clear.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement stored data services (0x14 ClearDTC, 0x19 ReadDTCInfo)`
  - Files: `src/uds/uds_svc_stored.c`, `inc/uds/uds_svc_stored.h`

---

- [x] 12. IO Control Service (0x2F — InputOutputControlByIdentifier)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for IO control with various control modes:
    - returnControlToStandard, resetToDefault, freezeCurrentState, shortTermAdjustment
    - Known DID returns correct control states
    - Unknown DID → NRC 0x31
    - Security access check → NRC 0x33 if locked
  - **GREEN**: Create `src/uds/uds_svc_io.c` + `inc/uds/uds_svc_io.h`:
    - `uds_svc_io_control()` — main handler for 0x2F
    - Control state management (per-DID current/overridden values)
    - Interface with DID registry for IO-capable DIDs
    - Control mode enum: `{RETURN_TO_STANDARD=0x01, RESET_TO_DEFAULT=0x02, FREEZE=0x03, SHORT_TERM_ADJUST=0x04}`
  - **REFACTOR**: Ensure IO state resets properly on session switch

  **Must NOT do**:
  - Don't implement actual hardware IO (callback to HAL)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (UDS IO control, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9-11, 13-17)
  - **Blocks**: Task 18
  - **Blocked By**: Tasks 6, 17 (DID registry)

  **Acceptance Criteria**:
  - [ ] 0x2F returns correct positive response for valid DID + controlMode
  - [ ] Unknown DID returns NRC 0x31
  - [ ] Security-gated DID requires unlocked state
  - [ ] IO control state resets on session end

  **QA Scenarios**:
  ```
  Scenario: IO control short-term adjustment
    Tool: Bash
    Preconditions: DID 0xF100 registered as IO-capable, unlocked
    Steps:
      1. Request: SID=0x2F, DID=0xF100, controlMode=0x04, data={0x64}
      2. Assert response SID = 0x6F
      3. Assert controlMode echoed = 0x04
    Expected Result: Positive response
    Evidence: .sisyphus/evidence/task-12-io-positive.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement IO control service (0x2F InputOutputControlByIdentifier)`
  - Files: `src/uds/uds_svc_io.c`, `inc/uds/uds_svc_io.h`

---

- [x] 13. Routine Control Service (0x31 — RoutineControl)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for routine control:
    - Start routine → response includes routineStatusRecord
    - Stop routine → response acknowledges stop
    - Request routine results
    - Unknown routineID → NRC 0x31
    - Security check → NRC 0x33
  - **GREEN**: Create `src/uds/uds_svc_routine.c` + `inc/uds/uds_svc_routine.h`:
    - `uds_svc_routine_control()` — main handler for 0x31
    - Routine control subfunction enum: `{START=0x01, STOP=0x02, REQUEST_RESULTS=0x03}`
    - Routine registry with callbacks: user registers (routineID, start_fn, stop_fn, results_fn)
    - Default routines: eraseMemory (0xFF00), checkProgrammingIntegrity (0xFF01)
  - **REFACTOR**: Thread-safe routine execution (blocking for now, hook for async)

  **Must NOT do**:
  - Don't implement actual programming logic (user callback)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (UDS routine control, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9-12, 14-17)
  - **Blocks**: Task 18
  - **Blocked By**: Tasks 6

  **Acceptance Criteria**:
  - [ ] Start routine returns routineStatusRecord
  - [ ] Stop routine stops active routine
  - [ ] Request results returns last known results
  - [ ] Unknown routineID → NRC 0x31

  **QA Scenarios**:
  ```
  Scenario: Start routine with valid ID
    Tool: Bash
    Preconditions: Routine 0xFF00 (eraseMemory) registered
    Steps:
      1. Request: SID=0x31, SF=0x01, routineID=0xFF00
      2. Assert response SID = 0x71
      3. Assert routineID echoed
    Expected Result: Positive response
    Evidence: .sisyphus/evidence/task-13-routine-start.txt

  Scenario: Unknown routine ID
    Tool: Bash
    Preconditions: No routine at 0x9999
    Steps:
      1. Request: SID=0x31, SF=0x01, routineID=0x9999
      2. Assert NRC == 0x31 (ROOR)
    Expected Result: Negative response
    Evidence: .sisyphus/evidence/task-13-routine-unknown.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement routine control service (0x31 RoutineControl)`
  - Files: `src/uds/uds_svc_routine.c`, `inc/uds/uds_svc_routine.h`

---

- [x] 14. Upload/Download Services (0x34/0x35/0x36/0x37/0x38)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED→GREEN per service** (5 sub-services):
    1. **0x34 RequestDownload**: memoryAddress + memorySize + format → set up download context, return maxBlockLength. NRC: 0x13, 0x22, 0x31, 0x33, 0x70
    2. **0x35 RequestUpload**: similar to 0x34 but for upload direction
    3. **0x36 TransferData**: blockSequenceCounter + data → write/read data to/from transfer context. NRC: 0x13, 0x22, 0x31, 0x71, 0x73
    4. **0x37 RequestTransferExit**: terminate active transfer context. NRC: 0x13, 0x22, 0x31
    5. **0x38 RequestFileTransfer**: file path/name + mode → file-based transfer context. NRC: 0x13, 0x22, 0x31, 0x33, 0x70
  - `src/uds/uds_svc_upload.c` + `inc/uds/uds_svc_upload.h`
  - Transfer context manager: buffer (configurable size), blockSequenceCounter validation, direction tracking
  - Memory region validation (address bounds per configured flash/RAM regions)

  **Must NOT do**:
  - Don't implement actual flash programming (user callback)
  - 0x38 is basic stub with file naming support but no actual filesystem

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (UDS upload/download, memory management, C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9-13, 15-17)
  - **Blocks**: Task 18
  - **Blocked By**: Tasks 6

  **Acceptance Criteria**:
  - [ ] 0x34 sets up download context and returns maxBlockLength
  - [ ] 0x36 TransferData accepts blocks in correct sequence (BS=1)
  - [ ] 0x36 wrong blockSequenceCounter → NRC 0x73
  - [ ] 0x37 terminates active transfer
  - [ ] Transfer context rejected if overlapping active transfer → NRC 0x21/0x22
  - [ ] 0x38 basic framework returns NRC 0x31 for unsupported (stub)

  **QA Scenarios**:
  ```
  Scenario: RequestDownload + TransferData + TransferExit
    Tool: Bash
    Preconditions: Memory region configured (0x8000-0x8FFF)
    Steps:
      1. Request: SID=0x34, dataFormatIdentifier=0x00, addr=0x8000, size=0x100
      2. Assert response SID=0x74, maxBlockLength > 0
      3. TransferData: SID=0x36, blockSeqCounter=0x01, data=N bytes
      4. Assert response SID=0x76
      5. RequestTransferExit: SID=0x37
      6. Assert response SID=0x77
    Expected Result: Complete download sequence succeeds
    Evidence: .sisyphus/evidence/task-14-download.txt

  Scenario: Wrong block sequence counter
    Tool: Bash
    Preconditions: Download context active, last BSC=0x01
    Steps:
      1. TransferData: BSC=0x03 (expected 0x02)
      2. Assert NRC == 0x73 (wrongBlockSequenceCounter)
    Expected Result: Negative response
    Evidence: .sisyphus/evidence/task-14-bsc-error.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement upload/download services (0x34/35/36/37/38)`
  - Files: `src/uds/uds_svc_upload.c`, `inc/uds/uds_svc_upload.h`

---

- [x] 15. DTC State Machine Engine

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for:
    - DTC status byte with all 8 bits (ISO 14229-1 Annex D)
    - DTC status transitions for each bit (testFailed → pending → confirmed → etc.)
    - Snapshot data record storage and retrieval
    - Extended data record storage and retrieval
    - DTC group filtering (powertrain 0x00xxxx, chassis 0x20xxxx, body 0x40xxxx)
    - Multiple DTC storage (configurable capacity, typically 50-100)
    - Clear DTC by group or all
    - Status mask filtering for 0x19 service
  - **GREEN**: Create `src/uds/uds_dtc.c` + `inc/uds/uds_dtc.h`:
    - `uds_dtc_t` struct: `{uint32_t dtc; uint8_t status; uint8_t *snapshot; uint16_t snapshot_len; uint8_t *extended; uint16_t ext_len;}`
    - `uds_dtc_init()` — initialize DTC database
    - `uds_dtc_set_status(dtc, bit_mask, set/clear)` — update individual status bits
    - `uds_dtc_get_by_mask(status_mask)` — return DTCs matching given status bits
    - `uds_dtc_clear(group_mask)` — clear DTCs by group
    - `uds_dtc_get_snapshot(dtc)` / `uds_dtc_get_extended(dtc)` — retrieve records
    - DTC status bit enum: `{TEST_FAILED=0x01, TEST_FAILED_THIS_OP_CYCLE=0x02, PENDING=0x04, CONFIRMED=0x08, TEST_FAILED_SINCE_LAST_CLEAR=0x10, TEST_FAILED_SINCE_THIS_OP_CYCLE=0x20, WARNING_INDICATOR=0x40, TEST_NOT_COMPLETED=0x80}`
  - **REFACTOR**: Add optional NVM storage hooks for persistent DTC storage

  **Must NOT do**:
  - Don't implement service dispatch (0x14/0x19 handlers are Tasks 11)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (DTC state machine, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9-14, 16, 17)
  - **Blocks**: Task 11, Task 18
  - **Blocked By**: Task 3 (needs types)

  **Acceptance Criteria**:
  - [ ] All 8 status bits independently settable/cleared
  - [ ] Status transitions follow DTC state diagram rules
  - [ ] Snapshot records stored per DTC on TEST_FAILED transition
  - [ ] Extended data records retrievable per DTC
  - [ ] Group filtering works correctly
  - [ ] TDD tests pass

  **QA Scenarios**:
  ```
  Scenario: DTC status transitions (test failed → confirmed)
    Tool: Bash
    Preconditions: DTC engine initialized empty
    Steps:
      1. uds_dtc_set_status(0x010203, TEST_FAILED, true)
      2. Assert status bit TEST_FAILED set
      3. uds_dtc_set_status(0x010203, PENDING, true)
      4. uds_dtc_set_status(0x010203, CONFIRMED, true)
      5. Assert all three bits set
      6. uds_dtc_clear(0xFF0000) — clear powertrain only
      7. Assert DTC 0x010203 cleared (belongs to powertrain)
    Expected Result: Status bits track correctly, group clear works
    Evidence: .sisyphus/evidence/task-15-dtc-status.txt

  Scenario: Filter DTCs by status mask
    Tool: Bash
    Preconditions: 3 DTCs with different status combinations
    Steps:
      1. uds_dtc_get_by_mask(0x09) — get CONFIRMED|TEST_FAILED_SINCE_LAST_CLEAR
      2. Assert correct DTCs returned
    Expected Result: Filtering correct
    Evidence: .sisyphus/evidence/task-15-dtc-filter.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement DTC state machine engine with 8-bit status tracking`
  - Files: `src/uds/uds_dtc.c`, `inc/uds/uds_dtc.h`

---

- [x] 16. Security Access State Machine

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for:
    - Security access state machine per Annex I (locked/unlocked states)
    - requestSeed (odd SubFunction) → returns non-zero seed
    - sendKey (even SubFunction) → validate key, unlock on match
    - Invalid key → NRC 0x35; repeated invalid attempts → NRC 0x36 after limit
    - Delay timer after failed attempts → NRC 0x37
    - Already unlocked → seed = 0x0000
    - Re-lock on session change (non-default → default)
    - requestSequenceError (sendKey before requestSeed) → NRC 0x24
  - **GREEN**: Create `src/uds/uds_security.c` + `inc/uds/uds_security.h`:
    - `uds_security_init()` — locked, counter=0
    - `uds_security_request_seed(level, seed_buf, *seed_len, *nrc)` — generate seed, track level
    - `uds_security_send_key(level, key_buf, key_len, *nrc)` — validate via callback
    - `uds_security_lock()` — force re-lock
    - `uds_security_is_unlocked(level)` — check access
    - `uds_security_get_delay_remaining()` — return remaining delay time
    - Configurable: `SECURITY_MAX_ATTEMPTS` (default 5), `SECURITY_DELAY_MS` (default 10000)
    - `security_key_validate_t` callback — user provides key validation logic
  - **REFACTOR**: Thread-safe protection for state changes

  **Must NOT do**:
  - Don't implement actual crypto (user callback for seed→key validation)
  - Don't integrate with session directly (called by service handler)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: N/A (state machine, automotive security, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9-15, 17)
  - **Blocks**: Tasks 9, 18
  - **Blocked By**: Tasks 3, 5 (timer mock for delay testing)

  **Acceptance Criteria**:
  - [ ] requestSeed returns non-zero seed when locked
  - [ ] sendKey with valid key transitions to unlocked
  - [ ] sendKey with invalid key stays locked → NRC 0x35
  - [ ] After N failed attempts → NRC 0x36, delay timer active → NRC 0x37
  - [ ] session lock function works
  - [ ] Annex I state chart compliance

  **QA Scenarios**:
  ```
  Scenario: Successful security access sequence
    Tool: Bash
    Preconditions: Security module initialized, locked
    Steps:
      1. requestSeed(level=0x01) → seed returned (non-zero), NRC=0
      2. sendKey(level=0x02, key=matching_key) → unlocked, NRC=0
      3. Assert uds_security_is_unlocked(level) == true
    Expected Result: Full unlock sequence succeeds
    Evidence: .sisyphus/evidence/task-16-sec-success.txt

  Scenario: Invalid key rejected
    Tool: Bash
    Preconditions: Security locked, after requestSeed
    Steps:
      1. sendKey(level=0x02, key=wrong_key) → NRC 0x35
      2. Assert still locked
    Expected Result: NRC 0x35, no unlock
    Evidence: .sisyphus/evidence/task-16-sec-invalid.txt

  Scenario: Request sequence error (sendKey before requestSeed)
    Tool: Bash
    Preconditions: Security locked, no requestSeed issued
    Steps:
      1. sendKey(level=0x02, key=any) → NRC 0x24
    Expected Result: NRC 0x24 (requestSequenceError)
    Evidence: .sisyphus/evidence/task-16-sec-seq-error.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement security access state machine per Annex I`
  - Files: `src/uds/uds_security.c`, `inc/uds/uds_security.h`

---

- [x] 17. DID Registry & Data Storage

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for:
    - Register DID with metadata (ID, length, access rights, data pointer)
    - Read DID value
    - Write DID value (for writable DIDs)
    - Security access check (read-only, write-only, read-write, secured)
    - Unknown DID → not found error
    - DID group query (for 0x22 read-by-identifier multiple)
    - Session-based access control (some DIDs only available in non-default session)
  - **GREEN**: Create `src/uds/uds_data.c` + `inc/uds/uds_data.h`:
    - `uds_did_entry_t` struct: `{uint16_t did; uint16_t len; uint8_t access; uint8_t *data; bool(*on_read)(...); bool(*on_write)(...);}`
    - `uds_did_register(entry)` — add DID to registry
    - `uds_did_read(did, buf, *len, *nrc)` — read DID value
    - `uds_did_write(did, data, len, *nrc)` — write DID (if writable)
    - `uds_did_find(did)` — look up DID in table
    - Default DIDs: 0xF190 (VIN, 17 bytes), 0xF186 (ECU serial number), 0xF187 (system supplier ECU software number), 0xF18C (system supplier ECU software version number)
    - `uds_did_configurator_t` — config struct for pre-populating default DIDs
  - **REFACTOR**: Optimize DID lookup (linear search for simplicity, hash for large tables optional)

  **Must NOT do**:
  - Don't implement service dispatch (handlers in Tasks 9, 10, 12)
  - Don't implement NVM persistence (storage is RAM + optional NVM callback)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: N/A (data registry, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 3 (with Tasks 9-16)
  - **Blocks**: Tasks 10, 12, 18
  - **Blocked By**: Task 3

  **Acceptance Criteria**:
  - [ ] DIDs can be registered with read/write access flags
  - [ ] Read returns correct data for registered DIDs
  - [ ] Write updates data for registered writable DIDs
  - [ ] Unknown DID returns error (NRC 0x31)
  - [ ] Security check for restricted DIDs
  - [ ] Default DIDs (VIN, serial, etc.) pre-registered

  **QA Scenarios**:
  ```
  Scenario: Register and read DID
    Tool: Bash
    Preconditions: DID registry initialized
    Steps:
      1. uds_did_register(0xF190, 17, READ_ONLY, data="WBA123456789...")
      2. uds_did_read(0xF190, buf, &len)
      3. Assert len == 17, buf matches VIN data
    Expected Result: DID read correctly
    Evidence: .sisyphus/evidence/task-17-did-read.txt

  Scenario: Unknown DID returns NRC
    Tool: Bash
    Preconditions: DID registry without DID 0xFFFF
    Steps:
      1. uds_did_read(0xFFFF, buf, &len, &nrc)
      2. Assert nrc == 0x31 (ROOR)
    Expected Result: NRC 0x31
    Evidence: .sisyphus/evidence/task-17-did-unknown.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement DID registry with read/write access and default DIDs`
  - Files: `src/uds/uds_data.c`, `inc/uds/uds_data.h`

---

- [x] 18. Master Service Dispatch (UDS Request Router)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for:
    - Route each known SID to correct service handler
    - Unknown SID → NRC 0x11 (SNS)
    - Service not supported in active session → NRC 0x7F (SNSIAS)
    - Message length validation per service before dispatch
    - SuppressPosRspMsgIndicationBit handling per Tables 4-7
    - Functional addressing NRC suppression rules
    - NRC 0x78 (RCRRP) to defer response when busy
  - **GREEN**: Create `src/uds/uds_service.c` + `inc/uds/uds_service.h`:
    - `uds_service_handler_t` typedef: function pointer `bool(*handler)(uds_request_t*, uds_response_t*)`
    - `uds_service_register(sid, handler, session_mask)` — register handler with session availability
    - `uds_service_dispatch(uds_request_t *req, uds_response_t *rsp)` — main dispatch entry point
    - `uds_service_init()` — register all built-in service handlers
    - **Note**: SID 0x84 (SecuredDataTransmission) is registered as a stub handler that returns NRC 0x33 (securityAccessDenied) — the SID is known but the security sub-layer is not implemented per "Must NOT Have: No security sub-layer". This is more correct than NRC 0x11 because 0x84 IS a known service identifier.
    - Dispatch logic per ISO 14229-1 Section 8.7:
      1. Check message length ≥ 1
      2. Look up SID in service table
      3. If not found: NRC 0x11 (physical) or suppress (functional)
      4. If found but not active session: NRC 0x7F
      5. Call handler → parse response
      6. Apply SPRMIB logic per Tables 4-7
      7. Apply functional addressing NRC suppression
  - **REFACTOR**: Move service handler table to separate .def file for easy extension

  **Must NOT do**:
  - Don't re-implement service-specific logic (delegate to Tasks 9-14, 19)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (dispatch/routing design, embedded C)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 4 (with Tasks 19, 20, 21)
  - **Blocks**: Task 22
  - **Blocked By**: Tasks 9, 10, 11, 12, 13, 14, 15, 16, 17

  **Acceptance Criteria**:
  - [ ] Known SID → dispatched to correct handler
  - [ ] Unknown SID → NRC 0x11 (physical), suppressed (functional)
  - [ ] Session mismatch → NRC 0x7F
  - [ ] Message too short → NRC 0x13 before dispatch
  - [ ] SPRMIB correctly suppresses positive responses per Tables 4-7
  - [ ] Functional addressing suppresses SNS/SFNS/ROOR per spec

  **QA Scenarios**:
  ```
  Scenario: Dispatch known service
    Tool: Bash
    Preconditions: All service handlers registered
    Steps:
      1. Send request: {0x10, 0x03} (DiagnosticSessionControl, extended)
      2. Assert handler called correctly
      3. Assert response contains 0x50
    Expected Result: Dispatched to correct handler
    Evidence: .sisyphus/evidence/task-18-dispatch-ok.txt

  Scenario: Unknown SID
    Tool: Bash
    Preconditions: Service dispatch initialized
    Steps:
      1. Send request: {0xFF} (unsupported SID)
      2. Assert NRC == 0x11 (SNS)
    Expected Result: NRC 0x11
    Evidence: .sisyphus/evidence/task-18-dispatch-unknown.txt

  Scenario: Functional addressing suppresses SNS
    Tool: Bash
    Preconditions: TA_type = FUNCTIONAL
    Steps:
      1. Send request: {0xFF} with functional addressing
      2. Assert no response (suppressed per Table 7, case e)
    Expected Result: No response sent
    Evidence: .sisyphus/evidence/task-18-dispatch-func-supress.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement master service dispatch with Section 8.7 compliance`
  - Files: `src/uds/uds_service.c`, `inc/uds/uds_service.h`

---

- [x] 19. Authentication Service (0x29) Framework

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for:
    - Request authentication configuration → return supported concepts
    - deAuthenticate → clear authentication state
    - Unknown/unsupported authentication task → NRC 0x31
    - Security check → NRC 0x34 (authenticationRequired)
  - **GREEN**: Create framework in `src/uds/uds_svc_diagcomm.c` already, or add `uds_svc_auth.c`:
    - 0x29 handler dispatches to authentication sub-tasks
    - SubFunction enum: `{AUTH_CONFIG=0x01, DE_AUTHENTICATE=0x02, VERIFY_CERT_UNI=0x11, VERIFY_CERT_BI=0x12, PROOF_OF_OWNERSHIP=0x13, TRANSMIT_CERT=0x14}`
    - Stub implementations for PKI methods (return NRC 0x34)
    - `deAuthenticate` returns success immediately
    - `authenticationConfiguration` returns manufacturer-specific info
    - Authentication state tracking (none/pending/authenticated)
  - **REFACTOR**: Prepare extension points for real PKI integration

  **Must NOT do**:
  - Don't implement actual PKI certificate verification
  - Don't implement crypto algorithms

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: N/A (framework design, security concepts)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 4 (with Tasks 18, 20, 21)
  - **Blocks**: Task 22
  - **Blocked By**: Tasks 6, 16 (security state context)

  **Acceptance Criteria**:
  - [ ] 0x29 returns authenticationConfiguration for SubFunction 0x01
  - [ ] deAuthenticate (0x02) returns success
  - [ ] PKI operations return NRC 0x34 (authenticationRequired — stub)
  - [ ] Framework integrates with security module

  **QA Scenarios**:
  ```
  Scenario: Request authentication configuration
    Tool: Bash
    Preconditions: Auth module initialized
    Steps:
      1. Request: SID=0x29, SF=0x01
      2. Assert response SID = 0x69
      3. Assert response contains configuration data
    Expected Result: Configuration returned
    Evidence: .sisyphus/evidence/task-19-auth-config.txt

  Scenario: Unsupported authentication task
    Tool: Bash
    Preconditions: Auth module initialized
    Steps:
      1. Request: SID=0x29, SF=0x11 (verifyCertificateUnidirectional)
      2. Assert NRC == 0x34 (authenticationRequired — stub)
    Expected Result: NRC 0x34
    Evidence: .sisyphus/evidence/task-19-auth-stub.txt
  ```

  **Commit**: YES
  - Message: `feat(uds): implement authentication service 0x29 framework with PKI stubs`
  - Files: `src/uds/uds_svc_auth.c`, `inc/uds/uds_svc_auth.h`

---

- [x] 20. LIN Master Simulator for PC Test Harness

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write tests for LIN master simulator:
    - Master sends diagnostic request frame
    - Master receives diagnostic response frame
    - Master handles timeout (no response from slave)
    - Master schedules frame transmission at correct timing (LIN schedule table)
  - **GREEN**: Create `test/lin_sim/lin_master_sim.c` + `test/lin_sim/lin_master_sim.h`:
    - `lin_sim_init(nad, baudrate)` — initialize master simulator
    - `lin_sim_send_request(nad, data, len)` — format and send LIN diagnostic request frame
    - `lin_sim_receive_response(buf, *len, timeout_ms)` — wait for response frame
    - `lin_sim_set_slave_callback(on_frame_sent)` — hook to inspect what slave sends
    - Integration with mock UART (inject TX frames, capture RX frames)
    - Timing simulation (LIN frame duration = 8*10/baud for each byte)
    - Schedule table: master alternately polls requests, receives responses
  - **REFACTOR**: Add support for multi-frame (FF/CF) transport

  **Must NOT do**:
  - Don't implement actual LIN hardware interface

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: N/A (test harness, LIN protocol, embedded test)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 4 (with Tasks 18, 19, 21)
  - **Blocks**: Task 22
  - **Blocked By**: Tasks 5, 8

  **Acceptance Criteria**:
  - [ ] Master simulator sends diagnostic request via mock UART
  - [ ] Master simulator receives and parses response
  - [ ] Timeout detection works correctly
  - [ ] Multi-frame support for long messages

  **QA Scenarios**:
  ```
  Scenario: LIN master sends request and receives response
    Tool: Bash
    Preconditions: Master + Slave instances connected via mock UART
    Steps:
      1. lin_sim_send_request(0x01, {0x10, 0x03}, 2)
      2. lin_sim_receive_response(buf, &len, 1000)
      3. Assert len > 0, buf[0] == 0x50 (response SID)
    Expected Result: Roundtrip successful
    Evidence: .sisyphus/evidence/task-20-sim-roundtrip.txt
  ```

  **Commit**: YES
  - Message: `test(sim): add LIN master simulator for PC test harness`
  - Files: `test/lin_sim/lin_master_sim.c`, `test/lin_sim/lin_master_sim.h`

---

- [x] 21. PC Simulation Main + Integration Test Runner

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write skeleton simulation main that compiles but does nothing useful yet
  - **GREEN**: Create `sim/main_sim.c` + `sim/sim_cfg.h`:
    - Initialize all UDS modules (session, security, DTC, DID, service dispatch)
    - Initialize mock HAL (UART, timer, NVM)
    - Initialize LIN transport layer
    - Main loop: poll for LIN frames → UDS dispatch → send response
    - Optional: interactive mode (user types UDS request → see response)
    - Integration test runner: batch sequence of UDS request/response scenarios
    - `sim_cfg.h`: configurable NAD, baud rate, P2 timers, DID defaults
  - **REFACTOR**: Add command-line argument support for test scenarios

  **Must NOT do**:
  - Don't implement real-time scheduling (simulation is sequential)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: N/A (integration, C simulation)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Wave**: 4 (with Tasks 18, 19, 20)
  - **Blocks**: Task 22
  - **Blocked By**: Tasks 7, 8, 15, 16, 17, 18

  **Acceptance Criteria**:
  - [ ] Simulation runs without crashes
  - [ ] Processes UDS requests end-to-end (LIN RX → parse → dispatch → serialized response → LIN TX)
  - [ ] Integration test runner executes all scenarios with pass/fail reporting
  - [ ] All unit tests pass via CTest

  **QA Scenarios**:
  ```
  Scenario: Simulation processes DiagnosticSessionControl
    Tool: Bash
    Preconditions: Simulator built, UDS modules initialized
    Steps:
      1. Inject LIN frame: NAD=0x01, PCI=SF, data={0x10, 0x03}
      2. Run simulator main loop one iteration
      3. Capture TX frame from mock UART
      4. Assert TX contains response: {0x01, 0x06, 0x50, 0x03, P2, P2*}
    Expected Result: Full processing chain works
    Evidence: .sisyphus/evidence/task-21-sim-e2e.txt
  ```

  **Commit**: YES
  - Message: `feat(sim): add PC simulation main and integration test runner`
  - Files: `sim/main_sim.c`, `sim/sim_cfg.h`

---

- [x] 22. Integration E2E Tests (Full UDS Conversation Scenarios)

  **What to do** (TDD: RED → GREEN → REFACTOR):
  - **RED**: Write integration test scenarios that verify end-to-end UDS conversations via LIN transport
  - **GREEN**: Create `test/test_integration.c` with scenarios:
    1. **Full diagnostic session lifecycle**: 0x10 → 0x27 (unlock) → 0x22 (read DID) → 0x2E (write DID) → 0x3E (tester present) → 0x10 default (close)
    2. **DTC lifecycle**: Set fault → confirm DTC → read with 0x19 → clear with 0x14 → verify cleared
    3. **Upload/Download**: 0x34 → 0x36 (multiple blocks) → 0x37
    4. **Routine control**: 0x31 start → verify status
    5. **Security rejection**: Request secured service without unlock → NRC 0x33
    6. **Functional addressing**: Broadcast request, NRC suppression
    7. **SPRMIB test**: Request with suppress=true, verify no response
    8. **Error handling**: Unknown SID, wrong length, invalid params
    9. **LIN transport**: Multi-frame (FF/CF) for long UDS messages
    10. **Session timeout**: P2*Server timeout handling
  - Use LIN master simulator to send requests and verify responses
  - Register all scenarios with CTest

  **Must NOT do**:
  - Don't create new services — integration only

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: N/A (integration testing, UDS protocol, C)

  **Parallelization**:
  - **Can Run In Parallel**: YES (sequential within Wave 4 — after Tasks 18, 20, 21)
  - **Wave**: 4 (after Tasks 18, 20, 21 complete)
  - **Blocks**: Task F1-F4
  - **Blocked By**: Tasks 18, 20, 21

  **Acceptance Criteria**:
  - [ ] All 10 integration scenarios pass
  - [ ] Full diagnostic session lifecycle verified
  - [ ] LIN multi-frame transport tested (FF + CF sequence)
  - [ ] NRC error paths covered
  - [ ] `ctest` reports all tests passed

  **QA Scenarios**:
  ```
  Scenario: Full diagnostic session lifecycle
    Tool: Bash
    Preconditions: Integration test binary built
    Steps:
      1. Run test_integration via CTest
      2. Assert test: DiagnosticSessionControl → SecurityAccess → ReadDataByIdentifier → TesterPresent → Return to default
    Expected Result: All steps succeed, correct SIDs and NRCs
    Evidence: .sisyphus/evidence/task-22-integration-lifecycle.txt

  Scenario: LIN multi-frame transport
    Tool: Bash
    Preconditions: Integration test binary built
    Steps:
      1. Read DID with >6 bytes data (triggers FF/CF)
      2. Assert response correctly reassembled
    Expected Result: Multi-frame works
    Evidence: .sisyphus/evidence/task-22-integration-multiframe.txt
  ```

  **Commit**: YES
  - Message: `test(integration): add end-to-end UDS integration tests with LIN transport`
  - Files: `test/test_integration.c`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, compile check, run test). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Build the project: `mkdir -p build && cd build && cmake .. && cmake --build .`. Run `ctest --output-on-failure`. Review all changed files for: `as any`/`@ts-ignore` (N/A for C), `// TODO` or `// FIXME` without ticket number, empty catch blocks (empty if/else/macros), `printf` in production code, commented-out code, unused variables/functions. Check AI slop: excessive comments, over-abstraction, generic names (data/result/item/temp).
  Output: `Build [PASS/FAIL] | Lint [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from clean state (`git checkout -- . && rm -rf build`). Execute EVERY QA scenario from EVERY task — follow exact steps, capture evidence. Test cross-task integration: send UDS diagnostic session request via LIN simulator → receive response → verify SID and NRC correctness. Test edge cases: empty state, invalid input, rapid requests. Save to `.sisyphus/evidence/final-qa/`.
  Output: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (`git log/diff`). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance. Detect cross-task contamination: Task N touching Task M's files. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

| Task(s) | Commit Message | Scope |
|---------|---------------|-------|
| 1 | `build(project): scaffold CMake project with Unity test framework` | CMake + dirs |
| 2 | `feat(hal): define HAL abstraction interfaces for UART, Timer, NVM` | inc/hal/* |
| 3 | `feat(uds): define core types, SIDs, NRCs, and message structures` | inc/uds/uds_core.h |
| 4 | `feat(lin): define LIN transport types, PCI encoding, and frame structures` | inc/uds/uds_lin_transport.h |
| 5 | `test(mock): add mock HAL implementations for PC simulation testing` | test/mock/* |
| 6 | `feat(uds): implement PDU parser/serializer core protocol engine` | src/uds/uds_core.* |
| 7 | `feat(uds): implement session state machine with Figure 7 transition rules` | src/uds/uds_session.* |
| 8 | `feat(lin): implement LIN transport adapter with SF/FF/CF frame handling` | src/uds/uds_lin_transport.* |
| 9 | `feat(uds): implement diagnostic and communication management services` | src/uds/uds_svc_diagcomm.* |
| 10 | `feat(uds): implement data transmission services (0x22/23/24/2A/2C/2E/3D)` | src/uds/uds_svc_data.* |
| 11 | `feat(uds): implement stored data services (0x14 ClearDTC, 0x19 ReadDTCInfo)` | src/uds/uds_svc_stored.* |
| 12 | `feat(uds): implement IO control service (0x2F)` | src/uds/uds_svc_io.* |
| 13 | `feat(uds): implement routine control service (0x31)` | src/uds/uds_svc_routine.* |
| 14 | `feat(uds): implement upload/download services (0x34/35/36/37/38)` | src/uds/uds_svc_upload.* |
| 15 | `feat(uds): implement DTC state machine engine with 8-bit status tracking` | src/uds/uds_dtc.* |
| 16 | `feat(uds): implement security access state machine per Annex I` | src/uds/uds_security.* |
| 17 | `feat(uds): implement DID registry with read/write access and default DIDs` | src/uds/uds_data.* |
| 18 | `feat(uds): implement master service dispatch with Section 8.7 compliance` | src/uds/uds_service.* |
| 19 | `feat(uds): implement authentication service 0x29 framework with PKI stubs` | src/uds/uds_svc_auth.* |
| 20 | `test(sim): add LIN master simulator for PC test harness` | test/lin_sim/* |
| 21 | `feat(sim): add PC simulation main and integration test runner` | sim/* |
| 22 | `test(integration): add end-to-end UDS integration tests with LIN transport` | test/test_integration.c |

---

## Success Criteria

### Verification Commands
```bash
mkdir -p build && cd build && cmake .. -G "Unix Makefiles" && cmake --build . && ctest --output-on-failure
# Expected: 100% tests passed, 0 failures
```

### Final Checklist
- [ ] All 22 engineering TODOs completed and committed
- [ ] All F1-F4 reviews passed with VERDICT: APPROVE
- [ ] `cmake --build .` compiles without errors or warnings
- [ ] `ctest` reports 100% tests passed
- [ ] All "Must Have" requirements verified present
- [ ] All "Must NOT Have" restrictions verified absent
- [ ] Evidence files exist in `.sisyphus/evidence/` for all QA scenarios
- [ ] PC simulation runs end-to-end UDS conversation without errors
- [ ] User has given explicit "okay" after final review presentation

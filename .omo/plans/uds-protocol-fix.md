# UDS-on-LIN Protocol Conformance Fix

## TL;DR

> **Quick Summary**: Fix 11 protocol conformance defects in the UDS driver — LIN timeout values, STmin encoding, FF test, security key, PKI/file-transfer stubs, and documentation gaps — aligned to ISO 17987-2 / ISO 14229-7, verified by extended Unity tests.
> 
> **Deliverables**:
> - 4 changed source files (`uds_lin_transport.c/.h`, `uds_security.c`, `uds_svc_auth.c`, `uds_svc_upload.c`)
> - 1 new conversion function (`lin_stmin_to_ms`)
> - 1 changed test file (`test_lin_transport.c`)
> - 1 new doc file (`doc/README.md`)
> 
> **Estimated Effort**: Short
> **Parallel Execution**: YES — 2 waves
> **Critical Path**: Task 1 → Task 3 → Task 9

---

## Context

### Original Request
Fix protocol conformance issues identified during a comprehensive audit of the UDS+LIN driver against ISO 17987-2 / ISO 14229-7.

### Interview Summary
**Key Discussions**:
- Timing target values: Industry reference (N_As=50ms, N_Ar=50ms, N_Bs=150ms, N_Br=150ms, N_Cr=150ms)
- Scope: All 11 items (7 original + 4 from Metis review)
- Test strategy: Tests-after — fix existing tests + add new timing/STmin tests
- Security key: Replace trivial complement with CRC-8 placeholder
- PKI auth: Add IMLOIF validation only, no full PKI implementation

**Metis Supplementary Findings**:
- Timeout comparison `>` → `>=` at line 597
- N_Ar defined in header but missing from runtime `g_timeout_ms[]` array
- N_Br also needs scaling (1500→150ms, in runtime array)
- `last_activity` conflation (max(tx,rx) for all timeout types) — deferred to follow-up

### Metis Review
**Identified Gaps** (addressed):
- N_Ar dead code → Add to runtime array (Task 1)
- N_Br scaling scope gap → Included in Task 1
- Timeout comparison operator → Task 3
- PKI stub IMLOIF → Task 6
- `last_activity` conflation → Explicitly excluded, documented in Task 1 comments

---

## Work Objectives

### Core Objective
Fix UDS-on-LIN protocol conformance defects in the UDS driver to align with ISO 17987-2 / ISO 14229-7, verified by extended TDD tests.

### Concrete Deliverables
- `inc/uds/uds_lin_transport.h` — Updated timeout #defines, new STmin API declaration
- `src/uds/uds_lin_transport.c` — STmin conversion function, `>=` fix, N_Ar in runtime array, `last_activity` comments
- `src/uds/uds_security.c` — CRC-8 default key validation
- `src/uds/uds_svc_auth.c` — IMLOIF validation in PKI stubs
- `src/uds/uds_svc_upload.c` — Improved NRC codes in RequestFileTransfer stub
- `test/test_lin_transport.c` — Fixed FF test, new STmin + timeout tests
- `doc/README.md` — Protocol document inventory noting missing ISO 17987-2

### Definition of Done
- [ ] `gcc -c src/uds/uds_lin_transport.c -Iinc` compiles clean
- [ ] `gcc -c src/uds/uds_security.c -Iinc` compiles clean  
- [ ] `gcc test/test_lin_transport.c src/uds/uds_lin_transport.c -Iinc -o test_runner && ./test_runner` → ALL TESTS PASS
- [ ] All 11 requirements verified by agent-executed QA

### Must Have
- All 5 timeout #defines updated (N_As, N_Ar, N_Bs, N_Br, N_Cr)
- N_Ar added to `g_timeout_ms[]` runtime array
- STmin microsecond encoding function (`lin_stmin_to_ms`)
- Timeout comparison `>` → `>=`
- FF boundary test corrected
- Security key validation not trivial complement
- PKI stubs validate IMLOIF
- RequestFileTransfer stub improved NRC codes

### Must NOT Have (Guardrails)
- Do NOT implement full PKI handshake logic (NRCs 0x50-0x5D remain defined but unused)
- Do NOT restructure `last_activity` conflation (deferred to follow-up)
- Do NOT change the pluggable security callback architecture
- Do NOT add new SIDs or expand the service dispatch table
- Do NOT create new `.c`/`.h` files beyond STmin conversion (which stays in existing transport files)

---

## Verification Strategy

> **ALL verification is agent-executed. No human intervention required.**

### Test Decision
- **Infrastructure exists**: YES (Unity framework, `test/test_lin_transport.c`)
- **Automated tests**: Tests-after
- **Framework**: Unity (via `local_unity/`)
- **Format**: Fix existing + add new test cases per task

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **API/Backend**: Use Bash (gcc compile + run test runner)
- **Code review**: Use Bash (grep for patterns)

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately — constants + infrastructure + security):
├── Task 1: Fix LIN timeout #defines + N_Ar runtime array [quick]
├── Task 2: Add STmin microsecond encoding [quick]
├── Task 3: Fix timeout comparison > → >= [quick]
├── Task 4: Fix FF boundary test [quick]
├── Task 5: Replace security key validation [quick]
├── Task 6: PKI stub IMLOIF validation [quick]
└── Task 7: RequestFileTransfer stub improved NRC [quick]

Wave 2 (After Wave 1 — tests + docs):
├── Task 8: Add STmin encoding test cases [quick]
├── Task 9: Add LIN timeout value test cases [quick]
├── Task 10: Create doc/README protocol inventory [writing]
└── Task 11: Run full test suite + final QA [quick]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high)
└── Task F4: Scope fidelity check (deep)
```

**Critical Path**: Task 1 → Task 9 → Task 11 → F1-F4
**Parallel Speedup**: ~60% faster than sequential (2 waves vs 11 sequential tasks)
**Max Concurrent**: 7 (Wave 1)

---

## TODOs

- [x] 1. Fix LIN timeout #defines + add N_Ar to runtime array

  **What to do**:
  - In `inc/uds/uds_lin_transport.h` lines 116-129: change 5 `#define` values:
    - `LIN_TIMEOUT_N_AS` 1000 → 50
    - `LIN_TIMEOUT_N_AR` 1000 → 50
    - `LIN_TIMEOUT_N_BS` 1500 → 150
    - `LIN_TIMEOUT_N_BR` 1500 → 150
    - `LIN_TIMEOUT_N_CR` 1500 → 150
  - In `src/uds/uds_lin_transport.c` lines 102-107: expand `g_timeout_ms[4]` to `g_timeout_ms[5]`, adding `LIN_TIMEOUT_N_AR` at index 4
  - In `src/uds/uds_lin_transport.c` line 588: update guard from `timeout_type >= 4` to `timeout_type >= 5` (now 5 entries)
  - Add comment above `g_timeout_ms[]` noting: `last_activity` conflation is a known limitation (uses max(tx,rx) for all timeout types), deferred to follow-up refactor
  - Update any inline comments referencing old timeout values

  **Must NOT do**:
  - Do NOT restructure the timeout checking logic beyond the `>` → `>=` fix (that's Task 3)
  - Do NOT add N_Ar slot without verifying existing callers pass correct timeout_type indexes
  - Do NOT change `LIN_DEFAULT_STMIN` or `LIN_DEFAULT_BS`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple constant changes in 2 files, no logic restructuring
  - **Skills**: `[]`
  - **Skills Evaluated but Omitted**: None needed — pure constant modifications

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 2, 3, 4, 5, 6, 7)
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 9 (timeout tests depend on updated values)
  - **Blocked By**: None (can start immediately)

  **References**:
  - `inc/uds/uds_lin_transport.h:116-129` — 5 timeout #defines to modify
  - `src/uds/uds_lin_transport.c:102-107` — runtime `g_timeout_ms[]` array to expand
  - `src/uds/uds_lin_transport.c:586-598` — `lin_transport_check_timeout()` usage of `g_timeout_ms[]`

  **Acceptance Criteria**:
  - [ ] All 5 `#define` values changed to target values (50, 50, 150, 150, 150)
  - [ ] `g_timeout_ms[]` expanded from 4 to 5 entries, N_Ar at index 4
  - [ ] Array guard at line 588 updated from `>= 4` to `>= 5`
  - [ ] `gcc -E inc/uds/uds_lin_transport.h -Iinc | grep TIMEOUT` shows new values
  - [ ] File compiles: `gcc -c src/uds/uds_lin_transport.c -Iinc`

  **QA Scenarios**:

  ```
  Scenario: All timeout defines report correct values after change
    Tool: Bash (gcc preprocessor + grep)
    Preconditions: File edited with new values
    Steps:
      1. Run: gcc -E inc/uds/uds_lin_transport.h -Iinc 2>/dev/null | grep "LIN_TIMEOUT_N_"
      2. Assert output contains: LIN_TIMEOUT_N_AS 50, LIN_TIMEOUT_N_AR 50, LIN_TIMEOUT_N_BS 150, LIN_TIMEOUT_N_BR 150, LIN_TIMEOUT_N_CR 150
      3. Assert no line contains: 1000 or 1500
    Expected Result: All 5 timeout values report target values, no old values present
    Failure Indicators: Any old value (1000/1500) remains in preprocessor output
    Evidence: .sisyphus/evidence/task-1-timeout-defines.txt

  Scenario: Runtime array has 5 entries with N_Ar present
    Tool: Bash (grep)
    Preconditions: File edited
    Steps:
      1. Run: grep -A 6 "g_timeout_ms\[" src/uds/uds_lin_transport.c
      2. Assert output shows array declaration with 5 entries
      3. Assert entry 4 is LIN_TIMEOUT_N_AR
    Expected Result: g_timeout_ms[5] with N_As, N_Bs, N_Br, N_Cr, N_Ar in order
    Failure Indicators: Array still size [4], or N_Ar missing
    Evidence: .sisyphus/evidence/task-1-runtime-array.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-1-timeout-defines.txt` — grep output of preprocessor
  - [ ] `task-1-runtime-array.txt` — grep of array declaration

  **Commit**: YES (groups with Task 3)
  - Message: `fix(transport): align LIN timeout values with ISO 17987-2 (N_As=50, N_Bs=150, N_Cr=150)`
  - Files: `inc/uds/uds_lin_transport.h`, `src/uds/uds_lin_transport.c`
  - Pre-commit: `gcc -c src/uds/uds_lin_transport.c -Iinc`

- [x] 2. Add STmin microsecond encoding support

  **What to do**:
  - In `src/uds/uds_lin_transport.c`: add `lin_stmin_to_ms()` static function:
    ```c
    static uint16_t lin_stmin_to_ms(uint8_t stmin_raw) {
        if (stmin_raw <= 0x7F) return (uint16_t)stmin_raw;          // 0-127 ms
        if (stmin_raw >= 0xF1 && stmin_raw <= 0xF9) return (uint16_t)(stmin_raw - 0xF0) * 100 / 1000; // 100-900 us → ms (rounded down)
        return 0; // reserved values → 0 ms
    }
    ```
  - In `lin_rx_decode()` FC handler (line ~374): replace `g_rx_fc_params.stmin = frame->data[3]` with `g_rx_fc_params.stmin = lin_stmin_to_ms(frame->data[3])`
  - In `lin_tx_encode_fc()` (line ~454): apply conversion before writing to frame
  - Add `@note` docstring explaining dual encoding per ISO 17987-2
  - In `inc/uds/uds_lin_transport.h`: add `lin_stmin_to_ms()` declaration (public API)

  **Must NOT do**:
  - Do NOT change the `lin_fc_params_t.stmin` type (stays `uint8_t` — now represents interpreted ms value after conversion)
  - Do NOT modify the STmin default value (`LIN_DEFAULT_STMIN = 10`)
  - Do NOT add runtime STmin enforcement (LIN slave doesn't schedule frames)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single function addition + 2 call-site changes in one file
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 3, 4, 5, 6, 7)
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 8 (STmin tests)
  - **Blocked By**: None

  **References**:
  - `src/uds/uds_lin_transport.c:374` — FC receive: `g_rx_fc_params.stmin = frame->data[3]`
  - `src/uds/uds_lin_transport.c:454` — FC send: `frame->data[3] = params->stmin`
  - `inc/uds/uds_lin_transport.h:106-110` — `lin_fc_params_t` struct definition
  - `inc/uds/uds_lin_transport.h:276` — existing docstring mentioning "0xF1-0xF9 = 100-900us"

  **Acceptance Criteria**:
  - [ ] `lin_stmin_to_ms(0x7F)` returns 127
  - [ ] `lin_stmin_to_ms(0xF1)` returns 0 (100µs → 0ms, integer truncated)
  - [ ] `lin_stmin_to_ms(0xF5)` returns 0 (500µs → 0ms)
  - [ ] `lin_stmin_to_ms(0xF9)` returns 0 (900µs → 0ms)
  - [ ] `lin_stmin_to_ms(0x80)` returns 0 (reserved → safe default)
  - [ ] FC receive path uses converted STmin value
  - [ ] FC transmit path writes raw byte (reverse mapping not needed — caller provides already-interpreted STmin)

  **QA Scenarios**:

  ```
  Scenario: STmin microsecond values decode correctly
    Tool: Bash (gcc compile + test driver)
    Preconditions: Compilation environment with -Iinc
    Steps:
      1. Create inline test: echo '#include "uds/uds_lin_transport.h" ... main() { assert(lin_stmin_to_ms(0xF1)==0); ... }' | gcc -x c - -Iinc -o /tmp/stmin_test
      2. Run /tmp/stmin_test
      3. Assert exit code 0
    Expected Result: All test assertions pass, microsecond values decode to ms
    Failure Indicators: Non-zero exit, or wrong return values
    Evidence: .sisyphus/evidence/task-2-stmin-conversion.txt

  Scenario: File compiles after changes
    Tool: Bash
    Steps:
      1. gcc -c src/uds/uds_lin_transport.c -Iinc -o /tmp/lin_transport.o
      2. Assert exit code 0, no warnings
    Expected Result: Clean compilation
    Evidence: .sisyphus/evidence/task-2-compile.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-2-stmin-conversion.txt` — test driver output
  - [ ] `task-2-compile.txt` — compilation log

  **Commit**: YES (groups with Task 1/3)
  - Message: `feat(transport): add STmin microsecond encoding support per ISO 17987-2`
  - Files: `inc/uds/uds_lin_transport.h`, `src/uds/uds_lin_transport.c`

- [x] 3. Fix timeout comparison `>` → `>=`

  **What to do**:
  - In `src/uds/uds_lin_transport.c` line 597: change `(current_time - last_activity) > timeout_ms` to `(current_time - last_activity) >= timeout_ms`
  - This ensures timeout fires at the exact boundary value, per ISO 17987-2 strict conformance

  **Must NOT do**:
  - Do NOT modify `last_activity` calculation (max(tx,rx)) — known limitation, deferred
  - Do NOT change the function signature

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single character change in one line

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 2, 4, 5, 6, 7)
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 9 (timeout tests verify boundary behavior)
  - **Blocked By**: None

  **References**:
  - `src/uds/uds_lin_transport.c:594-598` — `lin_transport_check_timeout()` function

  **Acceptance Criteria**:
  - [ ] Line 597 uses `>=` not `>`
  - [ ] File compiles: `gcc -c src/uds/uds_lin_transport.c -Iinc`

  **QA Scenarios**:

  ```
  Scenario: Timeout comparison operator is >=
    Tool: Bash (grep)
    Steps:
      1. grep -n ">= timeout_ms" src/uds/uds_lin_transport.c
      2. Assert exactly one match at line 597
      3. grep -n "> timeout_ms" src/uds/uds_lin_transport.c
      4. Assert zero matches
    Expected Result: Only >= present, no bare > at the timeout comparison
    Failure Indicators: > timeout_ms still exists at line 597
    Evidence: .sisyphus/evidence/task-3-comparison.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-3-comparison.txt` — grep output confirming `>=`

  **Commit**: YES (groups with Task 1)
  - Files: `src/uds/uds_lin_transport.c`

- [x] 4. Fix FF boundary test path

  **What to do**:
  - In `test/test_lin_transport.c` function `test_ff_exceeds_max_length` (lines 492-508):
    - Fix the confusing comment at line 498: remove `"wrong?"` and clarify that 0x0F|FF with 0xFF = 4095 is valid FF
    - Fix the test at line 502-503: `PCI=0x10|FF` with `data[2]=0x00` gives `0x00 << 8 | 0x00 = 0`, which triggers `total_len < 7` not `total_len > 4095`
    - Instead: use `PCI=LIN_PCI_FF | 0x0F` with `data[2]=0xFF` which = 4095 (boundary-valid), then test `PCI=LIN_PCI_FF` with `data[2]=0x00` and data[1] having 0x10 in high nibble = 4096 (exceeds max)
    - Actually simplify: test clearly that 4096 bytes FF is rejected, with explicit values and clear comments
    - Ensure the test assertion `TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status)` remains valid after fix

  **Must NOT do**:
  - Do NOT change the existing test assertion or expected error code
  - Do NOT delete or skip the test — just fix the test path and comments

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Test file cleanup — comment fixes + clarifying test values

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 2, 3, 5, 6, 7)
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 11 (final test suite run)
  - **Blocked By**: None

  **References**:
  - `test/test_lin_transport.c:492-508` — `test_ff_exceeds_max_length()` function
  - `src/uds/uds_lin_transport.c:276-280` — FF length extraction logic: `total_len = (pci & 0x0F) << 8 | data[2]`

  **Acceptance Criteria**:
  - [ ] Test function has clear, accurate comments (no `"wrong?"` or similar)
  - [ ] Test uses explicit FF values that clearly demonstrate the 4096 boundary
  - [ ] `TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status)` passes

  **QA Scenarios**:

  ```
  Scenario: FF boundary test has correct comments and logic
    Tool: Bash (grep + compile)
    Steps:
      1. grep -c "wrong?" test/test_lin_transport.c → assert 0
      2. grep -n "test_ff_exceeds_max_length" test/test_lin_transport.c → confirm function exists
      3. gcc test/test_lin_transport.c src/uds/uds_lin_transport.c -Iinc -Ilocal_unity/src -o /tmp/ff_test
      4. /tmp/ff_test → assert test_ff_exceeds_max_length PASS
    Expected Result: Test compiles and passes with clean comments
    Failure Indicators: "wrong?" still present, or test fails
    Evidence: .sisyphus/evidence/task-4-ff-test.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-4-ff-test.txt` — compilation + run output

  **Commit**: YES (groups with Task 8/9)
  - Message: `test(transport): fix FF boundary test comments and clarify 4096 rejection path`
  - Files: `test/test_lin_transport.c`
  - Pre-commit: `gcc test/test_lin_transport.c src/uds/uds_lin_transport.c -Iinc -Ilocal_unity/src -o /tmp/ff_test && /tmp/ff_test`

- [x] 5. Replace default security key validation

  **What to do**:
  - In `src/uds/uds_security.c` function `default_key_validate` (line ~158):
    - Replace `return (key[0] == (uint8_t)(~seed[0]))` with a CRC-8 based validation
    - CRC-8 polynomial: x^8 + x^2 + x + 1 (0x07), initial value 0x00
    - Algorithm: CRC-8 over the 8-byte seed, compare with key[0] (simplified single-byte check)
    - Add comment: "This is a weak default for testing. Production MUST install a cryptographically sound callback via `uds_security_set_key_validate_cb()`"
  - Add helper `static uint8_t crc8(const uint8_t *data, uint8_t len)` above `default_key_validate`
  - Update function docstring to describe CRC-8 approach
  - No header changes needed (internal function)

  **Must NOT do**:
  - Do NOT change the pluggable callback architecture (`uds_security_set_key_validate_cb` stays intact)
  - Do NOT implement AES, SHA, or any real cryptographic algorithm
  - Do NOT modify the LFSR seed generator

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Replace one trivial function body + add small CRC helper
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 2, 3, 4, 6, 7)
  - **Parallel Group**: Wave 1
  - **Blocks**: None (security callbacks are pluggable, no downstream test dependency)
  - **Blocked By**: None

  **References**:
  - `src/uds/uds_security.c:158-174` — `default_key_validate()` current implementation
  - `src/uds/uds_security.c:109` — `security_key_validate_t validate_cb` typedef
  - `src/uds/uds_security.c:196` — callback registration in `uds_security_init()`
  - CRC-8 reference: polynomial 0x07, init 0x00 (standard CRC-8/ITU)

  **Acceptance Criteria**:
  - [ ] `default_key_validate` no longer uses trivial complement
  - [ ] CRC-8 function computes correctly for known test vectors (e.g., `[0x00,0x00,...]` → CRC=0x00)
  - [ ] Security module compiles: `gcc -c src/uds/uds_security.c -Iinc`
  - [ ] Existing test `test/test_security.c` still passes (check if test patches callback)

  **QA Scenarios**:

  ```
  Scenario: CRC-8 validation replaces trivial complement
    Tool: Bash (grep)
    Steps:
      1. grep -c "~seed" src/uds/uds_security.c → assert 0 (no complement operator on seed)
      2. grep -n "crc8" src/uds/uds_security.c → assert function defined
      3. grep -n "CRC-8" src/uds/uds_security.c → assert comment explains approach
    Expected Result: No complement-based key check, CRC-8 function present
    Failure Indicators: Complement pattern still found, or CRC function missing
    Evidence: .sisyphus/evidence/task-5-security-key.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-5-security-key.txt` — grep results + compile log

  **Commit**: YES
  - Message: `refactor(security): replace trivial complement key check with CRC-8 validation`
  - Files: `src/uds/uds_security.c`
  - Pre-commit: `gcc -c src/uds/uds_security.c -Iinc`

- [x] 6. Add IMLOIF validation to PKI auth stubs

  **What to do**:
  - In `src/uds/uds_svc_auth.c` function `handle_pki_stub` (currently `(void)req`):
    - Remove `(void)req` — actually inspect the request
    - Validate minimum message length per PKI subfunction:
      - 0x11 (verifyCertificateUnidirectional): min 3 bytes (subfunction + 2-byte certificate client ID)
      - 0x12 (verifyCertificateBidirectional): min 3 bytes
      - 0x13 (proofOfOwnership): min 3 bytes
      - 0x14 (transmitCertificate): min 3 bytes
    - If `data_len < 3`: return NRC 0x13 (IMLOIF) instead of NRC 0x34
    - If length ok: keep returning NRC 0x34 (authenticationRequired — correct for stub)
  - Update function comment explaining what's validated vs stubbed

  **Must NOT do**:
  - Do NOT implement any actual PKI handshake logic
  - Do NOT use PKI NRCs 0x50-0x5D (they remain defined but unused)
  - Do NOT change the auth state machine

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Add length check to existing stub function, no new logic

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 2, 3, 4, 5, 7)
  - **Parallel Group**: Wave 1
  - **Blocks**: None (standalone improvement)
  - **Blocked By**: None

  **References**:
  - `src/uds/uds_svc_auth.c` — `handle_pki_stub()` function (find via grep for `(void)req`)
  - `inc/uds/uds_core.h:88-163` — NRC enum (0x13 = IMLOIF, 0x34 = authenticationRequired)
  - ISO 14229-1 Annex G — PKI subfunction minimum payloads (2-byte certificate client ID minimum)

  **Acceptance Criteria**:
  - [ ] `handle_pki_stub` checks `req->data_len` before returning NRC
  - [ ] Length < 3 → NRC 0x13 (incorrectMessageLengthOrInvalidFormat)
  - [ ] Length >= 3 → NRC 0x34 (authenticationRequired, existing behavior)
  - [ ] File compiles: `gcc -c src/uds/uds_svc_auth.c -Iinc`

  **QA Scenarios**:

  ```
  Scenario: PKI stub validates minimum message length
    Tool: Bash (grep)
    Steps:
      1. grep -n "data_len" src/uds/uds_svc_auth.c → assert found in handle_pki_stub context
      2. grep -c "(void)req" src/uds/uds_svc_auth.c → assert 0 (no more ignored params)
      3. grep -n "IMLOIF\|0x13" src/uds/uds_svc_auth.c → assert referenced near PKI length check
    Expected Result: IMLOIF validation present, (void)req removed
    Failure Indicators: (void)req still present, or no IMLOIF check
    Evidence: .sisyphus/evidence/task-6-pki-stub.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-6-pki-stub.txt` — grep results + compile log

  **Commit**: YES
  - Message: `fix(auth): add IMLOIF validation to PKI authentication stubs`
  - Files: `src/uds/uds_svc_auth.c`
  - Pre-commit: `gcc -c src/uds/uds_svc_auth.c -Iinc`

- [x] 7. Improve RequestFileTransfer stub error codes

  **What to do**:
  - In `src/uds/uds_svc_upload.c` function handling SID 0x38 (RequestFileTransfer):
    - Current: always returns NRC 0x31 (requestOutOfRange) after guards pass
    - Improve: differentiate error codes based on the actual rejection reason:
      - If `req->data_len < 2`: NRC 0x13 (IMLOIF — incorrect message length)
      - Default/unsupported modeOfOperation: NRC 0x31 (requestOutOfRange — keep existing)
      - If filePathAndName not valid format: NRC 0x31
    - Ensure existing session/security/subfunction guards execute BEFORE reaching the stub NRC
    - Add comment: "Stub — full implementation requires file system abstraction layer"

  **Must NOT do**:
  - Do NOT implement any file transfer logic
  - Do NOT add new subfunction handlers
  - Do NOT change the existing guard chain (session check, security check, subfunction dispatch)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Enhance existing stub with proper length validation
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 1, 2, 3, 4, 5, 6)
  - **Parallel Group**: Wave 1
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `src/uds/uds_svc_upload.c` — RequestFileTransfer handler (search for 0x38 or `request_file_transfer`)
  - `inc/uds/uds_core.h:88-163` — NRC enum (0x13=IMLOIF, 0x31=ROOR)
  - `inc/uds/uds_svc_upload.h` — handler declarations

  **Acceptance Criteria**:
  - [ ] RequestFileTransfer stub validates `data_len >= 2`
  - [ ] Short message → NRC 0x13 (IMLOIF)
  - [ ] Valid length → NRC 0x31 (ROOR) with improved comment
  - [ ] File compiles: `gcc -c src/uds/uds_svc_upload.c -Iinc`

  **QA Scenarios**:

  ```
  Scenario: RequestFileTransfer stub has improved NRC codes
    Tool: Bash (grep)
    Steps:
      1. grep -n "NRC_REQUEST_OUT_OF_RANGE\|0x31" src/uds/uds_svc_upload.c → assert still present (valid length path)
      2. grep -n "NRC_INCORRECT_MESSAGE_LENGTH\|0x13" src/uds/uds_svc_upload.c → assert present near 0x38 handler
      3. grep -c "Stub" src/uds/uds_svc_upload.c → assert at least 1 stub comment near 0x38
    Expected Result: IMLOIF check added, ROOR preserved, stub documented
    Failure Indicators: No IMLOIF reference, or no stub comment
    Evidence: .sisyphus/evidence/task-7-filetransfer-stub.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-7-filetransfer-stub.txt` — grep results + compile log

  **Commit**: YES
  - Message: `fix(upload): improve RequestFileTransfer stub with IMLOIF validation and documentation`
  - Files: `src/uds/uds_svc_upload.c`
  - Pre-commit: `gcc -c src/uds/uds_svc_upload.c -Iinc`

- [x] 8. Add STmin encoding test cases

  **What to do**:
  - In `test/test_lin_transport.c`: add 3 new Unity test functions:
    - `test_stmin_ms_values`: verify `lin_stmin_to_ms(0)`=0, `lin_stmin_to_ms(1)`=1, `lin_stmin_to_ms(127)`=127
    - `test_stmin_us_values`: verify `lin_stmin_to_ms(0xF1)`=0, `lin_stmin_to_ms(0xF5)`=0, `lin_stmin_to_ms(0xF9)`=0 (all <1ms → 0ms with integer arithmetic)
    - `test_stmin_reserved_values`: verify `lin_stmin_to_ms(0x80)`=0, `lin_stmin_to_ms(0xF0)`=0, `lin_stmin_to_ms(0xFA)`=0
  - Register all 3 in `main()` with `RUN_TEST()`
  - Include `uds/uds_lin_transport.h` if not already included

  **Must NOT do**:
  - Do NOT test floating-point STmin values (the API returns `uint16_t` ms)
  - Do NOT remove or rename existing test functions

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Add standard test functions following existing Unity patterns

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 9, 10)
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 11 (final test suite run)
  - **Blocked By**: Task 2 (STmin conversion function must exist first)

  **References**:
  - `test/test_lin_transport.c:513-550` — `main()` function with `RUN_TEST()` registration pattern
  - `test/test_lin_transport.c:1-50` — existing test function structure to mimic
  - `inc/uds/uds_lin_transport.h` — `lin_stmin_to_ms()` declaration (added in Task 2)

  **Acceptance Criteria**:
  - [ ] 3 new test functions added and registered in `main()`
  - [ ] All STmin tests pass: `gcc test/test_lin_transport.c src/uds/uds_lin_transport.c -Iinc -Ilocal_unity/src -o /tmp/stmin_test && /tmp/stmin_test`

  **QA Scenarios**:

  ```
  Scenario: STmin tests all pass after compilation
    Tool: Bash (compile + run)
    Preconditions: Task 2 completed (lin_stmin_to_ms function exists)
    Steps:
      1. gcc test/test_lin_transport.c src/uds/uds_lin_transport.c -Iinc -Ilocal_unity/src -o /tmp/stmin_runner 2>&1
      2. Assert compilation exit code 0
      3. /tmp/stmin_runner
      4. Assert exit code 0 (all tests pass)
      5. Assert output contains "test_stmin_ms_values:PASS", "test_stmin_us_values:PASS", "test_stmin_reserved_values:PASS"
    Expected Result: 3 new STmin tests compile and pass
    Failure Indicators: Compilation errors, or test failures, or missing test registration
    Evidence: .sisyphus/evidence/task-8-stmin-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-8-stmin-tests.txt` — full test runner output

  **Commit**: YES (groups with Task 4/9)
  - Files: `test/test_lin_transport.c`

- [x] 9. Add LIN timeout value test cases

  **What to do**:
  - In `test/test_lin_transport.c`: add 2 new Unity test functions:
    - `test_timeout_values_updated`: verify the 5 #define values equal target values:
      - `TEST_ASSERT_EQUAL(50, LIN_TIMEOUT_N_AS)`
      - `TEST_ASSERT_EQUAL(50, LIN_TIMEOUT_N_AR)`
      - `TEST_ASSERT_EQUAL(150, LIN_TIMEOUT_N_BS)`
      - `TEST_ASSERT_EQUAL(150, LIN_TIMEOUT_N_BR)`
      - `TEST_ASSERT_EQUAL(150, LIN_TIMEOUT_N_CR)`
    - `test_timeout_comparison_boundary`: verify `>=` fires at exact timeout:
      - Set `g_last_rx_time = 0` via `lin_transport_record_rx_time(0)` (if exposed)
      - Or: call `lin_transport_check_timeout(type, timeout_ms)` with `current_time = timeout_ms` and assert true
  - Register both in `main()` with `RUN_TEST()`

  **Must NOT do**:
  - Do NOT test internal static variables directly — use public API only
  - Do NOT mock the system timer — use the existing `lin_transport_record_*_time` and `lin_transport_check_timeout` API

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Assert-style tests on constants + public API boundary check

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 8, 10)
  - **Parallel Group**: Wave 2
  - **Blocks**: Task 11 (final test suite run)
  - **Blocked By**: Tasks 1, 3 (updated values + >= fix must exist first)

  **References**:
  - `test/test_lin_transport.c:513-550` — `main()` with `RUN_TEST()` registration pattern
  - `inc/uds/uds_lin_transport.h:288-302` — `lin_transport_check_timeout()` and `lin_transport_record_*_time()` APIs
  - `inc/uds/uds_lin_transport.h:116-129` — timeout #defines to assert against

  **Acceptance Criteria**:
  - [ ] 2 new test functions added and registered
  - [ ] All timeout tests pass

  **QA Scenarios**:

  ```
  Scenario: Timeout tests all pass
    Tool: Bash (compile + run)
    Preconditions: Tasks 1 and 3 completed
    Steps:
      1. gcc test/test_lin_transport.c src/uds/uds_lin_transport.c -Iinc -Ilocal_unity/src -o /tmp/timeout_runner 2>&1
      2. Assert compilation exit code 0
      3. /tmp/timeout_runner
      4. Assert exit code 0
      5. Assert output contains "test_timeout_values_updated:PASS", "test_timeout_comparison_boundary:PASS"
    Expected Result: Timeout tests compile and pass
    Failure Indicators: Compilation errors, test failures
    Evidence: .sisyphus/evidence/task-9-timeout-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-9-timeout-tests.txt` — full test runner output

  **Commit**: YES (groups with Task 4/8)
  - Message: `test(transport): add LIN timeout value and boundary comparison tests`
  - Files: `test/test_lin_transport.c`
  - Pre-commit: `gcc test/test_lin_transport.c src/uds/uds_lin_transport.c -Iinc -Ilocal_unity/src -o /tmp/timeout_test && /tmp/timeout_test`

- [x] 10. Create doc/README protocol document inventory

  **What to do**:
  - Create `doc/README.md` with:
    ```markdown
    # Protocol Documents Inventory

    ## Present
    | Document | Standard | Year | Format |
    |----------|----------|------|--------|
    | ISO 14229-1 | UDS Application Layer | 2020/2006/2004 | PDF + TXT |
    | ISO 14229-2 | UDS Session Layer | 2013 | PDF + TXT |
    | ISO 14229-3 | UDS on CAN | 2012 | PDF + TXT |
    | ISO 14229-4 | UDS on FlexRay | 2012 | PDF + TXT |
    | ISO 14229-5 | UDS on IP | 2013 | PDF + TXT |
    | ISO 14229-6 | UDS on K-Line | 2013 | PDF + TXT |
    | ISO 14229-7 | UDS on LIN (adaptation layer) | 2015 | PDF + TXT |
    | ISO 14229-8 | UDS on Clock Extension | 2020 | PDF + TXT |

    ## Missing
    | Document | Standard | Why Needed |
    |----------|----------|-----------|
    | **ISO 17987-2** | LIN Transport Protocol & Network Layer | Defines N_PCI formats (SF/FF/CF/FC), N_As/N_Bs/N_Cr timing, STmin encoding, transport state machine — all referenced by ISO 14229-7 |
    ```
  - Add a note that the TXT files are damaged PDF extractions and may have missing tables/figures

  **Must NOT do**:
  - Do NOT create any other documentation files
  - Do NOT add implementation commentary — inventory only

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Pure documentation task — markdown table creation

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 8, 9)
  - **Parallel Group**: Wave 2
  - **Blocks**: None
  - **Blocked By**: None

  **References**:
  - `doc/` — existing protocol document files to inventory
  - `dataflow.md` — existing project documentation for style reference

  **Acceptance Criteria**:
  - [ ] `doc/README.md` exists
  - [ ] Marks ISO 17987-2 as missing
  - [ ] Lists all 8 ISO 14229 documents as present
  - [ ] Notes TXT extraction quality

  **QA Scenarios**:

  ```
  Scenario: README.md exists with correct inventory
    Tool: Bash (grep)
    Steps:
      1. test -f doc/README.md → assert exists
      2. grep -c "ISO 17987-2" doc/README.md → assert >= 1
      3. grep -c "Missing" doc/README.md → assert >= 1
      4. grep -c "ISO 14229-1" doc/README.md → assert >= 1
      5. grep -c "ISO 14229-7" doc/README.md → assert >= 1
    Expected Result: README documents all present specs and flags missing one
    Failure Indicators: File missing, or no mention of ISO 17987-2
    Evidence: .sisyphus/evidence/task-10-doc-readme.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-10-doc-readme.txt` — grep output + file existence check

  **Commit**: YES
  - Message: `docs: add protocol document inventory noting missing ISO 17987-2`
  - Files: `doc/README.md`

- [x] 11. Run full test suite + final QA

  **What to do**:
  - Compile and run ALL existing tests in `test/test_lin_transport.c` including new ones from Tasks 4, 8, 9
  - Verify all 18 original + 5 new tests pass (23 total)
  - Check for regressions: verify existing test names still appear in output as PASS
  - Check compilation of all modified source files together:
    - `gcc -c src/uds/uds_lin_transport.c -Iinc`
    - `gcc -c src/uds/uds_security.c -Iinc`
    - `gcc -c src/uds/uds_svc_auth.c -Iinc`
    - `gcc -c src/uds/uds_svc_upload.c -Iinc`
    - All 4 compile clean without errors

  **Must NOT do**:
  - Do NOT run tests from other test files (only `test_lin_transport.c` in scope)
  - Do NOT add new test infrastructure files

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Compilation + test runner execution — mechanical verification

  **Parallelization**:
  - **Can Run In Parallel**: NO (depends on ALL Wave 1 + Wave 2 tasks)
  - **Parallel Group**: Wave 2 (sequential after Tasks 8, 9, 10)
  - **Blocks**: FINAL wave (F1-F4)
  - **Blocked By**: Tasks 1-9

  **References**:
  - `test/test_lin_transport.c` — complete test suite (after all edits)
  - `test/test_lin_transport.c:513-550` — `main()` with all `RUN_TEST()` registrations

  **Acceptance Criteria**:
  - [ ] All 23 tests pass (0 failures)
  - [ ] All 4 source files compile clean
  - [ ] No regression in original 18 test names

  **QA Scenarios**:

  ```
  Scenario: Full test suite passes
    Tool: Bash (compile + run)
    Steps:
      1. gcc test/test_lin_transport.c src/uds/uds_lin_transport.c src/uds/uds_security.c src/uds/uds_svc_auth.c src/uds/uds_svc_upload.c -Iinc -Ilocal_unity/src -o /tmp/full_runner 2>&1
      2. Assert compilation exit code 0
      3. /tmp/full_runner 2>&1 | tee /tmp/full_output.txt
      4. Assert runner exit code 0
      5. grep "FAIL" /tmp/full_output.txt → assert no matches
      6. grep -c "PASS" /tmp/full_output.txt → assert >= 23
    Expected Result: 23 tests pass, 0 failures, all source files compile
    Failure Indicators: Any FAIL line, non-zero exit, or compilation error
    Evidence: .sisyphus/evidence/task-11-full-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-11-full-tests.txt` — complete test runner output

  **Commit**: NO (verification only, no new code changes)

---

## Final Verification Wave

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, grep for constants). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan.
  Output: `Must Have [11/11] | Must NOT Have [5/5] | Tasks [11/11] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `gcc -c` for all 4 modified source files + compile test runner. Check all modified files for: `as any`/`@ts-ignore` (N/A for C), empty catches, commented-out code, unused imports. Check AI slop: excessive comments, over-abstraction. Review STmin function for correct boundary handling.
  Output: `Build [PASS/FAIL] | Sources [4/4] | Tests [23/23] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Execute EVERY QA scenario from EVERY task — follow exact steps, capture evidence. Test cross-task integration: run full test suite after all changes applied. Verify timeout values via preprocessor, STmin function via test driver, FF test via runner.
  Output: `Scenarios [11/11 pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (git diff). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance. Detect cross-task contamination: Task N touching Task M's files. Flag unaccounted changes.
  Output: `Tasks [11/11 compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **Wave 1**: `fix(transport): align LIN timeout values with ISO 17987-2 (N_As=50, N_Bs=150, N_Cr=150)` — `inc/uds/uds_lin_transport.h`, `src/uds/uds_lin_transport.c`, `test/test_lin_transport.c`
- **Wave 2**: `feat(transport): add STmin microsecond encoding per ISO 17987-2` — `inc/uds/uds_lin_transport.h`, `src/uds/uds_lin_transport.c`
- **Individual**: `refactor(security): replace trivial complement key check with CRC-8` — `src/uds/uds_security.c`
- **Individual**: `fix(auth): add IMLOIF validation to PKI stubs` — `src/uds/uds_svc_auth.c`
- **Individual**: `fix(upload): improve RequestFileTransfer stub error codes` — `src/uds/uds_svc_upload.c`
- **Individual**: `test(transport): add STmin and timeout value tests` — `test/test_lin_transport.c`
- **Individual**: `docs: add protocol document inventory noting missing ISO 17987-2` — `doc/README.md`

---

## Success Criteria

### Verification Commands
```bash
# Preprocessor check: timeout values
gcc -E inc/uds/uds_lin_transport.h -Iinc 2>/dev/null | grep "LIN_TIMEOUT_N_"

# Compilation check: all modified sources
gcc -c src/uds/uds_lin_transport.c -Iinc && \
gcc -c src/uds/uds_security.c -Iinc && \
gcc -c src/uds/uds_svc_auth.c -Iinc && \
gcc -c src/uds/uds_svc_upload.c -Iinc

# Full test suite
gcc test/test_lin_transport.c src/uds/uds_lin_transport.c src/uds/uds_security.c \
    src/uds/uds_svc_auth.c src/uds/uds_svc_upload.c \
    -Iinc -Ilocal_unity/src -o /tmp/full_runner && /tmp/full_runner
```

### Final Checklist
- [ ] All 5 timeout #defines at target values (50, 50, 150, 150, 150)
- [ ] N_Ar in runtime array (5 entries total)
- [ ] STmin microsecond encoding function present and correct
- [ ] Timeout comparison uses `>=`
- [ ] FF boundary test has clear comments and correct logic
- [ ] Security key not trivial complement
- [ ] PKI stubs validate IMLOIF
- [ ] RequestFileTransfer stub has improved NRC codes
- [ ] doc/README.md exists and flags missing ISO 17987-2
- [ ] All 23 tests pass (0 failures)
- [ ] All "Must NOT Have" patterns absent

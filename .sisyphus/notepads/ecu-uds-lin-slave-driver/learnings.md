
## 2026-05-14 — Wave 2 Task 8: LIN Transport Adapter (ISO 17987-3 Frame Encode/Decode)

### Completed
- Extended `inc/uds/uds_lin_transport.h` with 3 function declarations:
  - `lin_tx_encode()` — encode UDS PDU into 1+ LIN frames (SF/FF+CF)
  - `lin_rx_decode()` — decode single LIN frame into UDS PDU (SF or multi-frame)
  - `lin_transport_reset()` — clear transport state
- Created `src/uds/uds_lin_transport.c` — full LIN transport implementation with:
  - SF encoding/decoding (UDS payload ≤ 6 bytes → 1 frame)
  - FF+CF multi-frame encoding/decoding (UDS payload 7-4095 bytes)
  - Static transport state machine (`g_tx_state`, `g_rx_state`)
  - Static reassembly buffer (4095 bytes, `LIN_FF_MAX_LEN`)
  - NAD filtering in `lin_rx_decode` (checks against `g_expected_nad = LIN_NAD_DEFAULT`)
  - PCI validation (type checks, SF length ≤ 6, FF length ≥ 7, CF sequence numbers)
  - NULL/zero-length input validation
- Created `test/test_lin_transport.c` with 18 Unity TDD tests covering all requirements
- Updated `CMakeLists.txt` — added `uds_lin_transport.c` to `uds-core` lib + `uds-lin-transport-test` target

### Design Decisions

1. **PCI encoding scheme**:
   - SF: `PCI = 0x00 | data_len` (0x00 mask + 5-bit length, 1-6)
   - FF: `PCI = 0x20 | (length >> 8 & 0x0F)`, byte[2] = `length & 0xFF` (12-bit total length)
   - CF: `PCI = 0x40 | seq_num` (sequence 1-15, rolls over)
   - Payload placement: SF data at byte[2..7], FF data at byte[3..7] (5 bytes), CF data at byte[2..7] (6 bytes)

2. **State machine reset**: `lin_transport_reset()` zeros the reassembly buffer via `memset`, resets all sequence counters to 0, and sets both TX/RX states to IDLE. This is important for security (no stale data) and correctness (CF without prior FF properly rejected).

3. **NAD filtering**: Uses `g_expected_nad` static variable initialized to `LIN_NAD_DEFAULT` (0x01). Every `lin_rx_decode()` call checks `frame->data[0]` against it and returns `LIN_NAD_MISMATCH` if different. No setter function (current design uses compile-time default; can be added later via `lin_transport_set_nad()` if needed).

4. **Multi-frame receive state machine**: FF → CF(s) → complete:
   - FF stores total length, copies first 5 payload bytes to reassembly buffer, transitions to `LIN_RX_CF`
   - Each CF validates sequence (expects 1,2,3...15,1,2...), appends up to 6 bytes
   - When `g_rx_buffer_len >= g_rx_expected_len`, state returns to `LIN_RX_IDLE`
   - Invalid transitions (CF without FF, wrong sequence, nested FF) return `LIN_PCI_ERROR`

5. **Error code choice**: Used `LIN_PCI_ERROR` for all protocol/invalid-parameter errors (NULL pointers, zero-length, invalid PCI). This is the closest match in the existing `lin_status_t` enum since there's no dedicated `LIN_INVALID_PARAM` code.

6. **Error code return for NULL/zero**: NULL pointers (pdu, frames, frame_count) and zero-length data return `LIN_PCI_ERROR` from both encode and decode functions.

### Verification
- Build: `cmake --build build` → PASS (0 warnings, `-Wall -Werror`)
- Tests: `ctest --test-dir build --output-on-failure` → 18/18 PASS (all 3 suites = 37/37 total)
- Lin-transport tests:
  1. `test_sf_encode` — 4-byte payload → 1 frame, correct PCI
  2. `test_sf_encode_max_len` — 6-byte payload → max SF
  3. `test_sf_encode_min_len` — 1-byte payload → min SF
  4. `test_nad_mismatch` — wrong NAD → `LIN_NAD_MISMATCH`
  5. `test_decode_valid_sf` — NAD, PCI, data correctly extracted
  6. `test_decode_sf_zero_len` — 0-length payload accepted
  7. `test_multi_frame_encode` — 10 bytes → FF + CF (2 frames)
  8. `test_multi_frame_encode_three_frames` — 12 bytes → FF + 2×CF (3 frames)
  9. `test_multi_frame_decode` — FF then CF, 10 bytes reassembled correctly
  10. `test_invalid_pci_type` — PCI type 0x60 → `LIN_PCI_ERROR`
  11. `test_sf_invalid_length` — SF with len 7 → `LIN_PCI_ERROR`
  12. `test_ff_invalid_length` — FF with len 5 (≤ SF max) → `LIN_PCI_ERROR`
  13. `test_cf_without_ff` — CF without prior FF → `LIN_PCI_ERROR`
  14. `test_cf_wrong_sequence` — expected seq 1, got 3 → `LIN_PCI_ERROR`
  15. `test_ff_exceeds_max_length` — FF len 4096 > 4095 → `LIN_PCI_ERROR`
  16. `test_null_inputs` — 5 NULL pointer combinations → `LIN_PCI_ERROR`
  17. `test_zero_length_encode` — data_len=0 → `LIN_PCI_ERROR`
  18. `test_reset_clears_state` — partial receive then reset, CF rejected

### Gotchas
- Stack-allocated `lin_frame_t frames[N]` in tests contains garbage. When testing unwritten bytes for zero, must zero-initialize (`lin_frame_t frames[8] = {{{0}}}`) to avoid flaky tests.
- The FF 12-bit length encoding uses PCI bits 3:0 (not 4:0) for the high nibble, which means `LIN_PCI_LEN_MASK` (0x1F) cannot be used for FF length extraction — use a separate `0x0F` mask instead.
- CF sequence numbers are 1-15 (not 0-15), rolling over. A modulo-15 calculation with `(last_seq % 15) + 1` correctly handles the wrap from 15 back to 1.

## 2026-05-14 — Wave 3b Task 15: DTC State Machine Engine

### Completed
- Created `inc/uds/uds_dtc.h` — DTC engine header with:
  - `uds_dtc_status_t` enum (8 status bit flags per ISO 14229-1 Annex D)
  - `uds_dtc_record_t` struct (dtc, status, snapshot[8], extended[8], lengths)
  - `UDS_DTC_MAX` = 32 entries for static database
  - 10 function declarations (init, set_status, get_status, get_by_status_mask, clear, get_snapshot, get_extended, get_count, set_snapshot, set_extended)
- Created `src/uds/uds_dtc.c` — implementation with:
  - Static `dtc_db[UDS_DTC_MAX]` array, zero-initialized (dtc==0 marks empty slot)
  - `find_dtc_index()` / `find_or_create_dtc()` internal helpers
  - Group-based clearing using upper nibble (bits 23:20) of 24-bit DTC value
  - Group: 0x0=Powertrain, 0x2=Chassis, 0x4=Body, 0x6=Network; 0xFFFFFF=clear all
  - Snapshot and extended data up to 8 bytes each
  - NULL/zero-length protection on all pointer parameters
- Created `test/test_dtc.c` — 12 Unity TDD tests
- Updated `CMakeLists.txt` — added `uds_dtc.c` to `uds-core` lib + `uds-dtc-test` target

### Verification
- Build: `cmake --build build` → PASS (0 warnings, `-Wall -Werror`)
- Tests: `ctest --test-dir build -V` → 12/12 PASS (all 4 suites = 49/49 total)
- DTC tests:
  1. `test_init_clears_all_dtcs` — init after adding DTCs → count=0, all cleared
  2. `test_set_test_failed_bit` — set DTC_TEST_FAILED → status==0x01
  3. `test_set_multiple_bits` — set 3 bits → composite status correct
  4. `test_clear_by_group_powertrain` — clear group 0x0 → only powertrain DTCs cleared
  5. `test_clear_all` — 0xFFFFFF → all cleared
  6. `test_get_by_status_mask` — query by DTC_CONFIRMED → returns only confirmed DTCs
  7. `test_dtc_count_by_mask` — counts by different masks, zero mask = 0
  8. `test_snapshot_data` — set and get 4-byte snapshot, status unaffected
  9. `test_extended_data` — set and get 3-byte extended data
  10. `test_get_status_unknown_dtc` — unknown DTC → returns 0
  11. `test_clear_individual_bits` — clear one bit without affecting others
  12. `test_clear_non_existent_group` — clear group with no DTCs → no effect

### Design Decisions
- **Group determination**: DTC group is derived from the upper nibble (bits 23:20) of the 24-bit DTC value. Clear function compares this nibble against the group_mask parameter.
- **Empty slot sentinel**: `dtc.dtc == 0` marks an empty record. DTC 0x000000 is not a valid trouble code in practice.
- **Setter protection**: `uds_dtc_set_status` with `set=false` on an unknown DTC is silently ignored (no record created). With `set=true`, a new record is auto-created.
- **Snapshot/Extended setters**: Added as public API functions (`uds_dtc_set_snapshot`, `uds_dtc_set_extended`) to support testing, even though the task only explicitly listed getters. These are necessary for the "snapshot stored correctly" test case.

## 2026-05-14 — Wave 3a Task 9: Diagnostic & Communication Management Services

### Completed
- Created `inc/uds/uds_svc_diagcomm.h` — header with 8 handler declarations + event store API
- Created `src/uds/uds_svc_diagcomm.c` — implementation of all 8 service handlers:
  1. **0x10 DiagnosticSessionControl**: validates length, delegates to uds_session_switch, returns sessionParameterRecord (4 bytes: P2+P2*)
  2. **0x11 ECUReset**: supports hardReset(0x01), keyOffOnReset(0x02), softReset(0x03); returns powerDownTime=0x00
  3. **0x27 SecurityAccess**: dispatches odd/even subfunctions to requestSeed/sendKey; updates caller's unlock flag via context; returns seed data from static buffer
  4. **0x28 CommunicationControl**: validates controlType and communicationType ranges
  5. **0x3E TesterPresent**: echoes subfunction, SPRMIB suppresses positive response
  6. **0x85 ControlDTCSetting**: checks unlocked via context before allowing on/off
  7. **0x86 ResponseOnEvent**: manages static event store (max 4 events), supports start/stop/clear
  8. **0x87 LinkControl**: verifyBaudrate(3-byte ID) and transitionBaudrate(no data)
- Created `test/test_svc_diagcomm.c` — 43 Unity TDD tests covering all 8 services
- Updated `CMakeLists.txt` — added `uds_svc_diagcomm.c` to `uds-core` + `uds-diagcomm-test` target

### Design Decisions

1. **Negative response encoding via uds_response_t**: Negative responses use `rsp->sid = 0x7F` (the negative response prefix) and place the NRC as a 1-byte data payload. Since `uds_serialize_response` writes `[sid][subfunc_echo][data...]`, a negative response becomes `[0x7F][request_SID][NRC]` — exactly matching the ISO 14229-1 format. This means the dispatch layer (Task 18) can use a single serialization call without needing to special-case negatives.

2. **Static response buffers**: Three static buffers in the implementation file provide stable memory for response data pointers: `g_rsp_data_buf[4]` (short data like session params, powerDownTime, NRCs), `g_seed_buf[SECURITY_SEED_SIZE]` (SecurityAccess seed). These are safe because handlers are called sequentially (single-threaded).

3. **Context parameter polymorphism**: The `void *context` parameter is cast to the appropriate type per service:
   - 0x10: `uds_session_context_t*` — the session state machine
   - 0x27: `bool*` — unlock flag (write: set to true on successful sendKey)
   - 0x85: `bool*` — unlock flag (read: check before allowing DTC setting)
   - Others: unused (pass NULL)

4. **SPRMIB handling pattern**: Each handler checks the suppress bit ONLY for the success (positive response) path. Negative responses always return `true` (send). This follows ISO 14229-1 Section 8.7: "suppressPosRspMsgIndicationBit suppresses ONLY positive responses, NOT negative ones."

5. **Mock timer for test linking**: The test target links mock_timer.c to satisfy hal_timer_* symbol references from uds_security.c in uds-core. This follows the same pattern as the existing uds-security-test target.

### Verification
- Build: `cmake --build build` → PASS (0 warnings, `-Wall -Werror`)
- Tests: `ctest --test-dir build -V` → 43/43 PASS (all 7 suites = 148/148 total)
- Diagcomm test breakdown:
  - 0x10 DiagnosticSessionControl: 5 tests (valid switch, invalid session→0x12, IMLOIF→0x13, SPRMIB suppress, null context→0x22)
  - 0x11 ECUReset: 5 tests (hardReset, softReset, unsupported type→0x12, IMLOIF→0x13, SPRMIB suppress)
  - 0x27 SecurityAccess: 7 tests (requestSeed→seed, sendKey valid→unlock, sendKey invalid→0x35, no request→0x24, invalid level→0x12, IMLOIF no key→0x13, SPRMIB suppress)
  - 0x28 CommunicationControl: 5 tests (valid, IMLOIF→0x13, invalid controlType→0x12, invalid commType→0x31, SPRMIB suppress)
  - 0x3E TesterPresent: 4 tests (positive, SPRMIB suppress, IMLOIF→0x13, subfunction echoed)
  - 0x85 ControlDTCSetting: 6 tests (on+unlocked, off+unlocked, without unlock→0x33, invalid type→0x12, null context→0x22, SPRMIB suppress)
  - 0x86 ResponseOnEvent: 5 tests (start, stop clears, clear, invalid subfn→0x12, IMLOIF→0x13)
  - 0x87 LinkControl: 6 tests (verify, transition, unsupported→0x12, verify short→0x13, transition extra→0x13, SPRMIB suppress)
## 2026-05-14 — Wave 3a Task 12: IO Control Service (0x2F InputOutputControlByIdentifier)

### Completed
- Created `inc/uds/uds_svc_io.h` — header with:
  - `uds_io_control_mode_t` enum: RETURN_TO_STANDARD(0x01), RESET_TO_DEFAULT(0x02), FREEZE(0x03), SHORT_TERM_ADJUST(0x04)
  - `uds_svc_io_control()` handler declaration (same pattern as data services)
  - `uds_svc_io_init()` / `uds_svc_io_reset()` / `uds_svc_io_has_override()` — IO state management
  - Configuration macros: `IO_OVERRIDE_MAX` (16), `IO_OVERRIDE_VALUE_MAX` (8)
- Created `src/uds/uds_svc_io.c` — implementation with:
  - Static IO override table (`g_io_overrides[IO_OVERRIDE_MAX]`) storing per-DID override values
  - DID parsing via `UDS_REQ_DID(req)` pattern (reconstruct DID from subfunction.value + data[0])
  - Four control modes all implemented with proper length validation per mode
  - Security check: `did_requires_unlock()` checks for `DID_SECURED_READ`/`DID_SECURED_WRITE` access types
  - SPRMIB suppression handled (bit 7 of raw[1] = DID_high bit 7)
  - Negative response encoding via `set_neg_rsp()` (0x7F + request_SID + NRC)
- Created `test/test_svc_io.c` — 18 Unity TDD tests covering:
  - Positive path: returnControlToStandard(0x01), resetToDefault(0x02), freezeCurrentState(0x03), shortTermAdjustment(0x04)
  - Sequence tests: adjust→return, adjust→reset (override lifecycle)
  - NRC paths: 0x13(IMLOIF: short req, extra data on freeze/return, no param on adjust), 0x22(CNC: null context), 0x31(ROOR: unknown DID, invalid control mode), 0x33(SAD: secured DID without unlock)
  - SPRMIB suppression (DID 0xF190 raw[1]=0xF1→bit 7 set)
  - Security positive path (unlocked SECURED_READ works)
  - IO reset clears all overrides
- Updated `CMakeLists.txt` — added `uds_svc_io.c` to uds-core lib + uds-io-test target
- **Build infrastructure fix**: CMake 4.x drops function-style -D macro definitions. Added `unity_config.c`/`unity_config.h` in test/mock/ and modified `unity_internals.h` to provide forward declarations and macro aliases as actual C code instead of -D flags.

### Design Decisions

1. **Handler pattern identical to uds_svc_data.c**: Uses `set_neg_rsp()`/`set_pos_rsp()` static helpers, static response buffer `g_rsp_buf[256]`, and the `bool (*handler)(req, rsp, context)` signature. Context is `bool*` (unlocked flag).

2. **DID parsing via reconstruct_byte1**: Since the parser consumes raw[1] as a subfunction byte, IO control's DID (2 bytes at raw[1..2]) is reconstructed: `reconstruct_byte1(req)` recovers raw[1] from `subfunction.value | (suppress_rsp << 7)`. The final DID is `(raw[1] << 8) | req->data[0]`. The macro `UDS_REQ_BYTE1(req)` is used internally (instead of the uds_svc_data.h macro) to avoid dependency on that header.

3. **IO override table**: A static array of `(did, value[8], value_len, active)` entries. Created on shortTermAdjustment, cleared on returnControlToStandard/resetToDefault/io_reset. Maximum 16 concurrent overrides, each up to 8 bytes.

4. **Security model**: DIDs with `DID_SECURED_READ` or `DID_SECURED_WRITE` access flags require unlocked context for ANY IO control operation. Standard `DID_READ_WRITE` DIDs require no unlock. This matches the principle that IO control on secured data requires security access.

5. **Length validation per control mode**: Each mode has different length requirements:
   - returnToStandard/resetToDefault: exactly DID(2) + controlMode(1) = 3 raw bytes → after parser data_len==2
   - freezeCurrentState: exactly DID(2) + controlMode(1) = 3 raw bytes → after parser data_len==2
   - shortTermAdjustment: DID(2) + controlMode(1) + controlParameter(N) → after parser data_len>=3

6. **Negative responses always sent**: SPRMIB only suppresses positive responses. Negative responses (NRCs) always return true, as per ISO 14229-1 Section 8.7.

### Verification
- Build: `cmake --build build2` → PASS (0 warnings, `-Wall -Werror`)
- Tests: `uds-io-test` → 18/18 PASS
- IO test breakdown:
  1. test_2f_return_to_standard — DID=0x1234, mode=0x01 → 0x6F + DID + 0x01
  2. test_2f_reset_to_default — DID=0x1234, mode=0x02 → 0x6F + DID + 0x02
  3. test_2f_freeze_current_state — DID=0x1234 with data 0x11223344, mode=0x03 → 0x6F + DID + 0x03 + frozen data
  4. test_2f_short_term_adjust — DID=0x1234, mode=0x04, param=0xAABB → 0x6F + DID + 0x04 + AABB, override stored
  5. test_2f_adjust_then_return_to_standard — override cleared after return
  6. test_2f_adjust_then_reset_to_default — override cleared after reset
  7. test_2f_unknown_did — DID=0xFFFF → NRC 0x31
  8. test_2f_invalid_control_mode — mode=0xFF → NRC 0x31
  9. test_2f_security_denied_secured_read — SECURED_READ DID without unlock → NRC 0x33
  10. test_2f_security_denied_secured_write — SECURED_WRITE DID without unlock → NRC 0x33
  11. test_2f_imloif_short_request — only 2 raw bytes → NRC 0x13
  12. test_2f_null_context — NULL context → NRC 0x22
  13. test_2f_sprmib_suppress — suppress bit set on freezeCurrentState → returns false
  14. test_2f_freeze_extra_data — mode 0x03 with extra byte → NRC 0x13
  15. test_2f_return_extra_data — mode 0x01 with extra byte → NRC 0x13
  16. test_2f_adjust_no_param — mode 0x04 without parameter → NRC 0x13
  17. test_2f_secured_read_with_unlock — unlocked SECURED_READ DID, freeze works
  18. test_2f_io_reset_clears_all_overrides — uds_svc_io_reset() clears all entries

### Gotchas
- **CMake 4.x function-style macro handling**: CMake 4.x drops function-style -D definitions (e.g., `-DUNITY_BEGIN()=UnityBegin(__FILE__)`). This breaks Unity's configuration mechanism. Solution: define these as actual C functions in `unity_config.c` and as macros in a forced-include header `unity_config.h`. The compat module (`unity_compat.c`) needs `UNITY_PROGMEM` and `UNITY_FAILURE_DETAIL_SEPARATOR` defined before `unity.h` — these were added to `unity_internals.h` to ensure they're available across all translation units.
- **DID high byte = suppress_rsp**: For services without a subfunction (0x2F, 0x22, 0x2E), raw[1] is the first data byte whose bit 7 acts as SPRMIB. This means DIDs with high byte ≥ 0x80 (like 0xF190) have suppress_rsp=1. This is intentional per ISO 14229-1 but can be surprising in tests.

## 2026-05-14 — Wave 3c Task 11: Stored Data Services (0x14 ClearDiagnosticInformation, 0x19 ReadDTCInformation)

### Completed
- Created inc/uds/uds_svc_stored.h — header with uds_svc_stored_ctx_t context struct + 2 handler declarations
- Created src/uds/uds_svc_stored.c — implementation of:
  1. 0x14 ClearDiagnosticInformation: validates session (non-default), security (unlocked), length (3-byte groupOfDTC reconstructed from parser subfunction byte + data), delegates to uds_dtc_clear(); context uses uds_svc_stored_ctx_t with session context + unlock flag
  2. 0x19 ReadDTCInformation — 4 subfunctions:
     - 0x01 reportNumberOfDTCByStatusMask: returns count + DTCStatusAvailabilityMask
     - 0x02 reportDTCByStatusMask: returns DTC records (3-byte DTC + 1-byte status)
     - 0x06 reportDTCExtendedDataRecordByDTCNumber: snapshot (record=1) or extended (record=2); unsupported returns NRC 0x31
     - 0x0A reportSupportedDTC: returns all DTCs with status mask 0xFF
- Created test/test_svc_stored.c — 22 Unity TDD tests
- Updated CMakeLists.txt — added uds_svc_stored.c to uds-core lib + uds-stored-test target

### Design Decisions
1. Context structure for 0x14: Uses uds_svc_stored_ctx_t with void *sctx (cast to uds_session_context_t* in .c) and bool *unlocked. Follows diagcomm pattern of polymorphic void* context.
2. groupOfDTC reconstruction: Parser consumes byte[1] as subfunction. For 0x14 (no subfunction), handler reconstructs 3-byte group from subfunction.value|(suppress_rsp<<7) + data[0] + data[1].
3. ReportSupportedDTC: Uses uds_dtc_get_by_status_mask(0xFF, ...) returning all DTCs with any status. statusAvailabilityMask in response is 0xFF.
4. 0x06 record numbers: recordNumber=1 -> snapshot, recordNumber=2 -> extended, others -> NRC 0x31 (ROOR).
5. DTC group upper nibble pitfall: DTC 0x030303 has upper nibble 0x0 (powertrain), not 0x2 (chassis). For chassis tests use values like 0x230101.

### Verification
- Build: cmake --build build --target uds-stored-test -> PASS (0 warnings, -Wall -Werror)
- Tests: uds-stored-test -> 22/22 PASS
- NRC paths covered: 0x12 (invalid 0x19 subfunction), 0x13 (all IMLOIF cases), 0x22 (null context, default session), 0x31 (unsupported record number, DTC not found), 0x33 (security locked)
- SPRMIB suppression: tested for 0x19 subfunctions 0x01 and 0x02

## 2026-05-14: CMake Unity Test Framework Fix

- Vendored Unity at local_unity/src/ is a **minimal/custom build** — does NOT have standard Unity macros (UNITY_BEGIN, TEST_ASSERT_EQUAL, etc.) or output functions.
- Approach: forward-declare output functions in unity_internals.h + implement in unity_config.c
- Missing assertion macros provided via unity_config.h (force-included with -include)
- Missing function implementations (UnityAssertEqualIntArray, UnityAssertIntGreaterOrLessOrEqualNumber, UnityMessage, UnityAssertEqualMemory) provided in local_unity/compat/unity_compat.c
- --allow-multiple-definition linker flag needed because vendored unity.c has weak setUp/tearDown/main that conflict with test files
- CMake 4.x cannot pass function-style -D macros → use target_compile_options (raw) instead of target_compile_definitions
- UNITY_LINE_TYPE/UNITY_COUNTER_TYPE defaults must come before struct in unity_internals.h (fixed via -D in CMake)
- UNITY_FAILURE_DETAIL_SEPARATOR must be a -D string define (can't be a C function)

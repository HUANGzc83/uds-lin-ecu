cmake_minimum_required(VERSION 3.14)
project(uds-lin-ecu C)

# ---- C Standard (strict C11, no GNU extensions) ----
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ============================================================
# Compiler Warnings (applied per-target)
# ============================================================
macro(add_uds_warnings TARGET)
    target_compile_options(${TARGET} PRIVATE -Wall -Werror)
endmacro()

# Force-include Unity configuration for test executables
# (CMake 4.x drops function-style -D macro definitions on the command line)
macro(add_uds_test_config TARGET)
    target_compile_options(${TARGET} PRIVATE
        -include "${CMAKE_CURRENT_SOURCE_DIR}/test/mock/unity_config.h")
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test/mock)
endmacro()

# ============================================================
# Main Library Target: uds-core
# ============================================================
add_library(uds-core STATIC
    src/uds/uds_core.c
    src/uds/uds_session.c
    src/uds/uds_lin_transport.c
    src/uds/uds_dtc.c
    src/uds/uds_security.c
    src/uds/uds_data.c
    src/uds/uds_svc_diagcomm.c
    src/uds/uds_svc_data.c
    src/uds/uds_svc_stored.c
    src/uds/uds_svc_routine.c
    src/uds/uds_svc_io.c
    src/uds/uds_svc_upload.c
    src/uds/uds_svc_auth.c
    src/uds/uds_service.c
    src/hal/hal_stubs.c
)

target_include_directories(uds-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:inc>
)

add_uds_warnings(uds-core)

# ============================================================
# Unity Test Framework (vendored)
# ============================================================
# Unity v2.6.1 vendored under local_unity/
set(UNITY_ROOT      "${CMAKE_CURRENT_SOURCE_DIR}/local_unity")
set(UNITY_BUILD_DIR "${CMAKE_BINARY_DIR}/unity-build")

# Configure and build Unity as a subdirectory
add_subdirectory(${UNITY_ROOT} ${UNITY_BUILD_DIR} EXCLUDE_FROM_ALL)

# Unity expects UNITY_OUTPUT_CHAR and other output functions to be
# provided by the user.  CMake 4.x cannot pass function-style macro
# definitions (-DNAME(args)=body) on the command line, so we provide
# them as actual C functions in unity_config.c and forward declarations
# via unity_config.h (force-included via local_unity/CMakeLists.txt).
target_compile_definitions(unity PRIVATE
    UNITY_OMIT_OUTPUT_CHAR_HEADER_DECLARATION
)
target_sources(unity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/mock/unity_config.c)

# Vendored Unity (minimal build) needs type defines before the struct
# in unity_internals.h and output/assertion macros not present.
target_compile_definitions(unity PUBLIC
    UNITY_LINE_TYPE=UNITY_UINT
    UNITY_COUNTER_TYPE=UNITY_UINT
)
# All output macros (UNITY_OUTPUT_CHAR, UNITY_PRINT_EXEC_TIME,
# etc.) are provided as C functions in unity_config.c, which is linked
# into the unity target via target_sources.
# UNITY_FAILURE_DETAIL_SEPARATOR is defined in unity_internals.h to
# avoid shell quoting issues with -D on msys2.

# Vendored unity.c has weak setUp/tearDown/main stubs that conflict
# with each test file's own definitions. Allow the linker to pick the
# first definition (the test's) and ignore the weak ones from unity.
target_link_options(unity INTERFACE "-Wl,--allow-multiple-definition")

# ============================================================
# Testing Infrastructure
# ============================================================
enable_testing()

add_executable(uds-test
    test/test_core.c
)

target_link_libraries(uds-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

add_test(NAME uds-test COMMAND uds-test)

add_uds_warnings(uds-test)
add_uds_test_config(uds-test)

# ============================================================
# Session State Machine Tests (Wave 2 Task 7)
# ============================================================
add_executable(uds-session-test
    test/test_session.c
)

target_link_libraries(uds-session-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-session-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

add_test(NAME uds-session-test COMMAND uds-session-test)

add_uds_warnings(uds-session-test)
add_uds_test_config(uds-session-test)

# ============================================================
# LIN Transport Tests (Wave 2 Task 8)
# ============================================================
add_executable(uds-lin-transport-test
    test/test_lin_transport.c
)

target_link_libraries(uds-lin-transport-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-lin-transport-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

add_test(NAME uds-lin-transport-test COMMAND uds-lin-transport-test)

add_uds_warnings(uds-lin-transport-test)
add_uds_test_config(uds-lin-transport-test)

# ============================================================
# DTC State Machine Tests (Wave 3b Task 15)
# ============================================================
add_executable(uds-dtc-test
    test/test_dtc.c
)

target_link_libraries(uds-dtc-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-dtc-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

add_test(NAME uds-dtc-test COMMAND uds-dtc-test)

add_uds_warnings(uds-dtc-test)
add_uds_test_config(uds-dtc-test)

# ============================================================
# Security Access State Machine Tests (Wave 3b Task 16)
# ============================================================
# Note: This test compiles uds_security.c directly (rather than
# linking uds-core) so it can use mock_timer in place of hal_stubs
# for timer-dependent delay testing.
add_executable(uds-security-test
    test/test_security.c
    src/uds/uds_security.c
    test/mock/mock_timer.c
)

target_include_directories(uds-security-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(uds-security-test PRIVATE
    unity
)

add_test(NAME uds-security-test COMMAND uds-security-test)

add_uds_warnings(uds-security-test)
add_uds_test_config(uds-security-test)

# ============================================================
# DID Registry Tests (Wave 3b Task 17)
# ============================================================
add_executable(uds-did-test
    test/test_did.c
)

target_link_libraries(uds-did-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-did-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

add_test(NAME uds-did-test COMMAND uds-did-test)

add_uds_warnings(uds-did-test)
add_uds_test_config(uds-did-test)

# ============================================================
# Diagnostic & Communication Management Services Tests (Wave 3a Task 9)
# ============================================================
# Note: Links mock_timer to satisfy hal_timer symbols needed by uds_security.
add_executable(uds-diagcomm-test
    test/test_svc_diagcomm.c
    test/mock/mock_timer.c
)

target_link_libraries(uds-diagcomm-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-diagcomm-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-diagcomm-test COMMAND uds-diagcomm-test)

add_uds_warnings(uds-diagcomm-test)
add_uds_test_config(uds-diagcomm-test)

# ============================================================
# Data Transmission Services Tests (Wave 3a Task 10)
# ============================================================
add_executable(uds-data-service-test
    test/test_svc_data.c
)

target_link_libraries(uds-data-service-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-data-service-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-data-service-test COMMAND uds-data-service-test)

add_uds_warnings(uds-data-service-test)
add_uds_test_config(uds-data-service-test)

# ============================================================
# IO Control Service Tests (Wave 3a Task 12)
# ============================================================
add_executable(uds-io-test
    test/test_svc_io.c
)

target_link_libraries(uds-io-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-io-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-io-test COMMAND uds-io-test)

add_uds_warnings(uds-io-test)
add_uds_test_config(uds-io-test)

# ============================================================
# Upload/Download Services Tests (Wave 3c Task 14)
# ============================================================
add_executable(uds-upload-test
    test/test_svc_upload.c
    test/mock/mock_timer.c
)

target_link_libraries(uds-upload-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-upload-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-upload-test COMMAND uds-upload-test)

add_uds_warnings(uds-upload-test)
add_uds_test_config(uds-upload-test)

# ============================================================
# Stored Data Services Tests (Wave 3c Task 11)
# ============================================================
add_executable(uds-stored-test
    test/test_svc_stored.c
)

target_link_libraries(uds-stored-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-stored-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
)

add_test(NAME uds-stored-test COMMAND uds-stored-test)

add_uds_warnings(uds-stored-test)
add_uds_test_config(uds-stored-test)

# ============================================================
# Routine Control Service Tests (Wave 3b Task 13)
# ============================================================
add_executable(uds-routine-test
    test/test_svc_routine.c
)

target_link_libraries(uds-routine-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-routine-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-routine-test COMMAND uds-routine-test)

add_uds_warnings(uds-routine-test)
add_uds_test_config(uds-routine-test)

# ============================================================
# Authentication Service Tests (Wave 3c Task 19)
# ============================================================
add_executable(uds-auth-test
    test/test_svc_auth.c
)

target_link_libraries(uds-auth-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-auth-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-auth-test COMMAND uds-auth-test)

add_uds_warnings(uds-auth-test)
add_uds_test_config(uds-auth-test)

# ============================================================
# LIN Master Simulator Tests (Wave 3d Task 20)
# ============================================================
add_executable(uds-lin-sim-test
    test/test_lin_sim.c
    test/lin_sim/lin_master_sim.c
    test/mock/mock_uart.c
    test/mock/mock_timer.c
)

target_link_libraries(uds-lin-sim-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-lin-sim-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-lin-sim-test COMMAND uds-lin-sim-test)

add_uds_warnings(uds-lin-sim-test)
add_uds_test_config(uds-lin-sim-test)

# ============================================================
# Simulator Executable Target (links mock HAL for PC execution)
# ============================================================
# The uds-core library does not provide HAL implementations
# (hal_stubs.c is empty).  We link mock_uart, mock_timer,
# and mock_nvm to provide the HAL symbols on the PC.
add_executable(uds-sim
    sim/main_sim.c
    test/mock/mock_uart.c
    test/mock/mock_timer.c
    test/mock/mock_nvm.c
)

target_link_libraries(uds-sim PRIVATE
    uds-core
)

target_include_directories(uds-sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/sim
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_uds_warnings(uds-sim)

# ============================================================
# UDS Runner / Scaffolding Test
# ============================================================
add_executable(uds-runner-test
    test/test_uds_runner.c
)

target_link_libraries(uds-runner-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-runner-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

add_test(NAME uds-runner-test COMMAND uds-runner-test)

add_uds_warnings(uds-runner-test)
add_uds_test_config(uds-runner-test)

# ============================================================
# Integration Test Runner (Wave 4 Task 21)
# ============================================================
# End-to-end pipeline test: mock UART -> LIN decode -> UDS
# parse -> service dispatch -> response serialize -> LIN encode.
add_executable(uds-integration-test
    test/test_uds_integration.c
    test/mock/mock_uart.c
    test/mock/mock_timer.c
    test/mock/mock_nvm.c
)

target_link_libraries(uds-integration-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-integration-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-integration-test COMMAND uds-integration-test)

add_uds_warnings(uds-integration-test)
add_uds_test_config(uds-integration-test)

# ============================================================
# Service Dispatch Tests (Wave 3d Task 18)
# ============================================================
add_executable(uds-service-test
    test/test_svc_service.c
    test/mock/mock_timer.c
)

target_link_libraries(uds-service-test PRIVATE
    uds-core
    unity
)

target_include_directories(uds-service-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_test(NAME uds-service-test COMMAND uds-service-test)

add_uds_warnings(uds-service-test)
add_uds_test_config(uds-service-test)

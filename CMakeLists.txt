cmake_minimum_required(VERSION 3.14)
project(uds-lin-ecu C)

# ============================================================
# POSIX HAL Platform Option
# ============================================================
# Set with -DHAL_PLATFORM=posix or -DUDS_PLATFORM_POSIX=ON to
# replace hal_stubs.c with real POSIX implementations (UART via
# /dev/ttyS0, timer via clock_gettime, NVM via file I/O).
option(UDS_PLATFORM_POSIX "Build POSIX HAL implementations (UART, Timer, NVM)" OFF)

# Support HAL_PLATFORM=posix shorthand (e.g. cmake -B build -DHAL_PLATFORM=posix)
if(HAL_PLATFORM STREQUAL "posix")
    set(UDS_PLATFORM_POSIX ON)
endif()

if(UDS_PLATFORM_POSIX)
    set(HAL_BACKEND_SOURCES
        src/hal/hal_uart_posix.c
        src/hal/hal_timer_posix.c
        src/hal/hal_nvm_posix.c
    )
else()
    set(HAL_BACKEND_SOURCES src/hal/hal_stubs.c)
endif()

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
# Test Executable Macro (reduces 17x boilerplate to 1-liners)
# ============================================================
# Links uds-core and unity with --allow-multiple-definition (set on
# unity's INTERFACE).  Includes test/ and test/mock/ dirs via
# add_uds_test_config().  EXTRA_SOURCES from ARGN go to add_executable.
function(add_uds_test TEST_NAME TEST_SOURCE)
    add_executable(uds-${TEST_NAME}-test ${TEST_SOURCE} ${ARGN})
    target_link_libraries(uds-${TEST_NAME}-test PRIVATE uds-core unity)
    target_include_directories(uds-${TEST_NAME}-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test)
    add_test(NAME uds-${TEST_NAME}-test COMMAND uds-${TEST_NAME}-test)
    add_uds_warnings(uds-${TEST_NAME}-test)
    add_uds_test_config(uds-${TEST_NAME}-test)
endfunction()

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
    ${HAL_BACKEND_SOURCES}
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

add_uds_test(core test/test_core.c)

# ============================================================
# Session State Machine Tests (Wave 2 Task 7)
# ============================================================
add_uds_test(session test/test_session.c)

# ============================================================
# LIN Transport Tests (Wave 2 Task 8)
# ============================================================
add_uds_test(lin-transport test/test_lin_transport.c)

# ============================================================
# DTC State Machine Tests (Wave 3b Task 15)
# ============================================================
add_uds_test(dtc test/test_dtc.c)

# ============================================================
# Security Access State Machine Tests (Wave 3b Task 16)
# ============================================================
# Note: Compiles uds_security.c directly with mock_timer so the
# directly-compiled symbols override the uds-core equivalents via
# --allow-multiple-definition (propagated from unity's INTERFACE).
add_uds_test(security test/test_security.c src/uds/uds_security.c test/mock/mock_timer.c)

# ============================================================
# DID Registry Tests (Wave 3b Task 17)
# ============================================================
add_uds_test(did test/test_did.c)

# ============================================================
# Diagnostic & Communication Management Services Tests (Wave 3a Task 9)
# ============================================================
# Note: Links mock_timer to satisfy hal_timer symbols needed by uds_security.
add_uds_test(diagcomm test/test_svc_diagcomm.c test/mock/mock_timer.c)

# ============================================================
# Data Transmission Services Tests (Wave 3a Task 10)
# ============================================================
add_uds_test(data-service test/test_svc_data.c)

# ============================================================
# IO Control Service Tests (Wave 3a Task 12)
# ============================================================
add_uds_test(io test/test_svc_io.c)

# ============================================================
# Upload/Download Services Tests (Wave 3c Task 14)
# ============================================================
add_uds_test(upload test/test_svc_upload.c test/mock/mock_timer.c)

# ============================================================
# Stored Data Services Tests (Wave 3c Task 11)
# ============================================================
add_uds_test(stored test/test_svc_stored.c)

# ============================================================
# Routine Control Service Tests (Wave 3b Task 13)
# ============================================================
add_uds_test(routine test/test_svc_routine.c)

# ============================================================
# Authentication Service Tests (Wave 3c Task 19)
# ============================================================
add_uds_test(auth test/test_svc_auth.c)

# ============================================================
# LIN Master Simulator Tests (Wave 3d Task 20)
# ============================================================
add_uds_test(lin-sim test/test_lin_sim.c test/lin_sim/lin_master_sim.c test/mock/mock_uart.c test/mock/mock_timer.c)

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
add_uds_test(runner test/test_uds_runner.c)

# ============================================================
# Integration Test Runner (Wave 4 Task 21)
# ============================================================
# End-to-end pipeline test: mock UART -> LIN decode -> UDS
# parse -> service dispatch -> response serialize -> LIN encode.
add_uds_test(integration test/test_uds_integration.c test/mock/mock_uart.c test/mock/mock_timer.c test/mock/mock_nvm.c)

# ============================================================
# Service Dispatch Tests (Wave 3d Task 18)
# ============================================================
add_uds_test(service test/test_svc_service.c test/mock/mock_timer.c)

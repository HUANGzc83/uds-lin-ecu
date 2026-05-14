/* =========================================================================
    Unity Internals - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-26 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
========================================================================= */
#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/* PROGMEM is a no-op on desktop (PC simulation).  unity.c defines it
 * after including unity.h, but the compat module needs it earlier. */
#ifndef UNITY_PROGMEM
#define UNITY_PROGMEM
#endif

/* String separator used by Unity's FAIL assertion output.
 * Defined here instead of via -D flag because CMake 4.x + msys2
 * shell has trouble passing escaped quotes in -D definitions. */
#ifndef UNITY_FAILURE_DETAIL_SEPARATOR
#define UNITY_FAILURE_DETAIL_SEPARATOR " "
#endif

/* User-provided output function forward declarations.
 * These functions are defined in test/mock/unity_config.c and linked
 * into test executables.  Adding them here avoids implicit-declaration
 * warnings when compiling unity.c.
 * CMake 4.x cannot pass function-style -D macro definitions, so we
 * provide them as actual C functions instead of macros.
 */
#ifndef UNITY_OMIT_OUTPUT_CHAR_HEADER_DECLARATION
#define UNITY_OMIT_OUTPUT_CHAR_HEADER_DECLARATION
#endif
void UNITY_OUTPUT_CHAR(int);
void UNITY_PRINT_EXEC_TIME(void);
void UNITY_PRINT_EOL(void);
void UNITY_FLUSH_CALL(void);
void UNITY_OUTPUT_FLUSH(void);
void UNITY_OUTPUT_START(void);
void UNITY_OUTPUT_COMPLETE(void);

#if defined(__GNUC__) || defined(__clang__)
#define UNITY_FUNCTION_ATTR(a) __attribute__((a))
#else
#define UNITY_FUNCTION_ATTR(a)
#endif
#ifndef UNITY_NORETURN
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define UNITY_NORETURN _Noreturn
#else
#define UNITY_NORETURN UNITY_FUNCTION_ATTR(__noreturn__)
#endif
#endif

/* Integer width detection */
#ifndef UNITY_INT_WIDTH
#ifdef UINT_MAX
#if (UINT_MAX == 0xFFFF)
#define UNITY_INT_WIDTH (16)
#elif (UINT_MAX == 0xFFFFFFFF)
#define UNITY_INT_WIDTH (32)
#elif (UINT_MAX == 0xFFFFFFFFFFFFFFFF)
#define UNITY_INT_WIDTH (64)
#endif
#else
#define UNITY_INT_WIDTH (32)
#endif
#endif

#ifndef UNITY_LONG_WIDTH
#ifdef ULONG_MAX
#if (ULONG_MAX == 0xFFFF)
#define UNITY_LONG_WIDTH (16)
#elif (ULONG_MAX == 0xFFFFFFFF)
#define UNITY_LONG_WIDTH (32)
#elif (ULONG_MAX == 0xFFFFFFFFFFFFFFFF)
#define UNITY_LONG_WIDTH (64)
#endif
#else
#define UNITY_LONG_WIDTH (32)
#endif
#endif

#ifndef UNITY_POINTER_WIDTH
#ifdef UINTPTR_MAX
#if (UINTPTR_MAX <= 0xFFFF)
#define UNITY_POINTER_WIDTH (16)
#elif (UINTPTR_MAX <= 0xFFFFFFFF)
#define UNITY_POINTER_WIDTH (32)
#elif (UINTPTR_MAX <= 0xFFFFFFFFFFFFFFFF)
#define UNITY_POINTER_WIDTH (64)
#endif
#else
#define UNITY_POINTER_WIDTH UNITY_LONG_WIDTH
#endif
#endif

#if (UNITY_INT_WIDTH == 32)
typedef unsigned char UNITY_UINT8;
typedef unsigned short UNITY_UINT16;
typedef unsigned int UNITY_UINT32;
typedef signed char UNITY_INT8;
typedef signed short UNITY_INT16;
typedef signed int UNITY_INT32;
#else
#error Invalid UNITY_INT_WIDTH specified! (16 or 32 are supported)
#endif

#ifndef UNITY_SUPPORT_64
#if UNITY_LONG_WIDTH == 64 || UNITY_POINTER_WIDTH == 64
#define UNITY_SUPPORT_64
#endif
#endif

#ifndef UNITY_SUPPORT_64
typedef UNITY_UINT32 UNITY_UINT;
typedef UNITY_INT32 UNITY_INT;
#define UNITY_MAX_NIBBLES (8)
#else
#if (UNITY_LONG_WIDTH == 32)
typedef unsigned long long UNITY_UINT64;
typedef signed long long UNITY_INT64;
#else
typedef unsigned long UNITY_UINT64;
typedef signed long UNITY_INT64;
#endif
typedef UNITY_UINT64 UNITY_UINT;
typedef UNITY_INT64 UNITY_INT;
#define UNITY_MAX_NIBBLES (16)
#endif

#if (UNITY_POINTER_WIDTH == 32)
#define UNITY_PTR_TO_INT UNITY_INT32
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX32
#elif (UNITY_POINTER_WIDTH == 64)
#define UNITY_PTR_TO_INT UNITY_INT64
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX64
#endif

#ifndef UNITY_PTR_ATTRIBUTE
#define UNITY_PTR_ATTRIBUTE
#endif
#ifndef UNITY_INTERNAL_PTR
#define UNITY_INTERNAL_PTR UNITY_PTR_ATTRIBUTE const void*
#endif

#define UNITY_DISPLAY_RANGE_INT (0x10)
#define UNITY_DISPLAY_RANGE_UINT (0x20)
#define UNITY_DISPLAY_RANGE_HEX (0x40)
#define UNITY_DISPLAY_RANGE_CHAR (0x80)

typedef enum {
    UNITY_DISPLAY_STYLE_INT = (UNITY_INT_WIDTH / 8) + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_INT8 = 1 + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_INT16 = 2 + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_INT32 = 4 + UNITY_DISPLAY_RANGE_INT,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_INT64 = 8 + UNITY_DISPLAY_RANGE_INT,
#endif
    UNITY_DISPLAY_STYLE_UINT = (UNITY_INT_WIDTH / 8) + UNITY_DISPLAY_RANGE_UINT,
    UNITY_DISPLAY_STYLE_UINT8 = 1 + UNITY_DISPLAY_RANGE_UINT,
    UNITY_DISPLAY_STYLE_UINT16 = 2 + UNITY_DISPLAY_RANGE_UINT,
    UNITY_DISPLAY_STYLE_UINT32 = 4 + UNITY_DISPLAY_RANGE_UINT,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_UINT64 = 8 + UNITY_DISPLAY_RANGE_UINT,
#endif
    UNITY_DISPLAY_STYLE_HEX8 = 1 + UNITY_DISPLAY_RANGE_HEX,
    UNITY_DISPLAY_STYLE_HEX16 = 2 + UNITY_DISPLAY_RANGE_HEX,
    UNITY_DISPLAY_STYLE_HEX32 = 4 + UNITY_DISPLAY_RANGE_HEX,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_HEX64 = 8 + UNITY_DISPLAY_RANGE_HEX,
#endif
    UNITY_DISPLAY_STYLE_CHAR = 1 + UNITY_DISPLAY_RANGE_CHAR + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_UNKNOWN
} UNITY_DISPLAY_STYLE_T;

typedef enum {
    UNITY_EQUAL_TO = 0x1,
    UNITY_GREATER_THAN = 0x2,
    UNITY_GREATER_OR_EQUAL = 0x2 + UNITY_EQUAL_TO,
    UNITY_SMALLER_THAN = 0x4,
    UNITY_SMALLER_OR_EQUAL = 0x4 + UNITY_EQUAL_TO,
    UNITY_NOT_EQUAL = 0x8
} UNITY_COMPARISON_T;

typedef enum { UNITY_ARRAY_TO_VAL = 0, UNITY_ARRAY_TO_ARRAY, UNITY_ARRAY_UNKNOWN } UNITY_FLAGS_T;

#ifndef UNITY_EXCLUDE_DETAILS
#define UNITY_CLR_DETAILS() do { Unity.CurrentDetail1 = 0; Unity.CurrentDetail2 = 0; } while (0)
#define UNITY_SET_DETAIL(d1) do { Unity.CurrentDetail1 = (d1); Unity.CurrentDetail2 = 0; } while (0)
#define UNITY_SET_DETAILS(d1, d2) do { Unity.CurrentDetail1 = (d1); Unity.CurrentDetail2 = (d2); } while (0)
#else
#define UNITY_CLR_DETAILS()
#define UNITY_SET_DETAIL(d1)
#define UNITY_SET_DETAILS(d1, d2)
#endif

struct UNITY_STORAGE_T {
    const char* TestFile;
    const char* CurrentTestName;
#ifndef UNITY_EXCLUDE_DETAILS
    const char* CurrentDetail1;
    const char* CurrentDetail2;
#endif
    UNITY_LINE_TYPE CurrentTestLineNumber;
    UNITY_COUNTER_TYPE NumberOfTests;
    UNITY_COUNTER_TYPE TestFailures;
    UNITY_COUNTER_TYPE TestIgnores;
    UNITY_COUNTER_TYPE CurrentTestFailed;
    UNITY_COUNTER_TYPE CurrentTestIgnored;
    jmp_buf AbortFrame;
};

#ifndef UNITY_LINE_TYPE
#define UNITY_LINE_TYPE UNITY_UINT
#endif
#ifndef UNITY_COUNTER_TYPE
#define UNITY_COUNTER_TYPE UNITY_UINT
#endif

extern struct UNITY_STORAGE_T Unity;

typedef void (*UnityTestFunction)(void);

void UnityBegin(const char* filename);
int UnityEnd(void);
void UnityConcludeTest(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum);
void UnityPrint(const char* string);
void UnityPrintLen(const char* string, UNITY_UINT32 length);
void UnityPrintNumber(UNITY_INT number);
void UnityPrintNumberUnsigned(UNITY_UINT number);
void UnityPrintNumberHex(UNITY_UINT number, char nibbles);
void UnityPrintMask(UNITY_UINT mask, UNITY_UINT number);
void UnityPrintIntNumberByStyle(UNITY_INT number, UNITY_DISPLAY_STYLE_T style);
void UnityPrintUintNumberByStyle(UNITY_UINT number, UNITY_DISPLAY_STYLE_T style);
void UnityAssertEqualIntNumber(UNITY_INT expected, UNITY_INT actual, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style);
void UnityAssertEqualUintNumber(UNITY_UINT expected, UNITY_UINT actual, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style);
void UnityAssertBits(UNITY_INT mask, UNITY_INT expected, UNITY_INT actual, const char* msg, UNITY_LINE_TYPE line);
void UnityAssertEqualIntArray(UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual, UNITY_UINT32 num_elements, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style, UNITY_FLAGS_T flags);
void UnityAssertEqualString(const char* expected, const char* actual, const char* msg, UNITY_LINE_TYPE line);
void UnityAssertEqualStringLen(const char* expected, const char* actual, UNITY_UINT32 length, const char* msg, UNITY_LINE_TYPE line);
void UnityAssertEqualStringArray(UNITY_INTERNAL_PTR expected, const char** actual, UNITY_UINT32 num_elements, const char* msg, UNITY_LINE_TYPE line, UNITY_FLAGS_T flags);
void UnityAssertEqualMemory(UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual, UNITY_UINT32 length, UNITY_UINT32 num_elements, const char* msg, UNITY_LINE_TYPE line, UNITY_FLAGS_T flags);
void UnityMessage(const char* message, UNITY_LINE_TYPE line);
#ifndef UNITY_EXCLUDE_SETJMP_H
UNITY_NORETURN void UnityFail(const char* message, UNITY_LINE_TYPE line);
UNITY_NORETURN void UnityIgnore(const char* message, UNITY_LINE_TYPE line);
#endif

/* Assert implementation macros */
#define UNITY_TEST_ASSERT(condition, line, message) do { if (!(condition)) { UNITY_TEST_FAIL((line), (message)); } } while (0)
#define UNITY_TEST_ASSERT_NULL(pointer, line, message) UNITY_TEST_ASSERT(((pointer) == NULL), (line), (message))
#define UNITY_TEST_ASSERT_NOT_NULL(pointer, line, message) UNITY_TEST_ASSERT(((pointer) != NULL), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_INT(expected, actual, line, message) UnityAssertEqualIntNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_INT8(expected, actual, line, message) UnityAssertEqualIntNumber((UNITY_INT)(UNITY_INT8)(expected), (UNITY_INT)(UNITY_INT8)(actual), (message), (line), UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_EQUAL_INT16(expected, actual, line, message) UnityAssertEqualIntNumber((UNITY_INT)(UNITY_INT16)(expected), (UNITY_INT)(UNITY_INT16)(actual), (message), (line), UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_EQUAL_INT32(expected, actual, line, message) UnityAssertEqualIntNumber((UNITY_INT)(UNITY_INT32)(expected), (UNITY_INT)(UNITY_INT32)(actual), (message), (line), UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_EQUAL_UINT(expected, actual, line, message) UnityAssertEqualUintNumber((UNITY_UINT)(expected), (UNITY_UINT)(actual), (message), (line), UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_EQUAL_UINT8(expected, actual, line, message) UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT8)(expected), (UNITY_UINT)(UNITY_UINT8)(actual), (message), (line), UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_EQUAL_UINT16(expected, actual, line, message) UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT16)(expected), (UNITY_UINT)(UNITY_UINT16)(actual), (message), (line), UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_EQUAL_UINT32(expected, actual, line, message) UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT32)(expected), (UNITY_UINT)(UNITY_UINT32)(actual), (message), (line), UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_EQUAL_HEX8(expected, actual, line, message) UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT8)(expected), (UNITY_UINT)(UNITY_UINT8)(actual), (message), (line), UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_EQUAL_HEX16(expected, actual, line, message) UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT16)(expected), (UNITY_UINT)(UNITY_UINT16)(actual), (message), (line), UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_EQUAL_HEX32(expected, actual, line, message) UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT32)(expected), (UNITY_UINT)(UNITY_UINT32)(actual), (message), (line), UNITY_DISPLAY_STYLE_HEX32)
#define UNITY_TEST_ASSERT_EQUAL_PTR(expected, actual, line, message) UNITY_TEST_ASSERT(((UNITY_INTERNAL_PTR)(expected) == (UNITY_INTERNAL_PTR)(actual)), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_STRING(expected, actual, line, message) UnityAssertEqualString((expected), (actual), (message), (line))
#define UNITY_TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len, line, message) UnityAssertEqualStringLen((expected), (actual), (len), (message), (line))
#define UNITY_TEST_ASSERT_EQUAL_MEMORY(expected, actual, len, line, message) UnityAssertEqualMemory((expected), (actual), (len), 1, (message), (line), UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_BITS(mask, expected, actual, line, message) UnityAssertBits((mask), (expected), (actual), (message), (line))
#define UNITY_TEST_ASSERT_GREATER_THAN_INT(threshold, actual, line, message) UnityAssertIntGreaterOrLessOrEqualNumber((threshold), (actual), UNITY_GREATER_THAN, (message), (line), UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT(threshold, actual, line, message) UnityAssertIntGreaterOrLessOrEqualNumber((threshold), (actual), UNITY_SMALLER_THAN, (message), (line), UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_INT_WITHIN(delta, expected, actual, line, message) UnityAssertIntNumbersWithin((delta), (expected), (actual), (message), (line), UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements, line, message) UnityAssertEqualIntArray((expected), (actual), (num_elements), (message), (line), UNITY_DISPLAY_STYLE_INT, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, actual, num_elements, line, message) UnityAssertEqualIntArray((expected), (actual), (num_elements), (message), (line), UNITY_DISPLAY_STYLE_HEX32, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_UINT_ARRAY(expected, actual, num_elements, line, message) UnityAssertEqualIntArray((expected), (actual), (num_elements), (message), (line), UNITY_DISPLAY_STYLE_UINT, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, len, num, line, message) UnityAssertEqualMemory((expected), (actual), (len), (num), (message), (line), UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_PTR_ARRAY(expected, actual, num, line, message) UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (num), (message), (line), UNITY_DISPLAY_STYLE_POINTER, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num, line, message) UnityAssertEqualStringArray((expected), (actual), (num), (message), (line), UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_FAIL(line, message) UnityFail((message), (UNITY_LINE_TYPE)(line))
#define UNITY_TEST_IGNORE(line, message) UnityIgnore((message), (UNITY_LINE_TYPE)(line))

void UnityAssertIntGreaterOrLessOrEqualNumber(UNITY_INT threshold, UNITY_INT actual, UNITY_COMPARISON_T compare, const char *msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style);
void UnityAssertIntNumbersWithin(UNITY_UINT delta, UNITY_INT expected, UNITY_INT actual, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style);
void UnityAssertUintNumbersWithin(UNITY_UINT delta, UNITY_UINT expected, UNITY_UINT actual, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style);
void UnityAssertNumbersArrayWithin(UNITY_UINT delta, UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual, UNITY_UINT32 num_elements, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style, UNITY_FLAGS_T flags);
#endif

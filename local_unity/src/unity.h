/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-26 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
========================================================================= */
#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H
#define UNITY
#define UNITY_VERSION_MAJOR 2
#define UNITY_VERSION_MINOR 6
#define UNITY_VERSION_BUILD 3
#define UNITY_VERSION ((UNITY_VERSION_MAJOR << 16) | (UNITY_VERSION_MINOR << 8) | UNITY_VERSION_BUILD)
#ifdef __cplusplus
extern "C" {
#endif
#include "unity_internals.h"
void setUp(void);
void tearDown(void);
void suiteSetUp(void);
int suiteTearDown(int num_failures);
void resetTest(void);
void verifyTest(void);
/* Basic asserts */
#define TEST_FAIL_MESSAGE(message) UNITY_TEST_FAIL(__LINE__, (message))
#define TEST_FAIL() UNITY_TEST_FAIL(__LINE__, NULL)
#define TEST_IGNORE_MESSAGE(message) UNITY_TEST_IGNORE(__LINE__, (message))
#define TEST_IGNORE() UNITY_TEST_IGNORE(__LINE__, NULL)
#define TEST_MESSAGE(message) UnityMessage((message), __LINE__)
#define TEST_ONLY()
#define TEST_PASS() TEST_ABORT()
#define TEST_PASS_MESSAGE(message) do { UnityMessage((message), __LINE__); TEST_ABORT(); } while (0)
#define TEST_SOURCE_FILE(a)
#define TEST_INCLUDE_PATH(a)
/* Assert macros */
#define TEST_ASSERT(condition) UNITY_TEST_ASSERT((condition), __LINE__, " Expression Evaluated To FALSE")
#define TEST_ASSERT_TRUE(condition) UNITY_TEST_ASSERT((condition), __LINE__, " Expected TRUE Was FALSE")
#define TEST_ASSERT_FALSE(condition) UNITY_TEST_ASSERT(!(condition), __LINE__, " Expected FALSE Was TRUE")
#define TEST_ASSERT_NULL(pointer) UNITY_TEST_ASSERT_NULL((pointer), __LINE__, " Expected NULL")
#define TEST_ASSERT_NOT_NULL(pointer) UNITY_TEST_ASSERT_NOT_NULL((pointer), __LINE__, " Expected Non-NULL")
#define TEST_ASSERT_EQUAL_INT(expected, actual) UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_INT8(expected, actual) UNITY_TEST_ASSERT_EQUAL_INT8((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_INT16(expected, actual) UNITY_TEST_ASSERT_EQUAL_INT16((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_INT32(expected, actual) UNITY_TEST_ASSERT_EQUAL_INT32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT(expected, actual) UNITY_TEST_ASSERT_EQUAL_UINT((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT8(expected, actual) UNITY_TEST_ASSERT_EQUAL_UINT8((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT16(expected, actual) UNITY_TEST_ASSERT_EQUAL_UINT16((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT32(expected, actual) UNITY_TEST_ASSERT_EQUAL_UINT32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX(expected, actual) UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX8(expected, actual) UNITY_TEST_ASSERT_EQUAL_HEX8((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX16(expected, actual) UNITY_TEST_ASSERT_EQUAL_HEX16((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX32(expected, actual) UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_PTR(expected, actual) UNITY_TEST_ASSERT_EQUAL_PTR((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_STRING(expected, actual) UNITY_TEST_ASSERT_EQUAL_STRING((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len) UNITY_TEST_ASSERT_EQUAL_STRING_LEN((expected), (actual), (len), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) UNITY_TEST_ASSERT_EQUAL_MEMORY((expected), (actual), (len), __LINE__, NULL)
#define TEST_ASSERT_BITS(mask, expected, actual) UNITY_TEST_ASSERT_BITS((mask), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_GREATER_THAN(threshold, actual) UNITY_TEST_ASSERT_GREATER_THAN_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_LESS_THAN(threshold, actual) UNITY_TEST_ASSERT_SMALLER_THAN_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((expected), (actual), (num), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX_ARRAY(expected, actual, num) UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY((expected), (actual), (num), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT_ARRAY(expected, actual, num) UNITY_TEST_ASSERT_EQUAL_UINT_ARRAY((expected), (actual), (num), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, len, num) UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY((expected), (actual), (len), (num), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_PTR_ARRAY(expected, actual, num) UNITY_TEST_ASSERT_EQUAL_PTR_ARRAY((expected), (actual), (num), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num) UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY((expected), (actual), (num), __LINE__, NULL)
#define TEST_ASSERT_INT_WITHIN(delta, expected, actual) UNITY_TEST_ASSERT_INT_WITHIN((delta), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_HEX_WITHIN(delta, expected, actual) UNITY_TEST_ASSERT_HEX32_WITHIN((delta), (expected), (actual), __LINE__, NULL)
#define UNITY_NEW_TEST(a) do { Unity.CurrentTestName = (a); Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE)(__LINE__); Unity.NumberOfTests++; } while(0)
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#define TEST_PROTECT() (setjmp(Unity.AbortFrame) == 0)
#define TEST_ABORT() longjmp(Unity.AbortFrame, 1)
#ifdef __cplusplus
}
#endif
#endif

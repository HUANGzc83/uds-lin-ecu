/*
 * unity_config.h
 * Unity Test Framework — Configuration Header
 *
 * Defines Unity configuration macros that cannot be passed via CMake
 * -D flags on the command line (CMake 4.x drops function-style defines).
 *
 * This header is force-included via -include in every test target's
 * compile flags.
 */

#pragma once

#include "unity.h"

/* -----------------------------------------------------------------------
 * Convenience aliases for Unity assertion macros.
 * These avoid repetition of type-specific long names in tests.
 * ----------------------------------------------------------------------- */

#ifndef TEST_ASSERT_EQUAL_UINT8_ARRAY
#define TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num) \
    UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), \
                             (UNITY_UINT32)(num), NULL, __LINE__, \
                             UNITY_DISPLAY_STYLE_UINT8, UNITY_ARRAY_TO_ARRAY)
#endif

#ifndef TEST_ASSERT_GREATER_THAN_UINT16
#define TEST_ASSERT_GREATER_THAN_UINT16(threshold, actual) \
    TEST_ASSERT_GREATER_THAN(threshold, actual)
#endif

/* -----------------------------------------------------------------------
 * Alias for UNITY_BEGIN / UNITY_END — these are used by all test files
 * but cannot be passed via CMake -D flags (function-style macros with
 * arguments break on CMake 4.x command line propagation).
 * ----------------------------------------------------------------------- */
#ifndef UNITY_BEGIN
#define UNITY_BEGIN()    UnityBegin(__FILE__)
#endif
#ifndef UNITY_END
#define UNITY_END()      UnityEnd()
#endif

/* -----------------------------------------------------------------------
 * Generic TEST_ASSERT_EQUAL/TEST_ASSERT_NOT_EQUAL for readability.
 * ----------------------------------------------------------------------- */
#ifndef TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL(expected, actual) \
    TEST_ASSERT_EQUAL_INT(expected, actual)
#endif
#ifndef TEST_ASSERT_NOT_EQUAL
#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    TEST_ASSERT_TRUE((expected) != (actual))
#endif

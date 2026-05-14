/* =========================================================================
 *  unity_compat.c — Stub implementations for functions declared but not
 *  implemented in the minimal vendored unity.c.
 *
 *  Provides enough functionality for the UDS test suite to link and run.
 *  Not a complete Unity replacement — only what the test macros call.
 *
 *  NOTE: This file must NOT rely on static/internal symbols from unity.c.
 *  Only use the public Unity API (declared in unity_internals.h) and the
 *  output function macros provided via -D compiler flags.
 * ========================================================================= */
#include "unity.h"
#include <string.h>

#ifndef UNITY_PROGMEM
#define UNITY_PROGMEM
#endif

/*
 * Internal helpers replicated here because the equivalents in unity.c
 * are declared `static` and therefore not visible across translation units.
 */

/* Replicate UnityStrElement, UnityStrExpected, UnityStrWas, UnityStrSpacer */
static const char UNITY_PROGMEM kStrElem[]  = " Element ";
static const char UNITY_PROGMEM kStrExp[]   = " Expected ";
static const char UNITY_PROGMEM kStrWas[]   = " Was ";
static const char UNITY_PROGMEM kStrSep[]   = UNITY_FAILURE_DETAIL_SEPARATOR;
static const char UNITY_PROGMEM kStrFail[]  = "FAIL";

/* Replicate UnityTestResultsBegin (static in unity.c) */
static void beginResults(const char* file, UNITY_LINE_TYPE line)
{
    UnityPrint(file);
    UNITY_OUTPUT_CHAR(':');
    UnityPrintNumber((UNITY_INT)line);
    UNITY_OUTPUT_CHAR(':');
    UnityPrint(Unity.CurrentTestName);
    UNITY_OUTPUT_CHAR(':');
}

/* Replicate UnityTestResultsFailBegin (static in unity.c) */
static void beginFail(UNITY_LINE_TYPE line)
{
    beginResults(Unity.TestFile, line);
    UnityPrint(kStrFail);
    UNITY_OUTPUT_CHAR(':');
}

/* Replicate RETURN_IF_FAIL_OR_IGNORE */
#define RETURN_IF_FAIL \
    do { if (Unity.CurrentTestFailed || Unity.CurrentTestIgnored) { TEST_ABORT(); } } while (0)

/* Replicate UNITY_FAIL_AND_BAIL */
#define FAIL_AND_BAIL \
    do { Unity.CurrentTestFailed = 1; UNITY_OUTPUT_FLUSH(); TEST_ABORT(); } while (0)

/* -----------------------------------------------------------------------
 * Element-width helpers for UnityAssertEqualIntArray.
 * Returns the element size in bytes for a given display style.
 * ----------------------------------------------------------------------- */
static unsigned elem_size(UNITY_DISPLAY_STYLE_T style)
{
    unsigned nib = (unsigned)(style & 0x0F);
    return (nib == 0) ? (UNITY_INT_WIDTH / 8) : nib;
}

/* Compare two elements at the given width; return 0 if equal. */
static int elem_cmp(const void *pa, const void *pb, unsigned sz)
{
    unsigned i;
    for (i = 0; i < sz; i++) {
        if (((const unsigned char*)pa)[i] != ((const unsigned char*)pb)[i])
            return 1;
    }
    return 0;
}

/* Print one element at the given width (as unsigned hex). */
static void elem_print(const void *p, unsigned sz)
{
    UNITY_UINT v = 0;
    unsigned i;
    for (i = 0; i < sz && i < sizeof(v); i++)
        v = (v << 8) | ((const unsigned char*)p)[i];
    UnityPrintNumberHex(v, (char)(sz * 2));
}

/* -----------------------------------------------------------------------
 * UnityAssertEqualIntArray — compare two arrays element-by-element.
 * Handles different element widths via the UNITY_DISPLAY_STYLE_T style.
 * Used by TEST_ASSERT_EQUAL_UINT8_ARRAY, TEST_ASSERT_EQUAL_UINT_ARRAY, etc.
 * ----------------------------------------------------------------------- */
void UnityAssertEqualIntArray(UNITY_INTERNAL_PTR expected,
                              UNITY_INTERNAL_PTR actual,
                              UNITY_UINT32       num_elements,
                              const char*        msg,
                              UNITY_LINE_TYPE    line,
                              UNITY_DISPLAY_STYLE_T style,
                              UNITY_FLAGS_T      flags)
{
    UNITY_UINT32 i;
    unsigned sz = elem_size(style);
    const unsigned char *e = (const unsigned char*)expected;
    const unsigned char *a = (const unsigned char*)actual;

    (void)flags;

    RETURN_IF_FAIL;

    for (i = 0; i < num_elements; i++)
    {
        if (elem_cmp(e + i * sz, a + i * sz, sz))
        {
            beginFail(line);
            UnityPrint(kStrElem);
            UnityPrintNumberUnsigned((UNITY_UINT)(i + 1));
            UnityPrint(kStrExp);
            elem_print(e + i * sz, sz);
            UnityPrint(kStrWas);
            elem_print(a + i * sz, sz);
            if (msg) { UnityPrint(kStrSep); UnityPrint(msg); }
            FAIL_AND_BAIL;
        }
    }
}

/* -----------------------------------------------------------------------
 * UnityAssertIntGreaterOrLessOrEqualNumber — compare two integers.
 * Used by TEST_ASSERT_GREATER_THAN, TEST_ASSERT_LESS_THAN, etc.
 * ----------------------------------------------------------------------- */
void UnityAssertIntGreaterOrLessOrEqualNumber(UNITY_INT       threshold,
                                              UNITY_INT       actual,
                                              UNITY_COMPARISON_T compare,
                                              const char*     msg,
                                              UNITY_LINE_TYPE line,
                                              UNITY_DISPLAY_STYLE_T style)
{
    int pass = 0;
    (void)style;

    RETURN_IF_FAIL;

    switch (compare)
    {
        case UNITY_GREATER_THAN:        pass = (actual >  threshold); break;
        case UNITY_GREATER_OR_EQUAL:    pass = (actual >= threshold); break;
        case UNITY_SMALLER_THAN:        pass = (actual <  threshold); break;
        case UNITY_SMALLER_OR_EQUAL:    pass = (actual <= threshold); break;
        case UNITY_EQUAL_TO:            pass = (actual == threshold); break;
        case UNITY_NOT_EQUAL:           pass = (actual != threshold); break;
        default: break;
    }

    if (!pass)
    {
        beginFail(line);
        UnityPrint(kStrExp);
        UnityPrintNumber(threshold);
        UnityPrint(kStrWas);
        UnityPrintNumber(actual);
        if (msg) { UnityPrint(kStrSep); UnityPrint(msg); }
        FAIL_AND_BAIL;
    }
}

/* -----------------------------------------------------------------------
 * UnityMessage — output a message string.
 * Used by TEST_MESSAGE, TEST_PASS_MESSAGE, etc.
 * ----------------------------------------------------------------------- */
void UnityMessage(const char* message, UNITY_LINE_TYPE line)
{
    beginResults(Unity.TestFile, line);
    UnityPrint("MESSAGE: ");
    UnityPrint(message);
    UNITY_PRINT_EOL();
}

/* -----------------------------------------------------------------------
 * UnityAssertEqualMemory — compare two memory regions.
 * Used by TEST_ASSERT_EQUAL_MEMORY, TEST_ASSERT_EQUAL_MEMORY_ARRAY.
 * ----------------------------------------------------------------------- */
void UnityAssertEqualMemory(UNITY_INTERNAL_PTR expected,
                            UNITY_INTERNAL_PTR actual,
                            UNITY_UINT32       length,
                            UNITY_UINT32       num_elements,
                            const char*        msg,
                            UNITY_LINE_TYPE    line,
                            UNITY_FLAGS_T      flags)
{
    UNITY_UINT32 el;
    const unsigned char *e = (const unsigned char*)expected;
    const unsigned char *a = (const unsigned char*)actual;
    (void)flags;

    RETURN_IF_FAIL;

    for (el = 0; el < num_elements; el++)
    {
        UNITY_UINT32 i;
        const unsigned char *eb = e + el * length;
        const unsigned char *ab = a + el * length;
        for (i = 0; i < length; i++)
        {
            if (eb[i] != ab[i])
            {
                beginFail(line);
                UnityPrint(kStrElem);
                UnityPrintNumberUnsigned((UNITY_UINT)(el + 1));
                UnityPrint(" byte ");
                UnityPrintNumberUnsigned((UNITY_UINT)(i + 1));
                UnityPrint(kStrExp);
                elem_print(eb + i, 1);
                UnityPrint(kStrWas);
                elem_print(ab + i, 1);
                if (msg) { UnityPrint(kStrSep); UnityPrint(msg); }
                FAIL_AND_BAIL;
            }
        }
    }
}

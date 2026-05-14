/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-26 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
========================================================================= */
#include "unity.h"
#ifndef UNITY_PROGMEM
#define UNITY_PROGMEM
#endif
#ifdef UNITY_OMIT_OUTPUT_CHAR_HEADER_DECLARATION
void UNITY_OUTPUT_CHAR(int);
#endif
#define UNITY_FAIL_AND_BAIL do { Unity.CurrentTestFailed = 1; UNITY_OUTPUT_FLUSH(); TEST_ABORT(); } while (0)
#define UNITY_IGNORE_AND_BAIL do { Unity.CurrentTestIgnored = 1; UNITY_OUTPUT_FLUSH(); TEST_ABORT(); } while (0)
#define RETURN_IF_FAIL_OR_IGNORE do { if (Unity.CurrentTestFailed || Unity.CurrentTestIgnored) { TEST_ABORT(); } } while (0)
struct UNITY_STORAGE_T Unity;
const char UNITY_PROGMEM UnityStrOk[] = "OK";
const char UNITY_PROGMEM UnityStrPass[] = "PASS";
const char UNITY_PROGMEM UnityStrFail[] = "FAIL";
const char UNITY_PROGMEM UnityStrIgnore[] = "IGNORE";
static const char UNITY_PROGMEM UnityStrNull[] = "NULL";
static const char UNITY_PROGMEM UnityStrSpacer[] = UNITY_FAILURE_DETAIL_SEPARATOR;
static const char UNITY_PROGMEM UnityStrExpected[] = " Expected ";
static const char UNITY_PROGMEM UnityStrWas[] = " Was ";
static const char UNITY_PROGMEM UnityStrElement[] = " Element ";
const char UNITY_PROGMEM UnityStrErrShorthand[] = "Unity Shorthand Support Disabled";
const char UNITY_PROGMEM UnityStrErrFloat[] = "Unity Floating Point Disabled";
const char UNITY_PROGMEM UnityStrErrDouble[] = "Unity Double Precision Disabled";
const char UNITY_PROGMEM UnityStrErr64[] = "Unity 64-bit Support Disabled";
const char UNITY_PROGMEM UnityStrErrDetailStack[] = "Unity Detail Stack Support Disabled";
static void UnityPrintChar(const char* pch) {
    if ((*pch <= 126) && (*pch >= 32)) { UNITY_OUTPUT_CHAR(*pch); }
    else if (*pch == 13) { UNITY_OUTPUT_CHAR('\\'); UNITY_OUTPUT_CHAR('r'); }
    else if (*pch == 10) { UNITY_OUTPUT_CHAR('\\'); UNITY_OUTPUT_CHAR('n'); }
    else { UNITY_OUTPUT_CHAR('\\'); UNITY_OUTPUT_CHAR('x'); UnityPrintNumberHex((UNITY_UINT)*pch, 2); }
}
void UnityPrint(const char* string) { const char* pch = string; if (pch) while (*pch) { UnityPrintChar(pch); pch++; } }
void UnityPrintNumber(const UNITY_INT n) { UNITY_UINT num = (UNITY_UINT)n; if (n < 0) { UNITY_OUTPUT_CHAR('-'); num = (~num) + 1; } UnityPrintNumberUnsigned(num); }
void UnityPrintNumberUnsigned(UNITY_UINT num) { UNITY_UINT d = 1; while (num / d > 9) d *= 10; do { UNITY_OUTPUT_CHAR((char)('0' + (num / d % 10))); d /= 10; } while (d > 0); }
void UnityPrintNumberHex(UNITY_UINT num, char n) { int nib; char nibbles = n; if ((unsigned)nibbles > UNITY_MAX_NIBBLES) nibbles = UNITY_MAX_NIBBLES; while (nibbles > 0) { nibbles--; nib = (int)(num >> (nibbles * 4)) & 0x0F; UNITY_OUTPUT_CHAR((char)(nib <= 9 ? '0' + nib : 'A' - 10 + nib)); } }
static void UnityTestResultsBegin(const char* file, UNITY_LINE_TYPE line) { UnityPrint(file); UNITY_OUTPUT_CHAR(':'); UnityPrintNumber((UNITY_INT)line); UNITY_OUTPUT_CHAR(':'); UnityPrint(Unity.CurrentTestName); UNITY_OUTPUT_CHAR(':'); }
static void UnityTestResultsFailBegin(UNITY_LINE_TYPE line) { UnityTestResultsBegin(Unity.TestFile, line); UnityPrint(UnityStrFail); UNITY_OUTPUT_CHAR(':'); }
void UnityConcludeTest(void) {
    if (Unity.CurrentTestIgnored) Unity.TestIgnores++;
    else if (!Unity.CurrentTestFailed) { UnityTestResultsBegin(Unity.TestFile, Unity.CurrentTestLineNumber); UnityPrint(UnityStrPass); }
    else Unity.TestFailures++;
    Unity.CurrentTestFailed = 0; Unity.CurrentTestIgnored = 0; UNITY_PRINT_EXEC_TIME(); UNITY_PRINT_EOL(); UNITY_FLUSH_CALL();
}
void UnityAssertEqualIntNumber(UNITY_INT exp, UNITY_INT act, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style) {
    RETURN_IF_FAIL_OR_IGNORE; if (exp != act) { UnityTestResultsFailBegin(line); UnityPrint(UnityStrExpected); UnityPrintIntNumberByStyle(exp, style); UnityPrint(UnityStrWas); UnityPrintIntNumberByStyle(act, style); if (msg) { UnityPrint(UnityStrSpacer); UnityPrint(msg); } UNITY_FAIL_AND_BAIL; }
}
void UnityAssertEqualUintNumber(UNITY_UINT exp, UNITY_UINT act, const char* msg, UNITY_LINE_TYPE line, UNITY_DISPLAY_STYLE_T style) {
    RETURN_IF_FAIL_OR_IGNORE; if (exp != act) { UnityTestResultsFailBegin(line); UnityPrint(UnityStrExpected); UnityPrintUintNumberByStyle(exp, style); UnityPrint(UnityStrWas); UnityPrintUintNumberByStyle(act, style); if (msg) { UnityPrint(UnityStrSpacer); UnityPrint(msg); } UNITY_FAIL_AND_BAIL; }
}
void UnityPrintIntNumberByStyle(UNITY_INT n, UNITY_DISPLAY_STYLE_T s) {
    if (s == UNITY_DISPLAY_STYLE_CHAR) { UNITY_OUTPUT_CHAR('\''); UNITY_OUTPUT_CHAR((int)n); UNITY_OUTPUT_CHAR('\''); }
    else if ((s & UNITY_DISPLAY_RANGE_INT) == UNITY_DISPLAY_RANGE_INT) UnityPrintNumber(n);
    else if ((s & UNITY_DISPLAY_RANGE_UINT) == UNITY_DISPLAY_RANGE_UINT) UnityPrintNumberUnsigned((UNITY_UINT)n);
    else { UNITY_OUTPUT_CHAR('0'); UNITY_OUTPUT_CHAR('x'); UnityPrintNumberHex((UNITY_UINT)n, (char)((s & 0xF) * 2)); }
}
void UnityPrintUintNumberByStyle(UNITY_UINT n, UNITY_DISPLAY_STYLE_T s) {
    if ((s & UNITY_DISPLAY_RANGE_UINT) == UNITY_DISPLAY_RANGE_UINT) UnityPrintNumberUnsigned(n);
    else { UNITY_OUTPUT_CHAR('0'); UNITY_OUTPUT_CHAR('x'); UnityPrintNumberHex(n, (char)((s & 0xF) * 2)); }
}
void UnityAssertBits(UNITY_INT mask, UNITY_INT exp, UNITY_INT act, const char* msg, UNITY_LINE_TYPE line) {
    RETURN_IF_FAIL_OR_IGNORE; if ((mask & exp) != (mask & act)) { UnityTestResultsFailBegin(line); UnityPrint(UnityStrExpected); UnityPrintMask((UNITY_UINT)mask, (UNITY_UINT)exp); UnityPrint(UnityStrWas); UnityPrintMask((UNITY_UINT)mask, (UNITY_UINT)act); if (msg) { UnityPrint(UnityStrSpacer); UnityPrint(msg); } UNITY_FAIL_AND_BAIL; }
}
void UnityPrintMask(UNITY_UINT mask, UNITY_UINT num) {
    UNITY_UINT b = (UNITY_UINT)1 << (UNITY_INT_WIDTH - 1); UNITY_INT32 i;
    for (i = 0; i < UNITY_INT_WIDTH; i++) { if (b & mask) UNITY_OUTPUT_CHAR((b & num) ? '1' : '0'); else UNITY_OUTPUT_CHAR('X'); b >>= 1; }
}
void UnityBegin(const char* filename) { Unity.TestFile = filename; Unity.CurrentTestName = NULL; Unity.CurrentTestLineNumber = 0; Unity.NumberOfTests = 0; Unity.TestFailures = 0; Unity.TestIgnores = 0; Unity.CurrentTestFailed = 0; Unity.CurrentTestIgnored = 0; UNITY_OUTPUT_START(); }
int UnityEnd(void) {
    UNITY_PRINT_EOL(); UnityPrint("-----------------------"); UNITY_PRINT_EOL();
    UnityPrintNumber((UNITY_INT)Unity.NumberOfTests); UnityPrint(" Tests ");
    UnityPrintNumber((UNITY_INT)Unity.TestFailures); UnityPrint(" Failures ");
    UnityPrintNumber((UNITY_INT)Unity.TestIgnores); UnityPrint(" Ignored"); UNITY_PRINT_EOL();
    UnityPrint(Unity.TestFailures == 0U ? UnityStrOk : UnityStrFail); UNITY_PRINT_EOL(); UNITY_OUTPUT_COMPLETE();
    return (int)(Unity.TestFailures);
}
#ifndef UNITY_EXCLUDE_SETJMP_H
UNITY_NORETURN void UnityFail(const char* msg, UNITY_LINE_TYPE line) { UnityTestResultsFailBegin(line); UnityPrint(msg); UNITY_FAIL_AND_BAIL; }
UNITY_NORETURN void UnityIgnore(const char* msg, UNITY_LINE_TYPE line) { UnityTestResultsBegin(Unity.TestFile, line); UnityPrint(UnityStrIgnore); if (msg) { UNITY_OUTPUT_CHAR(':'); UnityPrint(msg); } UNITY_IGNORE_AND_BAIL; }
#endif
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum) {
    UNITY_CLR_DETAILS(); if (TEST_PROTECT()) { setUp(); Func(); } if (TEST_PROTECT()) { tearDown(); } UnityConcludeTest();
}
/* setUp / tearDown are provided by each test file */
/* main is provided by each test file */

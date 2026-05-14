/*
 * unity_config.c
 * Unity Test Framework — Output Function Stubs for PC Simulation
 *
 * Provides the output character and flush functions that Unity expects.
 * On desktop/Linux, these map to stdio putchar/fflush.
 *
 * The CMake target_compile_options provides these as macros (empty/no-op
 * on PC), but this file provides the C function symbols for any code
 * that calls them by name.  We #undef the macros first to avoid
 * preprocessor expansion clashing with the function definitions.
 */

#include <stdio.h>

/* Undo the -D macro definitions so we can define real C functions */
#undef UNITY_OUTPUT_CHAR
#undef UNITY_PRINT_EXEC_TIME
#undef UNITY_PRINT_EOL
#undef UNITY_FLUSH_CALL
#undef UNITY_OUTPUT_FLUSH
#undef UNITY_OUTPUT_START
#undef UNITY_OUTPUT_COMPLETE

void UNITY_OUTPUT_CHAR(int c)
{
    putchar(c);
}

void UNITY_PRINT_EXEC_TIME(void)
{
    /* Not implemented for PC simulation — no-op */
}

void UNITY_PRINT_EOL(void)
{
    putchar('\n');
}

void UNITY_FLUSH_CALL(void)
{
    fflush(stdout);
}

void UNITY_OUTPUT_FLUSH(void)
{
    fflush(stdout);
}

void UNITY_OUTPUT_START(void)
{
    /* Not implemented for PC simulation — no-op */
}

void UNITY_OUTPUT_COMPLETE(void)
{
    /* Not implemented for PC simulation — no-op */
}

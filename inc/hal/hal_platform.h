/*
 * hal_platform.h
 * Platform detection macros for HAL selection.
 *
 * Defines UDS_PLATFORM_POSIX when compiling on a POSIX-compliant
 * system (Linux, macOS, BSD).  MCU toolchains (ARM-GCC, IAR, etc.)
 * do NOT define __linux__ and therefore leave UDS_PLATFORM_POSIX
 * undefined, allowing the build to fall back to stubs or MCU HAL.
 *
 * Usage:
 *   #include "hal/hal_platform.h"
 *   #ifdef UDS_PLATFORM_POSIX
 *       // POSIX-specific code
 *   #endif
 */

#pragma once

#if defined(__linux__)
#   define UDS_PLATFORM_POSIX  1
#endif

/* ------------------------------------------------------------------
 * Default serial device path for the POSIX UART HAL.
 *
 * Override at build time with -DUDS_HAL_UART_DEVICE=\"/dev/ttyS1\".
 * ----------------------------------------------------------------*/
#ifndef UDS_HAL_UART_DEVICE
#   ifdef UDS_PLATFORM_POSIX
#       define UDS_HAL_UART_DEVICE  "/dev/ttyS0"
#   endif
#endif

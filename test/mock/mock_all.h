/*
 * mock_all.h
 * Convenience header — includes all mock HAL modules in one shot.
 *
 * Usage (in a test .c file):
 *   #include "mock_all.h"
 *
 * This pulls in the HAL interface declarations (via hal_common.h) and
 * the mock control API for UART, Timer, and NVM.
 */

#ifndef MOCK_ALL_H
#define MOCK_ALL_H

#include "hal/hal_common.h"
#include "mock_uart.h"
#include "mock_timer.h"
#include "mock_nvm.h"

#endif /* MOCK_ALL_H */

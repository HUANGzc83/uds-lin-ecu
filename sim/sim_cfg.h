/*
 * sim_cfg.h
 * PC Simulation Configuration Macros
 *
 * Tuning constants for the UDS-LIN ECU PC simulator.
 * Users may override defaults via compiler -D flags.
 *
 * Task 21 — PC Simulation Main + Integration Test Runner
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * LIN Bus Configuration                                                    *
 * ======================================================================== */

/** @brief Default Node Address for Diagnostics (NAD) — 0x01 per ISO 17987-3 */
#ifndef SIM_LIN_NAD
#define SIM_LIN_NAD         0x01u
#endif

/** @brief LIN bus baud rate in bits per second (default 19200) */
#ifndef SIM_LIN_BAUDRATE
#define SIM_LIN_BAUDRATE    19200u
#endif

/* ======================================================================== *
 * UDS Timing Configuration                                                 *
 * ======================================================================== *
 * These override the defaults in uds_session.c when building the simulator,
 * allowing tighter timing for simulation.  The simulator is sequential so
 * timeouts are effectively infinite; these values are informational.
 */

/** @brief P2Server_max timing in milliseconds */
#ifndef SIM_P2_SERVER_MAX
#define SIM_P2_SERVER_MAX       50u
#endif

/** @brief P2StarServer_max timing in milliseconds */
#ifndef SIM_P2_STAR_SERVER_MAX
#define SIM_P2_STAR_SERVER_MAX  5000u
#endif

/* ======================================================================== *
 * DID Defaults                                                             *
 * ======================================================================== *
 * Placeholder values registered during sim_init().  These mimic the
 * standard DIDs defined in uds_data.c.
 */

/** @brief Default Vehicle Identification Number (17 bytes) */
#ifndef SIM_VIN_DEFAULT
#define SIM_VIN_DEFAULT     "UDS-LIN-SIM-0000001"
#endif

/** @brief Default ECU serial number (10 bytes) */
#ifndef SIM_SERIAL_DEFAULT
#define SIM_SERIAL_DEFAULT  "SIM001-ECU"
#endif

/** @brief Default ECU software number (8 bytes) */
#ifndef SIM_SW_NUMBER
#define SIM_SW_NUMBER       "UDS-1.00"
#endif

/** @brief Default ECU software version number (8 bytes) */
#ifndef SIM_SW_VERSION
#define SIM_SW_VERSION      "V1.0.0.0"
#endif

/* ======================================================================== *
 * Main Loop Configuration                                                  *
 * ======================================================================== */

/** @brief Maximum iterations before sim_main() returns (0 = infinite) */
#ifndef SIM_MAX_ITERATIONS
#define SIM_MAX_ITERATIONS  0u
#endif

/** @brief UDS response serialization buffer size */
#ifndef SIM_RSP_BUF_SIZE
#define SIM_RSP_BUF_SIZE    512u
#endif

#ifdef __cplusplus
}
#endif

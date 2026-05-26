/*
 * uds_cfg.h
 * UDS Configuration Constants
 *
 * Central configuration header for UDS stack constants.
 * All values use #ifndef so users can override via compiler -D flags.
 *
 * Wave 1 Task 4 — Config Header Extraction
 */

#ifndef UDS_CFG_H
#define UDS_CFG_H

#include <stdint.h>

/* ======================================================================== *
 * Memory Region Configuration                                              *
 * ======================================================================== */

/** @brief Maximum number of registrable memory regions for 0x23/0x3D */
#ifndef UDS_MEM_REGION_MAX
#define UDS_MEM_REGION_MAX  8u
#endif

#endif /* UDS_CFG_H */

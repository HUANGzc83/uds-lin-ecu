/*
 * uds_dtc.h
 * DTC State Machine Engine — Module Header
 *
 * Full 8-bit DTC status tracking per ISO 14229-1:2020 Annex D.
 * Provides a static DTC database with status bit management,
 * group-based clearing, snapshot/extended data storage, and
 * status-mask-based querying.
 *
 * Wave 3b Task 15 — DTC State Machine Engine
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * DTC Status Byte Bits — ISO 14229-1:2020 Annex D                          *
 * ======================================================================== */

/** @brief Individual status bit flags for the 8-bit DTC status byte */
typedef enum {
    DTC_TEST_FAILED                   = 0x01, /**< @brief Bit 0 — testFailed */
    DTC_TEST_FAILED_THIS_OP_CYCLE     = 0x02, /**< @brief Bit 1 — testFailedThisOperationCycle */
    DTC_PENDING                        = 0x04, /**< @brief Bit 2 — pendingDTC */
    DTC_CONFIRMED                      = 0x08, /**< @brief Bit 3 — confirmedDTC */
    DTC_TEST_FAILED_SINCE_LAST_CLEAR  = 0x10, /**< @brief Bit 4 — testFailedSinceLastClear */
    DTC_THIS_OP_CYCLE                 = 0x20, /**< @brief Bit 5 — testNotCompletedSinceLastClear */
    DTC_WARNING_INDICATOR             = 0x40, /**< @brief Bit 6 — warningIndicatorRequested */
    DTC_TEST_NOT_COMPLETED            = 0x80  /**< @brief Bit 7 — testNotCompletedSinceThisOperationCycle */
} uds_dtc_status_t;

/* ======================================================================== *
 * DTC Record Structure                                                     *
 * ======================================================================== */

/** @brief Maximum number of DTC records in the static database */
#define UDS_DTC_MAX  32

/** @brief Single DTC record with full status, snapshot, and extended data */
typedef struct {
    uint32_t dtc;               /**< @brief DTC code (3-byte / 24-bit value, stored in lower 24 bits) */
    uint8_t  status;            /**< @brief 8-bit DTC status byte (bitmask of uds_dtc_status_t) */
    uint8_t  snapshot[8];       /**< @brief Snapshot / environmental data (max 8 bytes) */
    uint16_t snapshot_len;      /**< @brief Number of valid bytes in snapshot[] */
    uint8_t  extended[8];       /**< @brief Extended data (max 8 bytes) */
    uint16_t ext_len;           /**< @brief Number of valid bytes in extended[] */
} uds_dtc_record_t;

/* ======================================================================== *
 * Function Declarations                                                   *
 * ======================================================================== */

/**
 * @brief Initialize the static DTC database (clear all records).
 *
 * Sets all UDS_DTC_MAX entries to zero: DTC code, status, snapshot,
 * and extended data are all reset.
 */
void uds_dtc_init(void);

/**
 * @brief Set or clear individual status bits for a given DTC.
 *
 * If the DTC does not yet exist in the database and @p set is true,
 * a new record is created.  If @p set is false and the DTC does not
 * exist, the call is silently ignored.
 *
 * @param[in] dtc       DTC code (24-bit value)
 * @param[in] bit_mask  Bitmask of status bits to modify (e.g. DTC_TEST_FAILED)
 * @param[in] set       true to set the bits; false to clear them
 */
void uds_dtc_set_status(uint32_t dtc, uint8_t bit_mask, bool set);

/**
 * @brief Retrieve the current status byte for a DTC.
 *
 * @param[in] dtc  DTC code to look up
 * @return Status byte (bitmask of uds_dtc_status_t), or 0 if DTC is unknown
 */
uint8_t uds_dtc_get_status(uint32_t dtc);

/**
 * @brief Find all DTCs whose status byte matches a given mask.
 *
 * Each DTC whose (status & status_mask) != 0 is copied into @p out_dtcs.
 *
 * @param[in]  status_mask  Bitmask to test against each DTC's status byte
 * @param[out] out_dtcs     Buffer to receive matching DTC codes
 * @param[in]  max_count    Capacity of @p out_dtcs
 * @return Number of matching DTCs written to @p out_dtcs
 */
uint16_t uds_dtc_get_by_status_mask(uint8_t status_mask, uint32_t *out_dtcs, uint16_t max_count);

/**
 * @brief Clear DTC records by group (upper nibble of DTC).
 *
 * Groups are identified by the upper nibble (bits 23:20) of the 24-bit DTC:
 *   - 0x0 = Powertrain
 *   - 0x2 = Chassis
 *   - 0x4 = Body
 *   - 0x6 = Network
 * Special value 0xFFFFFF clears all DTCs regardless of group.
 *
 * @param[in] group_mask  Group identifier or 0xFFFFFF for clear-all
 */
void uds_dtc_clear(uint32_t group_mask);

/**
 * @brief Get snapshot data for a DTC.
 *
 * @param[in]  dtc  DTC code to look up
 * @param[out] buf  Buffer to receive snapshot data
 * @param[out] len  On success: number of bytes written to @p buf
 * @return true if DTC was found and snapshot data exists, false otherwise
 */
bool uds_dtc_get_snapshot(uint32_t dtc, uint8_t *buf, uint16_t *len);

/**
 * @brief Get extended data for a DTC.
 *
 * @param[in]  dtc  DTC code to look up
 * @param[out] buf  Buffer to receive extended data
 * @param[out] len  On success: number of bytes written to @p buf
 * @return true if DTC was found and extended data exists, false otherwise
 */
bool uds_dtc_get_extended(uint32_t dtc, uint8_t *buf, uint16_t *len);

/**
 * @brief Count DTCs matching a status mask.
 *
 * Counts all DTCs where (status & status_mask) != 0.
 *
 * @param[in] status_mask  Bitmask to test
 * @return Number of DTCs matching the mask
 */
uint16_t uds_dtc_get_count(uint8_t status_mask);

/**
 * @brief Set snapshot data for a DTC (for testing / internal use).
 *
 * Creates a new record if the DTC does not yet exist.
 * Copies up to 8 bytes of snapshot data.
 *
 * @param[in] dtc   DTC code
 * @param[in] data  Snapshot data buffer
 * @param[in] len   Number of bytes to copy (max 8)
 * @return true if data was stored, false if database is full
 */
bool uds_dtc_set_snapshot(uint32_t dtc, const uint8_t *data, uint16_t len);

/**
 * @brief Set extended data for a DTC (for testing / internal use).
 *
 * Creates a new record if the DTC does not yet exist.
 * Copies up to 8 bytes of extended data.
 *
 * @param[in] dtc   DTC code
 * @param[in] data  Extended data buffer
 * @param[in] len   Number of bytes to copy (max 8)
 * @return true if data was stored, false if database is full
 */
bool uds_dtc_set_extended(uint32_t dtc, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

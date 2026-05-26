/*
 * uds_data.h
 * DID Registry & Data Storage — Module Header
 *
 * Implements a static Data Identifier (DID) registry per ISO 14229-1:2020
 * with access control (read-only, write-only, read-write, secured access),
 * pluggable read/write callbacks, and pre-registered standard DIDs.
 *
 * Wave 3b Task 17 — DID Registry & Data Storage
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Configuration Macros                                                     *
 * ======================================================================== */

/** @brief Maximum number of DID entries in the static registry */
#ifndef DID_MAX_ENTRIES
#define DID_MAX_ENTRIES 64
#endif

/* ======================================================================== *
 * DID Access Type Enum                                                     *
 * ======================================================================== */

/** @brief DID access control types */
typedef enum {
    DID_READ_ONLY       = 1, /**< @brief Readable, not writable */
    DID_WRITE_ONLY      = 2, /**< @brief Writable, not readable */
    DID_READ_WRITE      = 3, /**< @brief Readable and writable */
    DID_SECURED_READ    = 4, /**< @brief Readable only when security unlocked */
    DID_SECURED_WRITE   = 5  /**< @brief Writable only when security unlocked */
} uds_did_access_t;

/* ======================================================================== *
 * DID Entry Structure                                                      *
 * ======================================================================== */

/** @brief Single DID entry in the registry */
typedef struct {
    uint16_t did;                               /**< @brief Data Identifier */
    uint16_t len;                               /**< @brief Data length in bytes */
    uds_did_access_t access;                    /**< @brief Access control type */

    /** @brief Pointer to data buffer (used when on_read/on_write is NULL) */
    const uint8_t *data;

    /**
     * @brief Optional read callback (invoked instead of copying from data ptr).
     *
     * Called when a read request is made for this DID and access is granted.
     * The callback should fill @p buf with the DID data and set @p len to
     * the number of bytes written.
     *
     * @param[in]  did  The Data Identifier being read
     * @param[out] buf  Buffer to receive the data
     * @param[in,out] len  On input: capacity of buf. On output: bytes written.
     * @return true on success, false on error
     */
    bool (*on_read)(uint16_t did, uint8_t *buf, uint16_t *len);

    /**
     * @brief Optional write callback (invoked instead of storing to data ptr).
     *
     * Called when a write request is made for this DID and access is granted.
     *
     * @param[in] did   The Data Identifier being written
     * @param[in] data  Data buffer to write
     * @param[in] len   Number of bytes to write
     * @return true on success, false on error
     */
    bool (*on_write)(uint16_t did, const uint8_t *data, uint16_t len);
} uds_did_entry_t;

/* ======================================================================== *
 * Function Declarations                                                   *
 * ======================================================================== */

/**
 * @brief Initialize the DID registry with default DIDs.
 *
 * Clears all existing entries and registers the standard DIDs:
 *   - 0xF190: VIN (17 bytes, READ_ONLY)
 *   - 0xF186: ECU serial number (10 bytes, READ_ONLY)
 *   - 0xF187: System supplier ECU software number (8 bytes, READ_ONLY)
 *   - 0xF18C: System supplier ECU software version number (8 bytes, READ_ONLY)
 */
void uds_did_init(void);

/**
 * @brief Register a custom DID entry in the registry.
 *
 * The entry is copied into the static registry. The caller must ensure
 * that the @p data pointer remains valid for the lifetime of the registry.
 *
 * @param[in] entry  Pointer to the DID entry to register
 * @return true if registered successfully, false if table full or DID exists
 */
bool uds_did_register(const uds_did_entry_t *entry);

/**
 * @brief Read data from a DID.
 *
 * Access is granted based on the DID's access control and the @p unlocked
 * parameter:
 *   - DID_READ_ONLY / DID_READ_WRITE: always readable
 *   - DID_SECURED_READ: readable only when @p unlocked is true
 *   - DID_WRITE_ONLY / DID_SECURED_WRITE: not readable
 *
 * If the entry has an on_read callback it is invoked; otherwise data is
 * copied from the entry's data buffer.
 *
 * @param[in]  did      Data Identifier to read
 * @param[out] buf      Buffer to receive the data (must be large enough)
 * @param[in,out] len   On input: capacity of buf. On success: bytes written.
 * @param[in]  unlocked Security unlock flag (true = security is unlocked)
 * @return true on successful read, false on error or access denied
 */
bool uds_did_read(uint16_t did, uint8_t *buf, uint16_t *len, bool unlocked);

/**
 * @brief Write data to a DID.
 *
 * Access is granted based on the DID's access control and the @p unlocked
 * parameter:
 *   - DID_WRITE_ONLY / DID_READ_WRITE: always writable
 *   - DID_SECURED_WRITE: writable only when @p unlocked is true
 *   - DID_READ_ONLY / DID_SECURED_READ: not writable
 *
 * If the entry has an on_write callback it is invoked; otherwise data is
 * copied into the entry's data buffer.
 *
 * @param[in] did      Data Identifier to write
 * @param[in] data     Data buffer to write
 * @param[in] len      Number of bytes to write (must not exceed DID length)
 * @param[in] unlocked Security unlock flag (true = security is unlocked)
 * @return true on successful write, false on error or access denied
 */
bool uds_did_write(uint16_t did, const uint8_t *data, uint16_t len, bool unlocked);

/**
 * @brief Find a DID entry in the registry.
 *
 * @param[in] did  Data Identifier to find
 * @return Pointer to the entry, or NULL if not found
 */
const uds_did_entry_t* uds_did_find(uint16_t did);

/**
 * @brief Get the number of registered DIDs.
 *
 * @return Number of entries in the registry
 */
uint16_t uds_did_get_count(void);

#ifdef __cplusplus
}
#endif

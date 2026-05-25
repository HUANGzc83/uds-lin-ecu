/*
 * uds_svc_data.h
 * Data Transmission Services — Module Header
 *
 * Implements handler functions for UDS data transmission services:
 *   0x22 — ReadDataByIdentifier
 *   0x23 — ReadMemoryByAddress
 *   0x24 — ReadScalingDataByIdentifier
 *   0x2A — ReadDataByPeriodicIdentifier
 *   0x2C — DynamicallyDefineDataIdentifier
 *   0x2E — WriteDataByIdentifier
 *   0x3D — WriteMemoryByAddress
 *
 * Each handler follows the pattern:
 *   bool handler(const uds_request_t *req, uds_response_t *rsp, void *context);
 *
 * Return value: true  = send response (rsp populated with positive or negative)
 *               false = suppress response (SPRMIB bit set on positive result)
 *
 * Wave 3a Task 10 — Data Transmission Services
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Configuration Macros                                                     *
 * ======================================================================== */

/** @brief Maximum number of dynamic DID entries (0x2C) */
#ifndef DYNAMIC_DID_MAX
#define DYNAMIC_DID_MAX 8
#endif

/** @brief Maximum number of source definitions per dynamic DID */
#ifndef DYNAMIC_DID_SOURCE_MAX
#define DYNAMIC_DID_SOURCE_MAX 4
#endif

/** @brief Maximum number of periodic DID schedule entries (0x2A) */
#ifndef PERIODIC_DID_MAX
#define PERIODIC_DID_MAX 8
#endif

/** @brief Size of the simulated memory buffer for 0x23/0x3D */
#ifndef MEMORY_BUF_SIZE
#define MEMORY_BUF_SIZE 1024
#endif

/** @brief Maximum number of registrable memory regions for 0x23/0x3D */
#ifndef UDS_MEM_REGION_MAX
#define UDS_MEM_REGION_MAX 8
#endif

/* ======================================================================== *
 * Helper Macro — Reconstruct byte[1] of raw request                        *
 * ======================================================================== *
 * The parser always extracts a subfunction from raw[1] (when len >= 2).
 * For services without a subfunction (0x22, 0x23, 0x24, 0x2C, 0x2E, 0x3D),
 * raw[1] is actually the first data byte.  This macro reconstructs it:
 *   byte[1] = subfunction.value | (suppress_rsp << 7)
 */
#define UDS_REQ_BYTE1(req) \
    ((uint8_t)((req)->subfunction.value | ((req)->subfunction.suppress_rsp << 7)))

/**
 * @brief Extract the 16-bit DID from a request where DID is in byte[1]-byte[2].
 *
 * The parser consumes byte[1] as subfunction.  UDS_REQ_BYTE1 recovers it,
 * and req->data[0] holds byte[2].
 */
#define UDS_REQ_DID(req) \
    ((uint16_t)(((uint16_t)UDS_REQ_BYTE1(req) << 8) | (req)->data[0]))

/* ======================================================================== *
 * Memory Region Types (for 0x23 / 0x3D)                                   *
 * ======================================================================== */

/** @brief A contiguous simulated memory region for Read/WriteMemoryByAddress */
typedef struct {
    uint32_t start_addr; /**< @brief First valid address (inclusive) */
    uint32_t end_addr;   /**< @brief Last valid address (inclusive) */
    uint8_t *data;       /**< @brief Pointer to simulated RAM buffer */
} uds_mem_region_t;

/* ======================================================================== *
 * Dynamic DID Types (for 0x2C)                                            *
 * ======================================================================== */

/** @brief Definition mode for DynamicallyDefineDataIdentifier */
typedef enum {
    DYNAMIC_DID_DEFINE_BY_ID    = 0x01, /**< @brief Define from source DIDs */
    DYNAMIC_DID_DEFINE_BY_MEM   = 0x02, /**< @brief Define from memory address */
    DYNAMIC_DID_DELETE          = 0x03  /**< @brief Delete a dynamic DID */
} uds_dynamic_did_mode_t;

/** @brief Single source entry within a dynamic DID definition */
typedef struct {
    uint8_t  source_type; /**< @brief 0=byId, 1=byMem */
    uint16_t did;         /**< @brief Source DID (for byId) */
    uint32_t addr;        /**< @brief Source address (for byMem) */
    uint16_t len;         /**< @brief Number of bytes from this source */
} uds_dynamic_did_source_t;

/** @brief A dynamically defined DID entry */
typedef struct {
    uint16_t                     did;           /**< @brief The dynamic DID */
    uint8_t                      source_count;  /**< @brief Number of sources */
    uds_dynamic_did_source_t     sources[DYNAMIC_DID_SOURCE_MAX];
    bool                         active;        /**< @brief Whether this entry is in use */
} uds_dynamic_did_entry_t;

/* ======================================================================== *
 * Periodic DID Types (for 0x2A)                                           *
 * ======================================================================== */

/** @brief A scheduled periodic DID entry */
typedef struct {
    uint8_t  transmission_mode; /**< @brief 0x01=slow, 0x02=medium, 0x03=fast */
    uint8_t  periodic_did;      /**< @brief The 1-byte periodic DID identifier */
    uint16_t source_did;        /**< @brief The actual 16-bit DID to read */
    bool     active;            /**< @brief Whether this schedule entry is active */
} uds_periodic_did_entry_t;

/* ======================================================================== *
 * Handler Function Declarations                                            *
 * ======================================================================== */

/**
 * @brief 0x22 — ReadDataByIdentifier
 *
 * Reads data for a given 16-bit DID from the DID registry.  The response
 * contains the DID followed by the data bytes.
 *
 * Request:  [0x22][DID_high][DID_low]
 * Response: [0x62][DID_high][DID_low][data...]
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Pointer to bool unlocked flag
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_read_data_by_id(const uds_request_t *req,
                              uds_response_t      *rsp,
                              void                *context);

/**
 * @brief 0x23 — ReadMemoryByAddress
 *
 * Reads data from a simulated memory region at the given address and size.
 * Uses addressAndLengthFormatIdentifier format byte.
 *
 * Request:  [0x23][addr_format][addr_bytes...][size_bytes...]
 * Response: [0x63][addr_format][addr_bytes...][size_bytes...][data...]
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Unused (pass NULL)
 * @return true to send response
 */
bool uds_svc_read_memory_by_address(const uds_request_t *req,
                                     uds_response_t      *rsp,
                                     void                *context);

/**
 * @brief 0x24 — ReadScalingDataByIdentifier
 *
 * Reads scaling data (simplified) for a given DID.  Returns the DID data
 * followed by fixed scaling factors: 2-byte scale factor, 2-byte offset,
 * 1-byte unit.
 *
 * Request:  [0x24][DID_high][DID_low]
 * Response: [0x64][DID_high][DID_low][data...][scale_hi][scale_lo][off_hi][off_lo][unit]
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Pointer to bool unlocked flag
 * @return true to send response
 */
bool uds_svc_read_scaling_data_by_id(const uds_request_t *req,
                                      uds_response_t      *rsp,
                                      void                *context);

/**
 * @brief 0x2A — ReadDataByPeriodicIdentifier
 *
 * Configures periodic DID transmission.  Stores the schedule (non-functional
 * for PC simulation — just stores configuration).
 *
 * Request:  [0x2A][transmissionMode][periodicDID][DID_high][DID_low]
 * Response: [0x6A][transmissionMode]
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Unused (pass NULL)
 * @return true to send response
 */
bool uds_svc_read_data_by_periodic_id(const uds_request_t *req,
                                       uds_response_t      *rsp,
                                       void                *context);

/**
 * @brief 0x2C — DynamicallyDefineDataIdentifier
 *
 * Creates or deletes dynamic DIDs from source DIDs or memory addresses.
 * Supported definition modes:
 *   0x01 — defineByIdentifier (source DIDs)
 *   0x02 — defineByMemory (memory address range)
 *   0x03 — delete (delete a dynamic DID)
 *
 * Request:  [0x2C][definitionMode][data...]
 * Response: [0x6C][DID_high][DID_low]
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Unused (pass NULL)
 * @return true to send response
 */
bool uds_svc_dynamically_define_data_id(const uds_request_t *req,
                                        uds_response_t      *rsp,
                                        void                *context);

/**
 * @brief 0x2E — WriteDataByIdentifier
 *
 * Writes data to a DID in the registry.  Validates access via context
 * (unlocked flag).
 *
 * Request:  [0x2E][DID_high][DID_low][data...]
 * Response: [0x6E][DID_high][DID_low]
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Pointer to bool unlocked flag
 * @return true to send response
 */
bool uds_svc_write_data_by_id(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context);

/**
 * @brief 0x3D — WriteMemoryByAddress
 *
 * Writes data to a simulated memory region at the given address.
 *
 * Request:  [0x3D][addr_format][addr_bytes...][size_bytes...][data...]
 * Response: [0x7D][addr_format][addr_bytes...][size_bytes...]
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Pointer to bool unlocked flag
 * @return true to send response
 */
bool uds_svc_write_memory_by_address(const uds_request_t *req,
                                      uds_response_t      *rsp,
                                      void                *context);

/* ======================================================================== *
 * Initialization & State Access APIs                                       *
 * ======================================================================== */

/**
 * @brief Initialise the data transmission services state.
 *
 * Clears dynamic DID storage, periodic schedule, and initialises the
 * simulated memory buffer.
 */
void uds_svc_data_init(void);

/**
 * @brief Get the simulated memory buffer (for test inspection).
 *
 * @return Pointer to the simulated RAM buffer
 */
uint8_t* uds_svc_data_get_memory_buf(void);

/**
 * @brief Get the number of memory regions defined.
 *
 * @return Number of valid memory regions
 */
uint16_t uds_svc_data_get_memory_region_count(void);

/**
 * @brief Get the memory region table.
 *
 * @return Pointer to the static memory region array
 */
const uds_mem_region_t* uds_svc_data_get_memory_regions(void);

/**
 * @brief Get the number of active dynamic DID entries.
 *
 * @return Active dynamic DID count
 */
uint8_t uds_svc_data_get_dynamic_did_count(void);

/**
 * @brief Get the dynamic DID entry at index.
 *
 * @param[in]  idx  Index into dynamic DID array
 * @return Pointer to the entry, or NULL if index out of range
 */
const uds_dynamic_did_entry_t* uds_svc_data_get_dynamic_did(uint8_t idx);

/**
 * @brief Get the number of active periodic DID schedule entries.
 *
 * @return Active periodic DID count
 */
uint8_t uds_svc_data_get_periodic_did_count(void);

/**
 * @brief Get the periodic DID entry at index.
 *
 * @param[in]  idx  Index into periodic DID array
 * @return Pointer to the entry, or NULL if index out of range
 */
const uds_periodic_did_entry_t* uds_svc_data_get_periodic_did(uint8_t idx);

/* ======================================================================== *
 * Runtime Memory Region Registration API                                   *
 * ======================================================================== */

/**
 * @brief Register a memory region for 0x23/0x3D address space.
 *
 * The region's data buffer must remain valid after registration.
 *
 * @param start  First valid address (inclusive)
 * @param end    Last valid address (inclusive), must be >= start
 * @param data   Pointer to the region's data buffer
 * @return true if registered, false if table full or invalid parameters
 */
bool uds_register_memory_region(uint32_t start, uint32_t end, uint8_t *data);

/**
 * @brief Get the number of currently registered memory regions.
 * @return Count of registered regions (0..UDS_MEM_REGION_MAX)
 */
uint8_t uds_memory_region_count(void);

/**
 * @brief Clear all registered memory regions (reset to empty).
 */
void uds_memory_regions_clear(void);

#ifdef __cplusplus
}
#endif

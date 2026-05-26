/*
 * uds_svc_upload.h
 * Upload/Download Services — Module Header
 *
 * Implements handler functions for UDS upload/download services:
 *   0x34 — RequestDownload
 *   0x35 — RequestUpload
 *   0x36 — TransferData
 *   0x37 — RequestTransferExit
 *   0x38 — RequestFileTransfer
 *
 * Each handler follows the pattern:
 *   bool handler(const uds_request_t *req, uds_response_t *rsp, void *context);
 *
 * Services 0x34, 0x35, 0x36, 0x37 do NOT have subfunctions per ISO 14229-1.
 * The parser consumes byte[1] as subfunction — these handlers reconstruct
 * the original byte[1] via UDS_REQ_BYTE1() where needed.
 *
 * Service 0x38 (RequestFileTransfer) DOES have a subfunction and is handled
 * as a basic stub returning NRC 0x31 (requestOutOfRange).
 *
 * Context: uds_session_context_t* for session checks.
 * Security: uds_security_is_unlocked() for access control.
 *
 * Wave 3c Task 14 — Upload/Download Services
 */

#pragma once

#include "uds/uds_core.h"
#include "uds/uds_cfg.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Configuration Macros                                                     *
 * ======================================================================== */

/** @brief Size of the transfer data buffer (default 1024 bytes) */
#ifndef UDS_TRANSFER_BUFFER_SIZE
#define UDS_TRANSFER_BUFFER_SIZE  1024u
#endif

/** @brief Default max block length returned by RequestDownload/RequestUpload */
#ifndef UDS_DEFAULT_MAX_BLOCK_LENGTH
#define UDS_DEFAULT_MAX_BLOCK_LENGTH  256u
#endif

/* ======================================================================== *
 * Transfer Direction Enum                                                 *
 * ======================================================================== */

/** @brief Direction of an active transfer */
typedef enum {
    UDS_TRANSFER_NONE      = 0, /**< @brief No active transfer */
    UDS_TRANSFER_DOWNLOAD  = 1, /**< @brief Download (tester -> server) */
    UDS_TRANSFER_UPLOAD    = 2  /**< @brief Upload (server -> tester) */
} uds_transfer_direction_t;

/* ======================================================================== *
 * Memory Region Type                                                      *
 * ======================================================================== */

/** @brief A valid memory address range for upload/download operations */
typedef struct {
    uint32_t start_addr; /**< @brief First valid address (inclusive) */
    uint32_t end_addr;   /**< @brief Last valid address (inclusive) */
} uds_transfer_mem_region_t;

/* ======================================================================== *
 * Transfer Context Structure                                              *
 * ======================================================================== */

/** @brief Active transfer state context */
typedef struct {
    bool                     active;             /**< @brief Transfer in progress */
    uds_transfer_direction_t direction;          /**< @brief Download or upload */
    uint32_t                 memory_addr;        /**< @brief Start memory address */
    uint32_t                 remaining_size;     /**< @brief Remaining bytes to transfer */
    uint16_t                 max_block_length;   /**< @brief Maximum bytes per block */
    uint8_t                  block_seq_counter;  /**< @brief Block sequence counter (1-based) */
    uint8_t                  buffer[UDS_TRANSFER_BUFFER_SIZE]; /**< @brief Transfer data buffer */
    uint16_t                 buffer_length;      /**< @brief Valid bytes in buffer */
} uds_transfer_context_t;

/* ======================================================================== *
 * Handler Function Declarations                                           *
 * ======================================================================== */

/**
 * @brief 0x34 - RequestDownload
 *
 * Initiates a download (tester -> server) transfer.  Parses a 1-byte
 * dataFormatIdentifier, 4-byte memoryAddress, and 4-byte memorySize.
 * Validates session (programming/extended), security (unlocked), and
 * memory address range.
 *
 * Request:  [0x34][dataFormatIdentifier][memoryAddr(4)][memorySize(4)]
 * Response: [0x74][maxBlockLength_H][maxBlockLength_L]
 *
 * Context: uds_session_context_t* (must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD), 0x70 (UDNA)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Session context (must be non-NULL)
 * @return true to send response
 */
bool uds_svc_request_download(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context);

/**
 * @brief 0x35 - RequestUpload
 *
 * Initiates an upload (server -> tester) transfer.  Same format and
 * validation as RequestDownload.
 *
 * Request:  [0x35][dataFormatIdentifier][memoryAddr(4)][memorySize(4)]
 * Response: [0x75][maxBlockLength_H][maxBlockLength_L]
 *
 * Context: uds_session_context_t* (must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD), 0x70 (UDNA)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Session context (must be non-NULL)
 * @return true to send response
 */
bool uds_svc_request_upload(const uds_request_t *req,
                             uds_response_t      *rsp,
                             void                *context);

/**
 * @brief 0x36 - TransferData
 *
 * Transfers data blocks for an active download or upload operation.
 * The blockSequenceCounter is validated (starts at 1, increments by 1,
 * wraps at 0xFF -> 0x00).
 *
 * For download (tester -> server): request contains data bytes to store.
 * For upload   (server -> tester): response contains data bytes to send.
 *
 * Request (download): [0x36][blockSeqCounter][data...]
 * Request (upload):   [0x36][blockSeqCounter]
 * Response:           [0x76][blockSeqCounter_echo]
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x71 (TDS), 0x73 (WBSC)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Unused (pass NULL)
 * @return true to send response
 */
bool uds_svc_transfer_data(const uds_request_t *req,
                            uds_response_t      *rsp,
                            void                *context);

/**
 * @brief 0x37 - RequestTransferExit
 *
 * Terminates an active transfer and cleans up the transfer context.
 *
 * Request:  [0x37]
 * Response: [0x77]
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
bool uds_svc_request_transfer_exit(const uds_request_t *req,
                                    uds_response_t      *rsp,
                                    void                *context);

/**
 * @brief 0x38 - RequestFileTransfer
 *
 * Basic stub for file transfer operations.  Parses subfunction (addFile=0x01,
 * deleteFile=0x02, replaceFile=0x03, readFile=0x04) but returns NRC 0x31
 * for all operations (filesystem access not supported).
 *
 * Request:  [0x38][subfunction][filePathLength][filePath...][data...]
 * Response: [0x78][subfunction_echo]
 *
 * Context: uds_session_context_t* (must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD), 0x70 (UDNA)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Session context (must be non-NULL)
 * @return true to send response
 */
bool uds_svc_request_file_transfer(const uds_request_t *req,
                                    uds_response_t      *rsp,
                                    void                *context);

/* ======================================================================== *
 * Transfer Context Management API                                         *
 * ======================================================================== */

/**
 * @brief Initialise the transfer context to idle state.
 *
 * Clears all fields: active=false, direction=NONE, buffer cleared.
 * Must be called once during initialisation.
 */
void uds_svc_transfer_init(void);

/**
 * @brief Get a pointer to the current transfer context (for inspection).
 *
 * @return Pointer to the static transfer context
 */
const uds_transfer_context_t* uds_svc_transfer_get_context(void);

/**
 * @brief Get a pointer to the transfer data buffer (for test/manual fill).
 *
 * Returns a non-const pointer so that test code can pre-fill the buffer
 * with data before initiating an upload transfer.
 *
 * @return Pointer to the transfer buffer (length UDS_TRANSFER_BUFFER_SIZE)
 */
uint8_t* uds_svc_transfer_get_buffer(void);

/**
 * @brief Register a valid memory address region for upload/download.
 *
 * The region table can hold up to UDS_MEM_REGION_MAX entries.  Duplicate
 * or overlapping entries are not checked.  If the table is full the new
 * region is silently ignored.
 *
 * @param[in] start_addr  Start address (inclusive)
 * @param[in] end_addr    End address (inclusive, must be >= start_addr)
 */
void uds_svc_transfer_register_memory_region(uint32_t start_addr,
                                              uint32_t end_addr);

/**
 * @brief Get the number of currently registered memory regions.
 *
 * @return Number of registered regions
 */
uint16_t uds_svc_transfer_get_region_count(void);

/**
 * @brief Get a pointer to the memory region table.
 *
 * @return Pointer to the static region array
 */
const uds_transfer_mem_region_t* uds_svc_transfer_get_regions(void);

#ifdef __cplusplus
}
#endif

/*
 * uds_svc_diagcomm.h
 * Diagnostic & Communication Management Services — Module Header
 *
 * Implements handler functions for UDS services 0x10, 0x11, 0x27, 0x28,
 * 0x3E, 0x85, 0x86, and 0x87 per ISO 14229-1:2020.
 *
 * Each handler follows the pattern:
 *   bool handler(const uds_request_t *req, uds_response_t *rsp, void *context);
 *
 * Return value: true  = send response (rsp populated with positive or negative)
 *               false = suppress response (SPRMIB bit set on positive result)
 *
 * Wave 3a Task 9 — Diagnostic & Communication Management Services
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

/** @brief Maximum number of stored event configurations for ResponseOnEvent */
#ifndef UDS_EVENT_STORE_MAX
#define UDS_EVENT_STORE_MAX  4u
#endif

/* ======================================================================== *
 * Event Store Types (used by ResponseOnEvent 0x86)                        *
 * ======================================================================== */

/** @brief A single event record in the ResponseOnEvent store */
typedef struct {
    uint8_t  event_type;       /**< @brief Event type identifier */
    uint8_t  data[8];          /**< @brief Event configuration data */
    uint16_t data_len;         /**< @brief Length of valid data in data[] */
    bool     active;           /**< @brief Whether this event is active */
} uds_event_record_t;

/* ======================================================================== *
 * Handler Function Declarations                                            *
 * ======================================================================== */

/**
 * @brief 0x10 — DiagnosticSessionControl
 *
 * Accepts a session type via req->subfunction.value, validates length,
 * calls uds_session_switch(), and populates the response with the
 * sessionParameterRecord (4 bytes: P2Server_max + P2*Server_max).
 *
 * Context: uds_session_context_t* (must be non-NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure (positive or negative)
 * @param[in,out] context  Session context pointer (must be non-NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_diagnostic_session_control(const uds_request_t *req,
                                        uds_response_t      *rsp,
                                        void                *context);

/**
 * @brief 0x11 — ECUReset
 *
 * Accepts a resetType SubFunction: 0x01 (hardReset), 0x02 (keyOffOnReset),
 * 0x03 (softReset).  On success returns powerDownTime = 0x00.
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC), 0x33 (SAD)
 *
 * @param[in]     req     Parsed UDS request
 * @param[out]    rsp     Response structure
 * @param[in,out] context Unused (pass NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_ecu_reset(const uds_request_t *req,
                       uds_response_t      *rsp,
                       void                *context);

/**
 * @brief 0x27 — SecurityAccess
 *
 * requestSeed (odd subfunction): returns seed data from the security module.
 * sendKey (even subfunction): validates key, on success unlocks the level.
 *
 * If context is non-NULL, the caller's bool is updated with the new
 * unlock state after a successful sendKey.
 *
 * Context: bool* (optional, pointer to unlocked flag)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x24 (RSE), 0x31 (ROOR),
 *      0x35 (IK), 0x36 (ENOA), 0x37 (RTDNE)
 *
 * @param[in]     req     Parsed UDS request
 * @param[out]    rsp     Response structure
 * @param[in,out] context Pointer to bool unlocked flag (may be NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_security_access(const uds_request_t *req,
                             uds_response_t      *rsp,
                             void                *context);

/**
 * @brief 0x28 — CommunicationControl
 *
 * Accepts controlType (subfunction) and communicationType (data byte 0).
 * Validates both are in known ranges.
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR)
 *
 * @param[in]     req     Parsed UDS request
 * @param[out]    rsp     Response structure
 * @param[in,out] context Unused (pass NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_communication_control(const uds_request_t *req,
                                   uds_response_t      *rsp,
                                   void                *context);

/**
 * @brief 0x3E — TesterPresent
 *
 * Echoes the SubFunction value in the positive response.
 * If the suppressPosRspMsgIndicationBit is set, the response is suppressed.
 * This service keeps the session alive.
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF)
 *
 * @param[in]     req     Parsed UDS request
 * @param[out]    rsp     Response structure
 * @param[in,out] context Unused (pass NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_tester_present(const uds_request_t *req,
                            uds_response_t      *rsp,
                            void                *context);

/**
 * @brief 0x85 — ControlDTCSetting
 *
 * Accepts dtcSettingType SubFunction: 0x01 (on), 0x02 (off).
 * Requires security unlocked via context.
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC), 0x33 (SAD)
 *
 * @param[in]     req     Parsed UDS request
 * @param[out]    rsp     Response structure
 * @param[in]     context Pointer to bool unlocked flag (must be non-NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_control_dtc_setting(const uds_request_t *req,
                                 uds_response_t      *rsp,
                                 void                *context);

/**
 * @brief 0x86 — ResponseOnEvent
 *
 * Configures and controls event-triggered responses.  Supports simplified
 * event storage (up to UDS_EVENT_STORE_MAX events).
 *
 * SubFunction values:
 *   0x00 = stopResponseOnEvent
 *   0x01 = startResponseOnEvent
 *   0x02 = clearResponseOnEvent
 *   0x03 = startResponseOnEventWithStorage
 *   0x04 = startResponseOnEventWithStorageAndClearResponseOnEvent
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR)
 *
 * @param[in]     req     Parsed UDS request
 * @param[out]    rsp     Response structure
 * @param[in,out] context Unused (pass NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_response_on_event(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context);

/**
 * @brief 0x87 — LinkControl
 *
 * Supports verifyBaudrateTransition (0x01) and transitionBaudrate (0x02).
 * For verify mode, data must contain a 3-byte baudrate identifier.
 *
 * Context: unused (pass NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC)
 *
 * @param[in]     req     Parsed UDS request
 * @param[out]    rsp     Response structure
 * @param[in,out] context Unused (pass NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_link_control(const uds_request_t *req,
                          uds_response_t      *rsp,
                          void                *context);

/* ======================================================================== *
 * Event Store API (for ResponseOnEvent 0x86)                               *
 * ======================================================================== */

/**
 * @brief Initialise the ResponseOnEvent store.
 *
 * Clears all event records.
 */
void uds_svc_event_store_init(void);

/**
 * @brief Clear all stored event records.
 */
void uds_svc_event_store_clear(void);

/**
 * @brief Get a pointer to the event store for inspection.
 *
 * @param[out] count  Optional: receives the number of event store entries
 * @return Pointer to the static event store array (size UDS_EVENT_STORE_MAX)
 */
const uds_event_record_t* uds_svc_event_store_get(uint8_t *count);

#ifdef __cplusplus
}
#endif

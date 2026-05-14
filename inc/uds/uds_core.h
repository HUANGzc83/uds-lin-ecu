/*
 * uds_core.h
 * UDS Core Types & Defines
 *
 * ISO 14229-1:2020 service identifiers, negative response codes,
 * message structures, and helper macros for UDS protocol implementation.
 *
 * Wave 1 Task 3 — UDS Core Types & Defines
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Service Identifiers (SIDs) — ISO 14229-1:2020 Table 2                     *
 * ======================================================================== */

/** @brief UDS request service identifiers (bit 6 = 0) */
typedef enum {
    DIAGNOSTIC_SESSION_CONTROL          = 0x10, /**< @brief 0x10 — DiagnosticSessionControl */
    ECU_RESET                           = 0x11, /**< @brief 0x11 — ECUReset */
    CLEAR_DIAGNOSTIC_INFO               = 0x14, /**< @brief 0x14 — ClearDiagnosticInformation */
    READ_DTC_INFO                       = 0x19, /**< @brief 0x19 — ReadDTCInformation */
    READ_DATA_BY_IDENTIFIER             = 0x22, /**< @brief 0x22 — ReadDataByIdentifier */
    READ_MEMORY_BY_ADDRESS              = 0x23, /**< @brief 0x23 — ReadMemoryByAddress */
    READ_SCALING_DATA_BY_ID             = 0x24, /**< @brief 0x24 — ReadScalingDataByIdentifier */
    SECURITY_ACCESS                     = 0x27, /**< @brief 0x27 — SecurityAccess */
    COMMUNICATION_CONTROL               = 0x28, /**< @brief 0x28 — CommunicationControl */
    AUTHENTICATION                      = 0x29, /**< @brief 0x29 — Authentication */
    READ_DATA_BY_PERIODIC_ID            = 0x2A, /**< @brief 0x2A — ReadDataByPeriodicIdentifier */
    DYNAMICALLY_DEFINE_DATA_ID          = 0x2C, /**< @brief 0x2C — DynamicallyDefineDataIdentifier */
    WRITE_DATA_BY_IDENTIFIER            = 0x2E, /**< @brief 0x2E — WriteDataByIdentifier */
    INPUT_OUTPUT_CONTROL_BY_ID          = 0x2F, /**< @brief 0x2F — InputOutputControlByIdentifier */
    ROUTINE_CONTROL                     = 0x31, /**< @brief 0x31 — RoutineControl */
    REQUEST_DOWNLOAD                    = 0x34, /**< @brief 0x34 — RequestDownload */
    REQUEST_UPLOAD                      = 0x35, /**< @brief 0x35 — RequestUpload */
    TRANSFER_DATA                       = 0x36, /**< @brief 0x36 — TransferData */
    REQUEST_TRANSFER_EXIT               = 0x37, /**< @brief 0x37 — RequestTransferExit */
    REQUEST_FILE_TRANSFER               = 0x38, /**< @brief 0x38 — RequestFileTransfer */
    WRITE_MEMORY_BY_ADDRESS             = 0x3D, /**< @brief 0x3D — WriteMemoryByAddress */
    TESTER_PRESENT                      = 0x3E, /**< @brief 0x3E — TesterPresent */
    SECURED_DATA_TRANSMISSION           = 0x84, /**< @brief 0x84 — SecuredDataTransmission */
    CONTROL_DTC_SETTING                 = 0x85, /**< @brief 0x85 — ControlDTCSetting */
    RESPONSE_ON_EVENT                   = 0x86, /**< @brief 0x86 — ResponseOnEvent */
    LINK_CONTROL                        = 0x87  /**< @brief 0x87 — LinkControl */
} uds_sid_t;

/** @brief UDS positive response service identifiers (request SID + 0x40, bit 6 = 1) */
typedef enum {
    DIAGNOSTIC_SESSION_CONTROL_RSP      = 0x50, /**< @brief 0x50 — DiagnosticSessionControl positive response */
    ECU_RESET_RSP                       = 0x51, /**< @brief 0x51 — ECUReset positive response */
    CLEAR_DIAGNOSTIC_INFO_RSP           = 0x54, /**< @brief 0x54 — ClearDiagnosticInformation positive response */
    READ_DTC_INFO_RSP                   = 0x59, /**< @brief 0x59 — ReadDTCInformation positive response */
    READ_DATA_BY_IDENTIFIER_RSP         = 0x62, /**< @brief 0x62 — ReadDataByIdentifier positive response */
    READ_MEMORY_BY_ADDRESS_RSP          = 0x63, /**< @brief 0x63 — ReadMemoryByAddress positive response */
    READ_SCALING_DATA_BY_ID_RSP         = 0x64, /**< @brief 0x64 — ReadScalingDataByIdentifier positive response */
    SECURITY_ACCESS_RSP                 = 0x67, /**< @brief 0x67 — SecurityAccess positive response */
    COMMUNICATION_CONTROL_RSP           = 0x68, /**< @brief 0x68 — CommunicationControl positive response */
    AUTHENTICATION_RSP                  = 0x69, /**< @brief 0x69 — Authentication positive response */
    READ_DATA_BY_PERIODIC_ID_RSP        = 0x6A, /**< @brief 0x6A — ReadDataByPeriodicIdentifier positive response */
    DYNAMICALLY_DEFINE_DATA_ID_RSP      = 0x6C, /**< @brief 0x6C — DynamicallyDefineDataIdentifier positive response */
    WRITE_DATA_BY_IDENTIFIER_RSP        = 0x6E, /**< @brief 0x6E — WriteDataByIdentifier positive response */
    INPUT_OUTPUT_CONTROL_BY_ID_RSP      = 0x6F, /**< @brief 0x6F — InputOutputControlByIdentifier positive response */
    ROUTINE_CONTROL_RSP                 = 0x71, /**< @brief 0x71 — RoutineControl positive response */
    REQUEST_DOWNLOAD_RSP                = 0x74, /**< @brief 0x74 — RequestDownload positive response */
    REQUEST_UPLOAD_RSP                  = 0x75, /**< @brief 0x75 — RequestUpload positive response */
    TRANSFER_DATA_RSP                   = 0x76, /**< @brief 0x76 — TransferData positive response */
    REQUEST_TRANSFER_EXIT_RSP           = 0x77, /**< @brief 0x77 — RequestTransferExit positive response */
    REQUEST_FILE_TRANSFER_RSP           = 0x78, /**< @brief 0x78 — RequestFileTransfer positive response */
    WRITE_MEMORY_BY_ADDRESS_RSP         = 0x7D, /**< @brief 0x7D — WriteMemoryByAddress positive response */
    TESTER_PRESENT_RSP                  = 0x7E, /**< @brief 0x7E — TesterPresent positive response */
    SECURED_DATA_TRANSMISSION_RSP       = 0xC4, /**< @brief 0xC4 — SecuredDataTransmission positive response */
    CONTROL_DTC_SETTING_RSP             = 0xC5, /**< @brief 0xC5 — ControlDTCSetting positive response */
    RESPONSE_ON_EVENT_RSP               = 0xC6, /**< @brief 0xC6 — ResponseOnEvent positive response */
    LINK_CONTROL_RSP                    = 0xC7  /**< @brief 0xC7 — LinkControl positive response */
} uds_response_sid_t;

/* ======================================================================== *
 * Negative Response Codes — ISO 14229-1:2020 Annex A, Table A.1            *
 * ======================================================================== */

/** @brief UDS negative response codes */
typedef enum {
    /* --- Internal / pseudo-NRC (not sent in negative response messages) --- */
    NRC_POSITIVE_RESPONSE                       = 0x00, /**< @brief 0x00 — positiveResponse (PR, server internal use only) */

    /* --- Communication related NRCs (0x10–0x3A) --- */
    NRC_GENERAL_REJECT                          = 0x10, /**< @brief 0x10 — generalReject (GR) */
    NRC_SERVICE_NOT_SUPPORTED                   = 0x11, /**< @brief 0x11 — serviceNotSupported (SNS) */
    NRC_SUB_FUNCTION_NOT_SUPPORTED              = 0x12, /**< @brief 0x12 — subFunctionNotSupported (SFNS) */
    NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT = 0x13, /**< @brief 0x13 — incorrectMessageLengthOrInvalidFormat (IMLOIF) */
    NRC_RESPONSE_TOO_LONG                       = 0x14, /**< @brief 0x14 — responseTooLong (RTL) */
    NRC_BUSY_REPEAT_REQUEST                     = 0x21, /**< @brief 0x21 — busyRepeatRequest (BRR) */
    NRC_CONDITIONS_NOT_CORRECT                  = 0x22, /**< @brief 0x22 — conditionsNotCorrect (CNC) */
    NRC_REQUEST_SEQUENCE_ERROR                  = 0x24, /**< @brief 0x24 — requestSequenceError (RSE) */
    NRC_NO_RESPONSE_FROM_SUBNET_COMPONENT       = 0x25, /**< @brief 0x25 — noResponseFromSubnetComponent (NRFSC) */
    NRC_FAILURE_PREVENTS_EXECUTION              = 0x26, /**< @brief 0x26 — failurePreventsExecutionOfRequestedAction (FPEORA) */
    NRC_REQUEST_OUT_OF_RANGE                    = 0x31, /**< @brief 0x31 — requestOutOfRange (ROOR) */
    NRC_SECURITY_ACCESS_DENIED                  = 0x33, /**< @brief 0x33 — securityAccessDenied (SAD) */
    NRC_AUTHENTICATION_REQUIRED                 = 0x34, /**< @brief 0x34 — authenticationRequired (AR) */
    NRC_INVALID_KEY                             = 0x35, /**< @brief 0x35 — invalidKey (IK) */
    NRC_EXCEED_NUMBER_OF_ATTEMPTS               = 0x36, /**< @brief 0x36 — exceedNumberOfAttempts (ENOA) */
    NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED         = 0x37, /**< @brief 0x37 — requiredTimeDelayNotExpired (RTDNE) */
    NRC_SECURE_DATA_TRANSMISSION_REQUIRED       = 0x38, /**< @brief 0x38 — secureDataTransmissionRequired (SDTR) */
    NRC_SECURE_DATA_TRANSMISSION_NOT_ALLOWED    = 0x39, /**< @brief 0x39 — secureDataTransmissionNotAllowed (SDTNA) */
    NRC_SECURE_DATA_VERIFICATION_FAILED         = 0x3A, /**< @brief 0x3A — secureDataVerificationFailed (SDTF) */

    /* --- Certificate / ownership / authentication NRCs (0x50–0x5D) --- */
    NRC_CERT_VERIFICATION_FAILED_TIME_PERIOD    = 0x50, /**< @brief 0x50 — Certificate verification failed: Invalid Time Period (CVFITP) */
    NRC_CERT_VERIFICATION_FAILED_SIGNATURE      = 0x51, /**< @brief 0x51 — Certificate verification failed: Invalid Signature (CVFIS) */
    NRC_CERT_VERIFICATION_FAILED_CHAIN_OF_TRUST = 0x52, /**< @brief 0x52 — Certificate verification failed: Invalid Chain of Trust (CVFICOT) */
    NRC_CERT_VERIFICATION_FAILED_TYPE           = 0x53, /**< @brief 0x53 — Certificate verification failed: Invalid Type (CVFIT) */
    NRC_CERT_VERIFICATION_FAILED_FORMAT         = 0x54, /**< @brief 0x54 — Certificate verification failed: Invalid Format (CVFIF) */
    NRC_CERT_VERIFICATION_FAILED_CONTENT        = 0x55, /**< @brief 0x55 — Certificate verification failed: Invalid Content (CVFIC) */
    NRC_CERT_VERIFICATION_FAILED_SCOPE          = 0x56, /**< @brief 0x56 — Certificate verification failed: Invalid Scope (CVFIS) */
    NRC_CERT_VERIFICATION_FAILED_CERTIFICATE    = 0x57, /**< @brief 0x57 — Certificate verification failed: Invalid Certificate / revoked (CVFIC) */
    NRC_OWNERSHIP_VERIFICATION_FAILED           = 0x58, /**< @brief 0x58 — Ownership verification failed (OVF) */
    NRC_CHALLENGE_CALCULATION_FAILED            = 0x59, /**< @brief 0x59 — Challenge calculation failed (CCF) */
    NRC_SETTING_ACCESS_RIGHTS_FAILED            = 0x5A, /**< @brief 0x5A — Setting Access Rights failed (SARF) */
    NRC_SESSION_KEY_CREATION_DERIVATION_FAILED  = 0x5B, /**< @brief 0x5B — Session key creation/derivation failed (SKCDF) */
    NRC_CONFIGURATION_DATA_USAGE_FAILED         = 0x5C, /**< @brief 0x5C — Configuration data usage failed (CDUF) */
    NRC_DEAUTHENTICATION_FAILED                 = 0x5D, /**< @brief 0x5D — DeAuthentication failed (DAF) */

    /* --- Upload / download NRCs (0x70–0x73) --- */
    NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED            = 0x70, /**< @brief 0x70 — uploadDownloadNotAccepted (UDNA) */
    NRC_TRANSFER_DATA_SUSPENDED                 = 0x71, /**< @brief 0x71 — transferDataSuspended (TDS) */
    NRC_GENERAL_PROGRAMMING_FAILURE             = 0x72, /**< @brief 0x72 — generalProgrammingFailure (GPF) */
    NRC_WRONG_BLOCK_SEQUENCE_COUNTER            = 0x73, /**< @brief 0x73 — wrongBlockSequenceCounter (WBSC) */

    /* --- Session / pending NRCs --- */
    NRC_REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING = 0x78, /**< @brief 0x78 — requestCorrectlyReceived-ResponsePending (RCRRP) */
    NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION = 0x7E, /**< @brief 0x7E — subFunctionNotSupportedInActiveSession (SFNSIAS) */
    NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION     = 0x7F, /**< @brief 0x7F — serviceNotSupportedInActiveSession (SNSIAS) */

    /* --- Engine / vehicle condition NRCs (0x81–0x93) --- */
    NRC_RPM_TOO_HIGH                            = 0x81, /**< @brief 0x81 — rpmTooHigh (RPMTH) */
    NRC_RPM_TOO_LOW                             = 0x82, /**< @brief 0x82 — rpmTooLow (RPMTL) */
    NRC_ENGINE_IS_RUNNING                       = 0x83, /**< @brief 0x83 — engineIsRunning (EIR) */
    NRC_ENGINE_IS_NOT_RUNNING                   = 0x84, /**< @brief 0x84 — engineIsNotRunning (EINR) */
    NRC_ENGINE_RUN_TIME_TOO_LOW                 = 0x85, /**< @brief 0x85 — engineRunTimeTooLow (ERTTL) */
    NRC_TEMPERATURE_TOO_HIGH                    = 0x86, /**< @brief 0x86 — temperatureTooHigh (TEMPTH) */
    NRC_TEMPERATURE_TOO_LOW                     = 0x87, /**< @brief 0x87 — temperatureTooLow (TEMPTL) */
    NRC_VEHICLE_SPEED_TOO_HIGH                  = 0x88, /**< @brief 0x88 — vehicleSpeedTooHigh (VSTH) */
    NRC_VEHICLE_SPEED_TOO_LOW                   = 0x89, /**< @brief 0x89 — vehicleSpeedTooLow (VSTL) */
    NRC_THROTTLE_PEDAL_TOO_HIGH                 = 0x8A, /**< @brief 0x8A — throttle/PedalTooHigh (TPTH) */
    NRC_THROTTLE_PEDAL_TOO_LOW                  = 0x8B, /**< @brief 0x8B — throttle/PedalTooLow (TPTL) */
    NRC_TRANSMISSION_RANGE_NOT_IN_NEUTRAL       = 0x8C, /**< @brief 0x8C — transmissionRangeNotInNeutral (TRNIN) */
    NRC_TRANSMISSION_RANGE_NOT_IN_GEAR          = 0x8D, /**< @brief 0x8D — transmissionRangeNotInGear (TRNIG) */
    NRC_BRAKE_SWITCH_NOT_CLOSED                 = 0x8F, /**< @brief 0x8F — brakeSwitch(es)NotClosed (BSNC) */
    NRC_SHIFTER_LEVER_NOT_IN_PARK               = 0x90, /**< @brief 0x90 — shifterLeverNotInPark (SLNIP) */
    NRC_TORQUE_CONVERTER_CLUTCH_LOCKED          = 0x91, /**< @brief 0x91 — torqueConverterClutchLocked (TCCL) */
    NRC_VOLTAGE_TOO_HIGH                        = 0x92, /**< @brief 0x92 — voltageTooHigh (VTH) */
    NRC_VOLTAGE_TOO_LOW                         = 0x93, /**< @brief 0x93 — voltageTooLow (VTL) */

    /* --- Resource NRC --- */
    NRC_RESOURCE_TEMPORARILY_NOT_AVAILABLE      = 0x94  /**< @brief 0x94 — resourceTemporarilyNotAvailable (RTNA) */
} uds_nrc_t;

/* ======================================================================== *
 * Subfunction Parameter Structure                                           *
 * ======================================================================== */

/** @brief UDS subfunction parameter byte (bit 7 = suppressPosRspMsgIndicationBit) */
typedef struct __attribute__((packed)) {
    uint8_t value        : 7; /**< @brief Subfunction value (bits 6:0) */
    uint8_t suppress_rsp : 1; /**< @brief Suppress positive response indication (bit 7) */
} uds_subfunction_t;

/* ======================================================================== *
 * Addressing Type Enum                                                     *
 * ======================================================================== */

/** @brief UDS target address type */
typedef enum {
    UDS_PHYSICAL  = 0, /**< @brief Physical addressing (point-to-point) */
    UDS_FUNCTIONAL = 1  /**< @brief Functional addressing (broadcast / multi-node) */
} uds_addressing_t;

/* ======================================================================== *
 * Diagnostic Session Types                                                 *
 * ======================================================================== */

/** @brief UDS diagnostic session types (ISO 14229-1 Table 25) */
typedef enum {
    UDS_DEFAULT_SESSION     = 0x01, /**< @brief 0x01 — defaultSession */
    UDS_PROGRAMMING_SESSION = 0x02, /**< @brief 0x02 — programmingSession */
    UDS_EXTENDED_SESSION    = 0x03  /**< @brief 0x03 — extendedDiagnosticSession */
} uds_session_t;

/* ======================================================================== *
 * Standard Diagnostic Session Return Parameters (Table 28/29)              *
 * ======================================================================== */

/** @brief Session timing parameters returned in DiagnosticSessionControl positive response */
typedef struct {
    uint16_t p2_server_max;      /**< @brief P2Server_max timing value in ms */
    uint16_t p2_star_server_max; /**< @brief P2StarServer_max timing value in ms */
} uds_std_return_t;

/* ======================================================================== *
 * UDS Message Container                                                    *
 * ======================================================================== */

/** @brief UDS message structure for request/response PDU transport */
typedef struct {
    uds_addressing_t ta_type; /**< @brief Target address type (physical or functional) */
    uint8_t          nad;     /**< @brief Node / network address identifier */
    uint8_t         *data;    /**< @brief Pointer to message payload buffer (SID + parameters) */
    uint16_t         len;     /**< @brief Length of the payload data in bytes */
} uds_msg_t;

/* ======================================================================== *
 * Helper Macros                                                            *
 * ======================================================================== */

/** @brief Check if a SID is a valid request SID (0x10–0x3E or 0x84–0x87) */
#define IS_REQUEST_SID(x)       (((x) >= 0x10 && (x) <= 0x3E) || ((x) >= 0x84 && (x) <= 0x87))

/** @brief Check if a SID is a valid positive response SID (0x50–0x7E or 0xC4–0xC7) */
#define IS_RESPONSE_SID(x)      (((x) >= 0x50 && (x) <= 0x7E) || ((x) >= 0xC4 && (x) <= 0xC7))

/** @brief Convert a request SID to its corresponding positive response SID (request + 0x40) */
#define SID_TO_RESPONSE(sid)    ((uint8_t)((sid) + 0x40))

/** @brief Convert a positive response SID back to the original request SID */
#define SID_FROM_RESPONSE(sid)  ((uint8_t)((sid) - 0x40))

/** @brief Check if a response PDU indicates a positive response (first byte != 0x7F) */
#define IS_POSITIVE_RESPONSE(data) ((data)[0] != 0x7F)

/* ======================================================================== *
 * Parser / Serializer Types — Wave 2 Task 6                                *
 * ======================================================================== */

/** @brief Parser/serializer operation status codes */
typedef enum {
    UDS_OK              =  0, /**< @brief Operation completed successfully */
    UDS_ERR_PARSE       = -1, /**< @brief Failed to parse input data (e.g., NULL pointer) */
    UDS_ERR_SERIALIZE   = -2, /**< @brief Failed to serialize output (e.g., buffer too small) */
    UDS_ERR_TOO_SHORT   = -3, /**< @brief Input buffer too short to contain a valid UDS message */
    UDS_ERR_INVALID_SID = -4  /**< @brief Service identifier out of valid range */
} uds_status_t;

/** @brief Parsed UDS request structure */
typedef struct {
    uint8_t           sid;           /**< @brief Service identifier */
    uds_subfunction_t subfunction;   /**< @brief Subfunction parameter (zeroed if absent) */
    const uint8_t    *data;          /**< @brief Pointer to request data following SID(/SubFunction) */
    uint16_t          data_len;      /**< @brief Number of bytes in data */
} uds_request_t;

/** @brief UDS response structure for serialization */
typedef struct {
    uint8_t        sid;           /**< @brief Positive response SID (request SID + 0x40) */
    uint8_t        subfunc_echo;  /**< @brief Subfunction echo byte (0 if no subfunction) */
    const uint8_t *data;          /**< @brief Pointer to response payload data */
    uint16_t       data_len;      /**< @brief Number of payload bytes */
} uds_response_t;

/* ======================================================================== *
 * Parser / Serializer Function Declarations — Wave 2 Task 6                *
 * ======================================================================== */

/**
 * @brief Parse raw UDS request bytes into a structured uds_request_t.
 *
 * Extracts SID, optional SubFunction byte (bits 6:0 → value, bit 7 → suppress_rsp),
 * and a pointer to any remaining data.
 *
 * @param[in]  raw  Pointer to raw byte buffer
 * @param[in]  len  Length of raw buffer in bytes
 * @param[out] req  Filled on success with parsed fields
 * @return UDS_OK on success, or an error code:
 *         - UDS_ERR_TOO_SHORT if len == 0
 *         - UDS_ERR_PARSE if raw == NULL or req == NULL
 */
uds_status_t uds_parse_request(const uint8_t *raw, uint16_t len, uds_request_t *req);

/**
 * @brief Serialize a uds_response_t into a raw byte buffer.
 *
 * Writes SID, subfunction echo, and data payload sequentially.
 *
 * @param[in]  rsp  Pointer to response struct to serialize
 * @param[out] buf  Destination buffer for serialized bytes
 * @param[in,out] len  On input: capacity of buf. On success: bytes written.
 * @return UDS_OK on success, or an error code:
 *         - UDS_ERR_PARSE if rsp == NULL, buf == NULL, or len == NULL
 *         - UDS_ERR_SERIALIZE if buffer capacity is insufficient
 */
uds_status_t uds_serialize_response(const uds_response_t *rsp, uint8_t *buf, uint16_t *len);

/**
 * @brief Serialize a UDS negative response: [0x7F][SID][NRC].
 *
 * @param[in]  sid  Request SID that was rejected
 * @param[in]  nrc  Negative response code
 * @param[out] buf  Destination buffer (must be at least 3 bytes)
 * @param[in,out] len  On input: capacity of buf. On success: bytes written (3).
 * @return UDS_OK on success, or an error code:
 *         - UDS_ERR_PARSE if buf == NULL or len == NULL
 *         - UDS_ERR_SERIALIZE if buffer capacity < 3
 */
uds_status_t uds_serialize_negative_response(uint8_t sid, uds_nrc_t nrc, uint8_t *buf, uint16_t *len);

/**
 * @brief Check whether a response PDU starts with a positive response indicator.
 *
 * Positive responses have first byte != 0x7F (the negative response prefix).
 *
 * @param[in]  data  Pointer to the response PDU
 * @return true if first byte != 0x7F, false otherwise
 */
bool uds_is_positive_response(const uint8_t *data);

/**
 * @brief Convert a request SID to its corresponding positive response SID.
 *
 * Positive response SID = request SID + 0x40.
 *
 * @param[in]  sid  Request service identifier
 * @return Positive response service identifier
 */
uint8_t uds_sid_to_response_sid(uint8_t sid);

#ifdef __cplusplus
}
#endif

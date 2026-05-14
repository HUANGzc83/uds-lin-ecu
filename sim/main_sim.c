/*
 * main_sim.c
 * UDS-LIN ECU PC Simulator — Main Entry Point
 *
 * Task 21 — PC Simulation Main + Integration Test Runner
 *
 * Implements a sequential (non-real-time) simulation of a UDS-over-LIN
 * ECU.  The main loop polls a mock UART for incoming LIN frames, decodes
 * UDS requests, dispatches to the appropriate service handler, serializes
 * the response, and transmits it back over the mock UART.
 *
 * All UDS modules are initialised in sim_init() before entering the loop.
 */

#include "sim_cfg.h"

#include "uds/uds_core.h"
#include "uds/uds_session.h"
#include "uds/uds_security.h"
#include "uds/uds_dtc.h"
#include "uds/uds_data.h"
#include "uds/uds_service.h"
#include "uds/uds_lin_transport.h"
#include "uds/uds_svc_diagcomm.h"
#include "uds/uds_svc_data.h"
#include "uds/uds_svc_io.h"
#include "uds/uds_svc_routine.h"
#include "uds/uds_svc_upload.h"
#include "uds/uds_svc_auth.h"
#include "uds/uds_svc_stored.h"

#include "mock_all.h"

#include <stdio.h>
#include <string.h>

/* ======================================================================== *
 * Simulator Context                                                        *
 * ======================================================================== *
 * Tracks the combined state needed by all UDS service handlers.
 * Different handlers cast the context pointer to different types:
 *
 *   uds_session_context_t*  — DiagnosticSessionControl, upload/download, auth
 *   bool*                    — SecurityAccess, ReadDataByIdentifier,
 *                              WriteDataByIdentifier, IOControl, RoutineControl,
 *                              ControlDTCSetting
 *   uds_svc_stored_ctx_t*   — ClearDiagnosticInformation
 *   NULL                     — ECUReset, TesterPresent, CommunicationControl,
 *                              ResponseOnEvent, LinkControl, ReadDTCInfo,
 *                              ReadMemoryByAddress, TransferData, etc.
 *
 * The main loop selects the correct sub-pointer for each SID when calling
 * dispatch, satisfying all casts while keeping state in one place.
 */

typedef struct {
    uds_session_context_t  session;          /**< @brief Session state machine context */
    bool                   unlocked;         /**< @brief Security unlock flag (managed by handlers) */
} sim_context_t;

/* ======================================================================== *
 * Static State                                                             *
 * ======================================================================== */

/** @brief Global simulator context */
static sim_context_t        g_sim_ctx;

/** @brief Stored context for 0x14 (ClearDiagnosticInformation) */
static uds_svc_stored_ctx_t g_stored_ctx;

/* ======================================================================== *
 * Helper: Select the dispatch context pointer for a given SID              *
 * ======================================================================== *
 * Returns the appropriate pointer to pass as the `context` argument to
 * uds_service_dispatch().  The returned pointer aliases fields within
 * the global g_sim_ctx and g_stored_ctx.
 */

static void* get_dispatch_context(uint8_t sid)
{
    switch (sid)
    {
    /* --- Session context required --- */
    case DIAGNOSTIC_SESSION_CONTROL:
    case REQUEST_DOWNLOAD:
    case REQUEST_UPLOAD:
    case REQUEST_FILE_TRANSFER:
    case AUTHENTICATION:
        return &g_sim_ctx.session;

    /* --- Bool unlocked flag required --- */
    case SECURITY_ACCESS:
    case READ_DATA_BY_IDENTIFIER:
    case READ_SCALING_DATA_BY_ID:
    case WRITE_DATA_BY_IDENTIFIER:
    case WRITE_MEMORY_BY_ADDRESS:
    case INPUT_OUTPUT_CONTROL_BY_ID:
    case ROUTINE_CONTROL:
    case CONTROL_DTC_SETTING:
        return &g_sim_ctx.unlocked;

    /* --- Composite stored context --- */
    case CLEAR_DIAGNOSTIC_INFO:
        g_stored_ctx.sctx     = &g_sim_ctx.session;
        g_stored_ctx.unlocked = &g_sim_ctx.unlocked;
        return &g_stored_ctx;

    /* --- Handlers that ignore context --- */
    default:
        return NULL;
    }
}

/* ======================================================================== *
 * Post-Dispatch Synchronisation                                           *
 * ======================================================================== *
 * After each dispatch some handlers modify internal module state without
 * updating our context bools.  This function keeps everything consistent.
 */

static void sync_after_dispatch(uint8_t sid, bool response_sent,
                                const uds_response_t *rsp)
{
    (void)response_sent;

    /* --- DiagnosticSessionControl: sync security on session switch --- */
    if (sid == DIAGNOSTIC_SESSION_CONTROL)
    {
        if (rsp != NULL && rsp->sid == DIAGNOSTIC_SESSION_CONTROL_RSP)
        {
            /* Successful session switch — if the session context says
             * security is now locked, propagate to the security module
             * and our unlocked flag. */
            if (g_sim_ctx.session.security_locked)
            {
                uds_security_lock();
                g_sim_ctx.unlocked = false;
            }
        }
    }

    /* --- Always sync unlocked flag from the security module --- *
     * The security_access handler writes true to our context bool
     * on successful sendKey.  Other paths (e.g. session switch) may
     * lock via uds_security_lock().  Syncing here catches both. */
    g_sim_ctx.unlocked = uds_security_is_unlocked();
}

/*
 * sim_init: Initialise All UDS Modules
 *
 * Called once before entering the main loop.  Initialises every UDS module
 * and the mock HAL layer.
 */

static void sim_init(void)
{
    /* --- Mock HAL --- */
    mock_uart_clear();
    mock_timer_clear();
    mock_nvm_clear();

    (void)hal_uart_init(SIM_LIN_BAUDRATE);
    hal_timer_init();

    /* --- Transport --- */
    lin_transport_reset();

    /* --- Session --- */
    uds_session_init(&g_sim_ctx.session);

    /* --- Security --- */
    uds_security_init();

    /* --- DTC --- */
    uds_dtc_init();

    /* --- DID Registry --- */
    uds_did_init();

    /* --- Data Transmission Services --- */
    uds_svc_data_init();

    /* --- IO Control --- */
    uds_svc_io_init();

    /* --- Routine Control --- */
    uds_svc_routine_init();

    /* --- Upload/Download --- */
    uds_svc_transfer_init();

    /* --- Authentication --- */
    uds_svc_auth_init();
    (void)uds_svc_event_store_init();

    /* --- Master Service Dispatch --- */
    uds_service_init();

    /* --- Initial state --- */
    g_sim_ctx.unlocked = false;
}

/* ======================================================================== *
 * process_lin_frame: Decode, Dispatch, and Respond                         *
 * ======================================================================== *
 * Takes a received LIN frame, extracts the UDS request, dispatches it,
 * serialises the response, encodes it as one or more LIN frames, and
 * transmits them via the mock UART.
 *
 * @return 0 on success, -1 on error (frame dropped, logged)
 */

static int process_lin_frame(const lin_frame_t *rx_frame)
{
    lin_diag_pdu_t  rx_pdu;
    uds_request_t   uds_req;
    uds_response_t  uds_rsp;
    uint8_t         serialised[SIM_RSP_BUF_SIZE];
    uint16_t        serialised_len;
    uds_status_t    st;

    /* ---- 1. Decode LIN frame -> PDU ---- */
    lin_status_t lin_st = lin_rx_decode(rx_frame, &rx_pdu);
    if (lin_st != LIN_OK)
    {
        fprintf(stderr, "[SIM] lin_rx_decode failed: %d\n", (int)lin_st);
        return -1;
    }

    /* ---- 2. Parse UDS request ---- */
    st = uds_parse_request(rx_pdu.uds_data, rx_pdu.data_len, &uds_req);
    if (st != UDS_OK)
    {
        fprintf(stderr, "[SIM] uds_parse_request failed: %d\n", (int)st);
        return -1;
    }

    /* ---- 3. Select context and dispatch ---- */
    void *context = get_dispatch_context(uds_req.sid);

    memset(&uds_rsp, 0, sizeof(uds_rsp));
    bool send_rsp = uds_service_dispatch(
        &uds_req, &uds_rsp,
        g_sim_ctx.session.current_session,
        UDS_PHYSICAL,
        context);

    /* ---- 4. Post-dispatch synchronisation ---- */
    sync_after_dispatch(uds_req.sid, send_rsp, &uds_rsp);

    if (!send_rsp)
    {
        /* Response suppressed (SPRMIB or functional addressing) — nothing to send */
        return 0;
    }

    /* ---- 5. Serialise response ---- */
    if (uds_rsp.sid == 0x7F)
    {
        /* Negative response */
        st = uds_serialize_negative_response(
            uds_rsp.subfunc_echo,
            (uds_nrc_t)(uds_rsp.data != NULL ? uds_rsp.data[0] : 0x10),
            serialised, &serialised_len);
    }
    else
    {
        /* Positive response */
        st = uds_serialize_response(&uds_rsp, serialised, &serialised_len);
    }

    if (st != UDS_OK)
    {
        fprintf(stderr, "[SIM] response serialization failed: %d\n", (int)st);
        return -1;
    }

    /* ---- 6. Encode response PDU as LIN frame(s) ---- */
    lin_diag_pdu_t tx_pdu;
    tx_pdu.nad      = rx_pdu.nad;         /* echo back the request's NAD */
    tx_pdu.pci      = 0;                  /* filled by lin_tx_encode */
    tx_pdu.uds_data = serialised;
    tx_pdu.data_len = (uint8_t)serialised_len;

    lin_frame_t tx_frames[8];
    uint8_t     tx_count = 0;

    lin_st = lin_tx_encode(&tx_pdu, tx_frames, &tx_count, 8);
    if (lin_st != LIN_OK)
    {
        fprintf(stderr, "[SIM] lin_tx_encode failed: %d\n", (int)lin_st);
        return -1;
    }

    /* ---- 7. Transmit each LIN frame over mock UART ---- */
    for (uint8_t i = 0; i < tx_count; i++)
    {
        hal_status_t hst = hal_uart_send(tx_frames[i].data, LIN_FRAME_SIZE);
        if (hst != HAL_OK)
        {
            fprintf(stderr, "[SIM] hal_uart_send frame %d failed: %d\n",
                    (int)i, (int)hst);
            return -1;
        }
    }

    return 0;
}

/* ======================================================================== *
 * Main Loop                                                                *
 * ======================================================================== */

int main(void)
{
    /* ---- Initialisation ---- */
    printf("UDS-LIN ECU Simulator v1.0\n");
    printf("  NAD:         0x%02X\n", (unsigned)SIM_LIN_NAD);
    printf("  Baudrate:    %u\n",     (unsigned)SIM_LIN_BAUDRATE);

    sim_init();

    printf("  Initialised: OK\n");
    printf("  Entering main loop...\n");
    printf("========================================\n");

    /* ---- Main loop ---- */
    uint32_t iteration = 0;

    while (1)
    {
        /* Check iteration limit */
        if (SIM_MAX_ITERATIONS > 0u && iteration >= SIM_MAX_ITERATIONS)
        {
            printf("[SIM] Reached max iterations (%u), exiting.\n",
                   (unsigned)SIM_MAX_ITERATIONS);
            break;
        }
        iteration++;

        /* Try to receive one LIN frame (8 bytes) from the mock UART.
         * In simulation mode, hal_uart_receive with timeout=0 returns
         * HAL_TIMEOUT immediately if fewer than 8 bytes are available. */
        lin_frame_t rx_frame;
        hal_status_t rx_st = hal_uart_receive(rx_frame.data,
                                               LIN_FRAME_SIZE, 0);

        if (rx_st == HAL_OK)
        {
            /* Got a complete LIN frame — process it */
            if (process_lin_frame(&rx_frame) != 0)
            {
                fprintf(stderr, "[SIM] Frame processing error at iteration %u\n",
                        (unsigned)iteration);
            }
        }
        else if (rx_st == HAL_TIMEOUT)
        {
            /* No data available — simulate idle CPU (yield / sleep) */
            /* In a real ECU we'd enter low-power mode or service other
             * peripherals.  Here we just spin-wait. */
        }
        else
        {
            fprintf(stderr, "[SIM] hal_uart_receive error: %d\n", (int)rx_st);
        }
    }

    printf("========================================\n");
    printf("[SIM] Simulator shut down.\n");

    return 0;
}

/*
 * uds_dtc.c
 * DTC State Machine Engine — Implementation
 *
 * Static DTC database with full 8-bit status tracking per ISO 14229-1:2020
 * Annex D.  Supports individual status bit manipulation, group-based
 * clearing, snapshot & extended data storage, and status-mask queries.
 *
 * Wave 3b Task 15 — DTC State Machine Engine
 */

#include "uds/uds_dtc.h"
#include <string.h>   /* memset, memcpy */

/* ======================================================================== *
 * Static DTC Database                                                      *
 * ======================================================================== */

/** @brief Static array of DTC records, zero-initialised == empty */
static uds_dtc_record_t dtc_db[UDS_DTC_MAX];

/* ======================================================================== *
 * Internal Helpers                                                         *
 * ======================================================================== */

/**
 * @brief Find the index of a DTC in the database.
 *
 * @param[in] dtc  DTC code to search for
 * @return Index into dtc_db[], or -1 if not found
 */
static int find_dtc_index(uint32_t dtc)
{
    for (int i = 0; i < UDS_DTC_MAX; i++)
    {
        if (dtc_db[i].dtc == dtc)
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Find a DTC in the database, or create a new entry if it does
 *        not yet exist.
 *
 * @param[in] dtc  DTC code to find or create
 * @return Index into dtc_db[], or -1 if the database is full
 */
static int find_or_create_dtc(uint32_t dtc)
{
    int idx = find_dtc_index(dtc);
    if (idx >= 0)
    {
        return idx;
    }

    /* Find first empty slot (dtc == 0 marks empty) */
    for (int i = 0; i < UDS_DTC_MAX; i++)
    {
        if (dtc_db[i].dtc == 0)
        {
            dtc_db[i].dtc          = dtc;
            dtc_db[i].status       = 0;
            dtc_db[i].snapshot_len = 0;
            dtc_db[i].ext_len      = 0;
            return i;
        }
    }

    return -1; /* Database full */
}

/* ======================================================================== *
 * uds_dtc_init                                                             *
 * ======================================================================== */
void uds_dtc_init(void)
{
    memset(dtc_db, 0, sizeof(dtc_db));
}

/* ======================================================================== *
 * uds_dtc_set_status                                                       *
 * ======================================================================== */
void uds_dtc_set_status(uint32_t dtc, uint8_t bit_mask, bool set)
{
    /* Reject zero DTC (empty-slot sentinel) */
    if (dtc == 0)
    {
        return;
    }

    int idx;
    if (set)
    {
        idx = find_or_create_dtc(dtc);
        if (idx < 0)
        {
            return; /* Database full */
        }
    }
    else
    {
        idx = find_dtc_index(dtc);
        if (idx < 0)
        {
            return; /* Unknown DTC, nothing to clear */
        }
    }

    if (set)
    {
        dtc_db[idx].status |= bit_mask;
    }
    else
    {
        dtc_db[idx].status &= ~bit_mask;
    }
}

/* ======================================================================== *
 * uds_dtc_get_status                                                       *
 * ======================================================================== */
uint8_t uds_dtc_get_status(uint32_t dtc)
{
    int idx = find_dtc_index(dtc);
    if (idx < 0)
    {
        return 0;
    }
    return dtc_db[idx].status;
}

/* ======================================================================== *
 * uds_dtc_get_by_status_mask                                               *
 * ======================================================================== */
uint16_t uds_dtc_get_by_status_mask(uint8_t status_mask,
                                    uint32_t *out_dtcs,
                                    uint16_t max_count)
{
    uint16_t count = 0;

    if (out_dtcs == NULL || max_count == 0)
    {
        return 0;
    }

    for (int i = 0; i < UDS_DTC_MAX && count < max_count; i++)
    {
        if (dtc_db[i].dtc != 0 && (dtc_db[i].status & status_mask) != 0)
        {
            out_dtcs[count++] = dtc_db[i].dtc;
        }
    }

    return count;
}

/* ======================================================================== *
 * uds_dtc_clear                                                            *
 * ======================================================================== *
 * DTC groups are identified by the upper nibble (bits 23:20) of the
 * 24-bit DTC value:
 *   0x0 = Powertrain, 0x2 = Chassis, 0x4 = Body, 0x6 = Network
 *   0xFFFFFF = clear all
 * ======================================================================== */
void uds_dtc_clear(uint32_t group_mask)
{
    for (int i = 0; i < UDS_DTC_MAX; i++)
    {
        if (dtc_db[i].dtc == 0)
        {
            continue;
        }

        if (group_mask == 0xFFFFFFU)
        {
            /* Clear all */
            memset(&dtc_db[i], 0, sizeof(uds_dtc_record_t));
        }
        else
        {
            /* Compare upper nibble (bits 23:20) with group_mask */
            uint8_t group = (uint8_t)((dtc_db[i].dtc >> 20) & 0x0F);
            if (group == (uint8_t)(group_mask & 0x0F))
            {
                memset(&dtc_db[i], 0, sizeof(uds_dtc_record_t));
            }
        }
    }
}

/* ======================================================================== *
 * uds_dtc_get_snapshot                                                     *
 * ======================================================================== */
bool uds_dtc_get_snapshot(uint32_t dtc, uint8_t *buf, uint16_t *len)
{
    if (buf == NULL || len == NULL)
    {
        return false;
    }

    int idx = find_dtc_index(dtc);
    if (idx < 0)
    {
        *len = 0;
        return false;
    }

    uint16_t copy_len = dtc_db[idx].snapshot_len;
    if (copy_len > 0)
    {
        (void)memcpy(buf, dtc_db[idx].snapshot, copy_len);
    }
    *len = copy_len;
    return (copy_len > 0);
}

/* ======================================================================== *
 * uds_dtc_get_extended                                                     *
 * ======================================================================== */
bool uds_dtc_get_extended(uint32_t dtc, uint8_t *buf, uint16_t *len)
{
    if (buf == NULL || len == NULL)
    {
        return false;
    }

    int idx = find_dtc_index(dtc);
    if (idx < 0)
    {
        *len = 0;
        return false;
    }

    uint16_t copy_len = dtc_db[idx].ext_len;
    if (copy_len > 0)
    {
        (void)memcpy(buf, dtc_db[idx].extended, copy_len);
    }
    *len = copy_len;
    return (copy_len > 0);
}

/* ======================================================================== *
 * uds_dtc_get_count                                                        *
 * ======================================================================== */
uint16_t uds_dtc_get_count(uint8_t status_mask)
{
    uint16_t count = 0;

    for (int i = 0; i < UDS_DTC_MAX; i++)
    {
        if (dtc_db[i].dtc != 0 && (dtc_db[i].status & status_mask) != 0)
        {
            count++;
        }
    }

    return count;
}

/* ======================================================================== *
 * uds_dtc_set_snapshot                                                     *
 * ======================================================================== */
bool uds_dtc_set_snapshot(uint32_t dtc, const uint8_t *data, uint16_t len)
{
    if (dtc == 0 || data == NULL || len == 0 || len > 8)
    {
        return false;
    }

    int idx = find_or_create_dtc(dtc);
    if (idx < 0)
    {
        return false; /* Database full */
    }

    (void)memcpy(dtc_db[idx].snapshot, data, len);
    dtc_db[idx].snapshot_len = len;
    return true;
}

/* ======================================================================== *
 * uds_dtc_set_extended                                                     *
 * ======================================================================== */
bool uds_dtc_set_extended(uint32_t dtc, const uint8_t *data, uint16_t len)
{
    if (dtc == 0 || data == NULL || len == 0 || len > 8)
    {
        return false;
    }

    int idx = find_or_create_dtc(dtc);
    if (idx < 0)
    {
        return false; /* Database full */
    }

    (void)memcpy(dtc_db[idx].extended, data, len);
    dtc_db[idx].ext_len = len;
    return true;
}

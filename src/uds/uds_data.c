/*
 * uds_data.c
 * DID Registry & Data Storage — Implementation
 *
 * Static DID registry with access control (read-only, write-only,
 * read-write, secured read, secured write), pluggable read/write
 * callbacks, and pre-registered standard DIDs (VIN, ECU serial
 * number, software number, software version).
 *
 * Wave 3b Task 17 — DID Registry & Data Storage
 */

#include "uds/uds_data.h"
#include <string.h>   /* memset, memcpy */

/* ======================================================================== *
 * Static DID Registry                                                      *
 * ======================================================================== */

/** @brief Static array of DID entries */
static uds_did_entry_t did_registry[DID_MAX_ENTRIES];

/** @brief Number of currently registered DID entries */
static uint16_t did_count = 0;

/* ======================================================================== *
 * Default DID Data Buffers (static const, never modified at runtime)       *
 * ======================================================================== */

/** @brief 0xF190 — VIN (17 bytes): placeholder */
static const uint8_t default_vin[]    = "WDB123456789ABCDEF";

/** @brief 0xF186 — ECU serial number (10 bytes): placeholder */
static const uint8_t default_ecu_sn[] = "SN12345678";

/** @brief 0xF187 — System supplier ECU software number (8 bytes) */
static const uint8_t default_sw_num[] = {0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x01};

/** @brief 0xF18C — System supplier ECU software version number (8 bytes) */
static const uint8_t default_sw_ver[] = {0x01, 0x02, 0x03, 0x00,
                                         0x00, 0x00, 0x00, 0x00};

/* ======================================================================== *
 * Internal Helpers                                                         *
 * ======================================================================== */

/**
 * @brief Find the index of a DID in the registry.
 *
 * @param[in] did  Data Identifier to search for
 * @return Index into did_registry[], or -1 if not found
 */
static int find_did_index(uint16_t did)
{
    for (uint16_t i = 0; i < did_count; i++)
    {
        if (did_registry[i].did == did)
        {
            return (int)i;
        }
    }
    return -1;
}

/* ======================================================================== *
 * uds_did_init                                                             *
 * ======================================================================== */
void uds_did_init(void)
{
    did_count = 0;
    memset(did_registry, 0, sizeof(did_registry));

    /* ---- Register default DIDs ---- */

    /* 0xF190: VIN (17 bytes, READ_ONLY) */
    did_registry[did_count].did       = 0xF190;
    did_registry[did_count].len       = 17;
    did_registry[did_count].access    = DID_READ_ONLY;
    did_registry[did_count].data      = (uint8_t*)default_vin;
    did_registry[did_count].on_read   = NULL;
    did_registry[did_count].on_write  = NULL;
    did_count++;

    /* 0xF186: ECU serial number (10 bytes, READ_ONLY) */
    did_registry[did_count].did       = 0xF186;
    did_registry[did_count].len       = 10;
    did_registry[did_count].access    = DID_READ_ONLY;
    did_registry[did_count].data      = (uint8_t*)default_ecu_sn;
    did_registry[did_count].on_read   = NULL;
    did_registry[did_count].on_write  = NULL;
    did_count++;

    /* 0xF187: System supplier ECU software number (8 bytes, READ_ONLY) */
    did_registry[did_count].did       = 0xF187;
    did_registry[did_count].len       = 8;
    did_registry[did_count].access    = DID_READ_ONLY;
    did_registry[did_count].data      = (uint8_t*)default_sw_num;
    did_registry[did_count].on_read   = NULL;
    did_registry[did_count].on_write  = NULL;
    did_count++;

    /* 0xF18C: System supplier ECU software version number (8 bytes, READ_ONLY) */
    did_registry[did_count].did       = 0xF18C;
    did_registry[did_count].len       = 8;
    did_registry[did_count].access    = DID_READ_ONLY;
    did_registry[did_count].data      = (uint8_t*)default_sw_ver;
    did_registry[did_count].on_read   = NULL;
    did_registry[did_count].on_write  = NULL;
    did_count++;
}

/* ======================================================================== *
 * uds_did_register                                                         *
 * ======================================================================== */
bool uds_did_register(const uds_did_entry_t *entry)
{
    if (entry == NULL)
    {
        return false;
    }

    /* Reject duplicate DID */
    if (find_did_index(entry->did) >= 0)
    {
        return false;
    }

    /* Reject if table full */
    if (did_count >= DID_MAX_ENTRIES)
    {
        return false;
    }

    did_registry[did_count] = *entry;
    did_count++;
    return true;
}

/* ======================================================================== *
 * uds_did_read                                                             *
 * ======================================================================== */
bool uds_did_read(uint16_t did, uint8_t *buf, uint16_t *len, bool unlocked)
{
    if (buf == NULL || len == NULL)
    {
        return false;
    }

    int idx = find_did_index(did);
    if (idx < 0)
    {
        *len = 0;
        return false;
    }

    const uds_did_entry_t *entry = &did_registry[idx];

    /* ---- Access control check ---- */
    switch (entry->access)
    {
        case DID_READ_ONLY:
        case DID_READ_WRITE:
            /* Always readable */
            break;

        case DID_SECURED_READ:
            if (!unlocked)
            {
                *len = 0;
                return false;
            }
            break;

        case DID_WRITE_ONLY:
        case DID_SECURED_WRITE:
            /* These access types are not readable */
            *len = 0;
            return false;

        default:
            *len = 0;
            return false;
    }

    /* Check caller's buffer capacity */
    if (*len < entry->len)
    {
        *len = 0;
        return false;
    }

    /* Read data: use callback if provided, otherwise copy from buffer */
    if (entry->on_read != NULL)
    {
        return entry->on_read(entry->did, buf, len);
    }

    (void)memcpy(buf, entry->data, entry->len);
    *len = entry->len;
    return true;
}

/* ======================================================================== *
 * uds_did_write                                                            *
 * ======================================================================== */
bool uds_did_write(uint16_t did, const uint8_t *data, uint16_t len, bool unlocked)
{
    if (data == NULL || len == 0)
    {
        return false;
    }

    int idx = find_did_index(did);
    if (idx < 0)
    {
        return false;
    }

    uds_did_entry_t *entry = &did_registry[idx];

    /* ---- Access control check ---- */
    switch (entry->access)
    {
        case DID_WRITE_ONLY:
        case DID_READ_WRITE:
            /* Always writable */
            break;

        case DID_SECURED_WRITE:
            if (!unlocked)
            {
                return false;
            }
            break;

        case DID_READ_ONLY:
        case DID_SECURED_READ:
            /* These access types are not writable */
            return false;

        default:
            return false;
    }

    /* Validate data length does not exceed buffer capacity */
    if (len > entry->len)
    {
        return false;
    }

    /* Write data: use callback if provided, otherwise copy to buffer */
    if (entry->on_write != NULL)
    {
        return entry->on_write(entry->did, data, len);
    }

    (void)memcpy(entry->data, data, len);
    return true;
}

/* ======================================================================== *
 * uds_did_find                                                             *
 * ======================================================================== */
const uds_did_entry_t* uds_did_find(uint16_t did)
{
    int idx = find_did_index(did);
    if (idx < 0)
    {
        return NULL;
    }
    return &did_registry[idx];
}

/* ======================================================================== *
 * uds_did_get_count                                                        *
 * ======================================================================== */
uint16_t uds_did_get_count(void)
{
    return did_count;
}

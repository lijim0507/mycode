/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds_service_func.h"
#include "uds_service.h"

#include <string.h>
#include <stdlib.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#define UDS_DATA_TABLE_MAX  2

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static uint16_t uds_did_from_data(const uint8_t *data);
static const uds_data_identifier_t *uds_find_did(uint16_t did);
static uint8_t generate_random_byte(void);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static uint8_t data_buf_1[3]  = {0x10, 0x11, 0x12};
static uint8_t data_buf_2[10] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};

static const uds_data_identifier_t data_table[UDS_DATA_TABLE_MAX] = {
    {0xF3A0, data_buf_1, 3,  UDS_SESSION_DEFAULT,   UDS_SESSION_EXTENDED, UDS_SEC_LEVEL_LOCKED, UDS_SEC_LEVEL_1},
    {0xF3A1, data_buf_2, 10, UDS_SESSION_DEFAULT,   UDS_SESSION_EXTENDED, UDS_SEC_LEVEL_LOCKED, UDS_SEC_LEVEL_1},
};

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/
static uint16_t uds_did_from_data(const uint8_t *data)
{
    return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

static const uds_data_identifier_t *uds_find_did(uint16_t did)
{
    uint16_t i;
    for (i = 0; i < UDS_DATA_TABLE_MAX; i++)
    {
        if (data_table[i].did == did)
        {
            return &data_table[i];
        }
    }
    return NULL;
}

static uint8_t generate_random_byte(void)
{
    return (uint8_t)(rand() % 256);
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

uds_handler_result_t uds_diag_session_control(uds_request_t *req, uds_response_t *resp)
{
    uds_session_t requested_session;

    if (req->sub_sid < 0x01 || req->sub_sid > 0x03)
    {
        uds_service_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    requested_session = (uds_session_t)req->sub_sid;
    uds_service_set_session(requested_session);

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data[0] = req->sub_sid;
    resp->data_len = 1;

    return UDS_HANDLER_DONE;
}

uds_handler_result_t uds_security_access(uds_request_t *req, uds_response_t *resp)
{
    static uint8_t seed_value = 0;

    if (req->sub_sid % 2 == 1 && req->sub_sid <= 0x05)
    {
        seed_value = generate_random_byte();

        resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
        resp->data[0] = seed_value;
        resp->data_len = 1;

        return UDS_HANDLER_DONE;
    }
    else if (req->sub_sid == 0x02)
    {
        if (req->data_len >= 1 && req->data[0] == (uint8_t)(seed_value + 5))
        {
            uds_service_set_security_level(UDS_SEC_LEVEL_1);
            resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
            resp->data_len = 0;
            return UDS_HANDLER_DONE;
        }
        else
        {
            uds_service_send_negative_response(req->sid, NRC_INVALID_KEY);
            return UDS_HANDLER_NRC_SENT;
        }
    }
    else if (req->sub_sid == 0x04)
    {
        if (req->data_len >= 1 && req->data[0] == (uint8_t)(seed_value + 10))
        {
            uds_service_set_security_level(UDS_SEC_LEVEL_2);
            resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
            resp->data_len = 0;
            return UDS_HANDLER_DONE;
        }
        else
        {
            uds_service_send_negative_response(req->sid, NRC_INVALID_KEY);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    uds_service_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
    return UDS_HANDLER_NRC_SENT;
}

uds_handler_result_t uds_tester_present(uds_request_t *req, uds_response_t *resp)
{
    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_len = 0;

    return UDS_HANDLER_DONE;
}

uds_handler_result_t uds_read_data_by_id(uds_request_t *req, uds_response_t *resp)
{
    uint16_t did;
    const uds_data_identifier_t *did_entry;
    uds_session_t        cur_session;
    uds_security_level_t cur_level;

    if (req->data_len < 2)
    {
        uds_service_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    did = uds_did_from_data(req->data);
    did_entry = uds_find_did(did);

    if (did_entry == NULL)
    {
        uds_service_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    cur_session = uds_service_get_session();
    cur_level   = uds_service_get_security_level();

    if (did_entry->session_read != UDS_SESSION_DEFAULT)
    {
        if (cur_session != did_entry->session_read)
        {
            uds_service_send_negative_response(req->sid, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    if (did_entry->level_read != UDS_SEC_LEVEL_LOCKED)
    {
        if (cur_level != did_entry->level_read)
        {
            uds_service_send_negative_response(req->sid, NRC_SECURITY_ACCESS_DENIED);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data[0] = (uint8_t)(did >> 8);
    resp->data[1] = (uint8_t)(did & 0xFF);
    (void)memcpy(&resp->data[2], did_entry->data_ptr, did_entry->data_len);
    resp->data_len = 2 + did_entry->data_len;

    return UDS_HANDLER_DONE;
}

uds_handler_result_t uds_write_data_by_id(uds_request_t *req, uds_response_t *resp)
{
    uint16_t did;
    const uds_data_identifier_t *did_entry;
    uds_session_t        cur_session;
    uds_security_level_t cur_level;

    if (req->data_len < 2)
    {
        uds_service_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    did = uds_did_from_data(req->data);
    did_entry = uds_find_did(did);

    cur_session = uds_service_get_session();
    cur_level   = uds_service_get_security_level();

    if (did_entry == NULL)
    {
        uds_service_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (did_entry->session_write != cur_session)
    {
        uds_service_send_negative_response(req->sid, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
        return UDS_HANDLER_NRC_SENT;
    }

    if (did_entry->level_write != cur_level)
    {
        uds_service_send_negative_response(req->sid, NRC_SECURITY_ACCESS_DENIED);
        return UDS_HANDLER_NRC_SENT;
    }

    {
        uint16_t write_len = req->data_len - 2;
        if (write_len > did_entry->data_len)
        {
            write_len = did_entry->data_len;
        }
        (void)memcpy(did_entry->data_ptr, &req->data[2], write_len);
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_len = 0;

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
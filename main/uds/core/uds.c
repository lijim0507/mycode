/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds_service.h"
#include "uds_service_func.h"
#include "isotp.h"
#include "uds_port.h"

#include <string.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void uds_isotp_on_recv(uint8_t *data, uint16_t len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static uint8_t        g_isotp_send_buf[UDS_BUF_SIZE];
static uint8_t        g_isotp_recv_buf[UDS_BUF_SIZE];
static isotp_handle_t g_isotp_handle;

static volatile uint8_t  g_request_flag;
static uint16_t           g_request_len;
static uint8_t            g_request_buf[UDS_BUF_SIZE];

static uds_request_t   g_request;
static uds_response_t  g_response;

static uint8_t         g_initialized;

static uds_session_t        g_current_session        = UDS_SESSION_DEFAULT;
static uds_security_level_t g_current_security_level = UDS_SEC_LEVEL_LOCKED;

static const uds_command_t Commands_Table[UDS_SERVICE_TABLE_SIZE] = {
                                                    /* [0x00] */ {UDS_SERVICE_ID_RESERVED_00,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x01] */ {UDS_SERVICE_ID_RESERVED_01,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x02] */ {UDS_SERVICE_ID_RESERVED_02,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x03] */ {UDS_SERVICE_ID_RESERVED_03,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x04] */ {UDS_SERVICE_ID_RESERVED_04,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x05] */ {UDS_SERVICE_ID_RESERVED_05,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x06] */ {UDS_SERVICE_ID_RESERVED_06,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x07] */ {UDS_SERVICE_ID_RESERVED_07,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x08] */ {UDS_SERVICE_ID_RESERVED_08,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x09] */ {UDS_SERVICE_ID_RESERVED_09,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x0A] */ {UDS_SERVICE_ID_RESERVED_0A,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x0B] */ {UDS_SERVICE_ID_RESERVED_0B,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x0C] */ {UDS_SERVICE_ID_RESERVED_0C,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x0D] */ {UDS_SERVICE_ID_RESERVED_0D,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x0E] */ {UDS_SERVICE_ID_RESERVED_0E,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x0F] */ {UDS_SERVICE_ID_RESERVED_0F,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x10] */ {UDS_SERVICE_ID_DIAGNOSTIC_SESSION_CONTROL, UDS_SERVICE_TYPE_SID_SUB,     uds_diag_session_control},
                                                    /* [0x11] */ {UDS_SERVICE_ID_RESERVED_11,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x12] */ {UDS_SERVICE_ID_RESERVED_12,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x13] */ {UDS_SERVICE_ID_RESERVED_13,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x14] */ {UDS_SERVICE_ID_RESERVED_14,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x15] */ {UDS_SERVICE_ID_RESERVED_15,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x16] */ {UDS_SERVICE_ID_RESERVED_16,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x17] */ {UDS_SERVICE_ID_RESERVED_17,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x18] */ {UDS_SERVICE_ID_RESERVED_18,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x19] */ {UDS_SERVICE_ID_RESERVED_19,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x1A] */ {UDS_SERVICE_ID_RESERVED_1A,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x1B] */ {UDS_SERVICE_ID_RESERVED_1B,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x1C] */ {UDS_SERVICE_ID_RESERVED_1C,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x1D] */ {UDS_SERVICE_ID_RESERVED_1D,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x1E] */ {UDS_SERVICE_ID_RESERVED_1E,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x1F] */ {UDS_SERVICE_ID_RESERVED_1F,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x20] */ {UDS_SERVICE_ID_RESERVED_20,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x21] */ {UDS_SERVICE_ID_RESERVED_21,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x22] */ {UDS_SERVICE_ID_READ_DATA_BY_IDENTIFIER,    UDS_SERVICE_TYPE_SID_DATA,    uds_read_data_by_id},
                                                    /* [0x23] */ {UDS_SERVICE_ID_RESERVED_23,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x24] */ {UDS_SERVICE_ID_RESERVED_24,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x25] */ {UDS_SERVICE_ID_RESERVED_25,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x26] */ {UDS_SERVICE_ID_RESERVED_26,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x27] */ {UDS_SERVICE_ID_SECURITY_ACCESS,             UDS_SERVICE_TYPE_SID_SUB_DATA, uds_security_access},
                                                    /* [0x28] */ {UDS_SERVICE_ID_RESERVED_28,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x29] */ {UDS_SERVICE_ID_RESERVED_29,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x2A] */ {UDS_SERVICE_ID_RESERVED_2A,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x2B] */ {UDS_SERVICE_ID_RESERVED_2B,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x2C] */ {UDS_SERVICE_ID_RESERVED_2C,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x2D] */ {UDS_SERVICE_ID_RESERVED_2D,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x2E] */ {UDS_SERVICE_ID_WRITE_DATA_BY_IDENTIFIER,    UDS_SERVICE_TYPE_SID_DATA,    uds_write_data_by_id},
                                                    /* [0x2F] */ {UDS_SERVICE_ID_RESERVED_2F,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x30] */ {UDS_SERVICE_ID_RESERVED_30,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x31] */ {UDS_SERVICE_ID_RESERVED_31,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x32] */ {UDS_SERVICE_ID_RESERVED_32,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x33] */ {UDS_SERVICE_ID_RESERVED_33,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x34] */ {UDS_SERVICE_ID_RESERVED_34,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x35] */ {UDS_SERVICE_ID_RESERVED_35,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x36] */ {UDS_SERVICE_ID_RESERVED_36,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x37] */ {UDS_SERVICE_ID_RESERVED_37,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x38] */ {UDS_SERVICE_ID_RESERVED_38,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x39] */ {UDS_SERVICE_ID_RESERVED_39,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x3A] */ {UDS_SERVICE_ID_RESERVED_3A,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x3B] */ {UDS_SERVICE_ID_RESERVED_3B,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x3C] */ {UDS_SERVICE_ID_RESERVED_3C,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x3D] */ {UDS_SERVICE_ID_RESERVED_3D,                UDS_SERVICE_TYPE_NONE,        NULL},
                                                    /* [0x3E] */ {UDS_SERVICE_ID_TESTER_PRESENT,               UDS_SERVICE_TYPE_SID_ONLY,    uds_tester_present},
                                                    /* [0x3F] */ {UDS_SERVICE_ID_RESERVED_3F,                UDS_SERVICE_TYPE_NONE,        NULL},
};

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/
static void uds_isotp_on_recv(uint8_t *data, uint16_t len)
{
    if (len > UDS_BUF_SIZE)
    {
        return;
    }

    (void)memcpy(g_request_buf, data, len);
    g_request_len = len;
    g_request_flag = 1;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int uds_service_init(void)
{
    const uds_can_driver_t *driver;

    if (g_initialized)
    {
        uds_service_deinit();
    }

    driver = uds_port_get_driver();
    if (!driver || !driver->send || !driver->get_ms)
    {
        return -1;
    }

    if (driver->init && driver->init() != 0)
    {
        return -2;
    }

    if (isotp_init() != ISOTP_RET_OK)
    {
        if (driver->deinit)
        {
            driver->deinit();
        }
        return -3;
    }

    isotp_init_handle(&g_isotp_handle, 0x7E8, 0x7E0,
                      g_isotp_send_buf, sizeof(g_isotp_send_buf),
                      g_isotp_recv_buf, sizeof(g_isotp_recv_buf),
                      NULL);

    isotp_register_recv_cb(&g_isotp_handle, uds_isotp_on_recv);

    g_current_session        = UDS_SESSION_DEFAULT;
    g_current_security_level = UDS_SEC_LEVEL_LOCKED;
    g_request_flag            = 0;
    g_request_len             = 0;

    g_initialized = 1;
    return 0;
}

void uds_service_deinit(void)
{
    if (!g_initialized)
    {
        return;
    }

    isotp_deinit();

    g_initialized = 0;
}

void uds_service_on_can_frame(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!g_initialized)
    {
        return;
    }
    isotp_feed(&g_isotp_handle, id, (uint8_t *)data, len);
}

void uds_service_poll(void)
{
    if (!g_initialized)
    {
        return;
    }
    isotp_poll(&g_isotp_handle);
}

void uds_service_process(void)
{
    uint8_t sid;
    uds_service_type_t    expected_type;
    uds_service_handler_t handler;
    uds_handler_result_t  result;

    if (!g_request_flag)
    {
        return;
    }

    g_request_flag = 0;

    if (g_request_len == 0)
    {
        return;
    }

    sid = g_request_buf[0];

    if (sid >= UDS_SERVICE_TABLE_SIZE)
    {
        uds_service_send_negative_response(sid, NRC_SERVICE_NOT_SUPPORTED);
        return;
    }

    expected_type = Commands_Table[sid].service_type;
    handler       = Commands_Table[sid].handler;

    if (expected_type == UDS_SERVICE_TYPE_NONE || handler == NULL)
    {
        uds_service_send_negative_response(sid, NRC_SERVICE_NOT_SUPPORTED);
        return;
    }

    (void)memset(&g_request,  0, sizeof(g_request));
    (void)memset(&g_response, 0, sizeof(g_response));

    if (expected_type == UDS_SERVICE_TYPE_SID_ONLY)
    {
        g_request.sid      = g_request_buf[0];
        g_request.sub_sid  = 0;
        g_request.data_len = 0;
    }
    else if (expected_type == UDS_SERVICE_TYPE_SID_SUB)
    {
        if (g_request_len < 2)
        {
            uds_service_send_negative_response(sid, NRC_INCORRECT_MESSAGE_LENGTH);
            return;
        }
        g_request.sid      = g_request_buf[0];
        g_request.sub_sid  = g_request_buf[1];
        g_request.data_len = 0;
    }
    else if (expected_type == UDS_SERVICE_TYPE_SID_DATA)
    {
        if (g_request_len < 2)
        {
            uds_service_send_negative_response(sid, NRC_INCORRECT_MESSAGE_LENGTH);
            return;
        }
        g_request.sid      = g_request_buf[0];
        g_request.sub_sid  = 0;
        g_request.data_len = g_request_len - 1;
        if (g_request.data_len > UDS_BUF_SIZE)
        {
            g_request.data_len = UDS_BUF_SIZE;
        }
        (void)memcpy(g_request.data, &g_request_buf[1], g_request.data_len);
    }
    else if (expected_type == UDS_SERVICE_TYPE_SID_SUB_DATA)
    {
        if (g_request_len < 3)
        {
            uds_service_send_negative_response(sid, NRC_INCORRECT_MESSAGE_LENGTH);
            return;
        }
        g_request.sid      = g_request_buf[0];
        g_request.sub_sid  = g_request_buf[1];
        g_request.data_len = g_request_len - 2;
        if (g_request.data_len > UDS_BUF_SIZE)
        {
            g_request.data_len = UDS_BUF_SIZE;
        }
        (void)memcpy(g_request.data, &g_request_buf[2], g_request.data_len);
    }
    else
    {
        uds_service_send_negative_response(sid, NRC_SERVICE_NOT_SUPPORTED);
        return;
    }

    result = handler(&g_request, &g_response);

    if (result == UDS_HANDLER_NRC_SENT)
    {
        return;
    }
    if (result == UDS_HANDLER_ERROR)
    {
        uds_service_send_negative_response(sid, NRC_GENERAL_REJECT);
        return;
    }

    {
        static uint8_t send_buf[UDS_BUF_SIZE];
        uint16_t send_len = 1 + g_response.data_len;

        send_buf[0] = g_response.sid;
        if (g_response.data_len > 0)
        {
            (void)memcpy(&send_buf[1], g_response.data, g_response.data_len);
        }

        uds_service_send_response(send_buf, send_len);
    }
}

void uds_service_send_response(const uint8_t *data, uint16_t len)
{
    if (!g_initialized || data == NULL || len == 0)
    {
        return;
    }
    isotp_send(&g_isotp_handle, data, len);
}

void uds_service_send_negative_response(uint8_t sid, uint8_t nrc_code)
{
    uint8_t buf[3];
    buf[0] = UDS_NEGATIVE_RESPONSE_SID;
    buf[1] = sid;
    buf[2] = nrc_code;
    uds_service_send_response(buf, 3);
}

uds_session_t uds_service_get_session(void)
{
    return g_current_session;
}

uds_security_level_t uds_service_get_security_level(void)
{
    return g_current_security_level;
}

void uds_service_set_session(uds_session_t session)
{
    g_current_session = session;
}

void uds_service_set_security_level(uds_security_level_t level)
{
    g_current_security_level = level;
}
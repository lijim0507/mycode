/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds_service.h"
#include "uds_defs.h"
#include "uds.h"


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

const uds_service_t services_table[UDS_SERVICE_TABLE_SIZE] = {
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
    /* [0x27] */ {UDS_SERVICE_ID_SECURITY_ACCESS,            UDS_SERVICE_TYPE_SID_SUB_DATA, uds_security_access},
    /* [0x28] */ {UDS_SERVICE_ID_RESERVED_28,                UDS_SERVICE_TYPE_NONE,        NULL},
    /* [0x29] */ {UDS_SERVICE_ID_RESERVED_29,                UDS_SERVICE_TYPE_NONE,        NULL},
    /* [0x2A] */ {UDS_SERVICE_ID_RESERVED_2A,                UDS_SERVICE_TYPE_NONE,        NULL},
    /* [0x2B] */ {UDS_SERVICE_ID_RESERVED_2B,                UDS_SERVICE_TYPE_NONE,        NULL},
    /* [0x2C] */ {UDS_SERVICE_ID_RESERVED_2C,                UDS_SERVICE_TYPE_NONE,        NULL},
    /* [0x2D] */ {UDS_SERVICE_ID_RESERVED_2D,                UDS_SERVICE_TYPE_NONE,        NULL},
    /* [0x2E] */ {UDS_SERVICE_ID_WRITE_DATA_BY_IDENTIFIER,   UDS_SERVICE_TYPE_SID_DATA,    uds_write_data_by_id},
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
    /* [0x3E] */ {UDS_SERVICE_ID_TESTER_PRESENT,             UDS_SERVICE_TYPE_SID_ONLY,    uds_tester_present},
    /* [0x3F] */ {UDS_SERVICE_ID_RESERVED_3F,                UDS_SERVICE_TYPE_NONE,        NULL},
};

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/
/**
 * @brief 从请求数据中提取 16 位数据标识符 (DID)
 *
 * @param data 指向请求数据中 DID 的首字节（大端序）
 * @return uint16_t 提取得到的 DID 值
 */
static uint16_t uds_did_from_data(const uint8_t *data)
{
    return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

/**
 * @brief 在 DID 表中查找匹配的数据标识符条目
 *
 * @param did 要查找的数据标识符
 * @return const uds_data_identifier_t* 匹配的条目指针，未找到返回 NULL
 */
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

/**
 * @brief 生成一个伪随机字节，用于安全访问种子值
 *
 * @return uint8_t 伪随机字节
 */
static uint8_t generate_random_byte(void)
{
    return (uint8_t)(rand() % 256);
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief UDS 0x10 诊断会话控制服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_diag_session_control(uds_request_t *req, uds_response_t *resp)
{
    uds_session_t requested_session;

    if (req->sub_sid < 0x01 || req->sub_sid > 0x03)
    {
        uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    requested_session = (uds_session_t)req->sub_sid;
    uds_set_session(requested_session);

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data[0] = req->sub_sid;
    resp->data_len = 1;

    return UDS_HANDLER_DONE;
}

/**
 * @brief UDS 0x27 安全访问服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
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
            uds_set_security_level(UDS_SEC_LEVEL_1);
            resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
            resp->data_len = 0;
            return UDS_HANDLER_DONE;
        }
        else
        {
            uds_send_negative_response(req->sid, NRC_INVALID_KEY);
            return UDS_HANDLER_NRC_SENT;
        }
    }
    else if (req->sub_sid == 0x04)
    {
        if (req->data_len >= 1 && req->data[0] == (uint8_t)(seed_value + 10))
        {
            uds_set_security_level(UDS_SEC_LEVEL_2);
            resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
            resp->data_len = 0;
            return UDS_HANDLER_DONE;
        }
        else
        {
            uds_send_negative_response(req->sid, NRC_INVALID_KEY);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
    return UDS_HANDLER_NRC_SENT;
}

/**
 * @brief UDS 0x3E Tester Present 服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_tester_present(uds_request_t *req, uds_response_t *resp)
{
    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_len = 0;

    return UDS_HANDLER_DONE;
}

/**
 * @brief UDS 0x22 读数据标识符服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_read_data_by_id(uds_request_t *req, uds_response_t *resp)
{
    uint16_t did;
    const uds_data_identifier_t *did_entry;
    uds_session_t        cur_session;
    uds_security_level_t cur_level;

    if (req->data_len < 2)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    did = uds_did_from_data(req->data);
    did_entry = uds_find_did(did);

    if (did_entry == NULL)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    cur_session = uds_get_session();
    cur_level   = uds_get_security_level();

    if (did_entry->session_read != UDS_SESSION_DEFAULT)
    {
        if (cur_session != did_entry->session_read)
        {
            uds_send_negative_response(req->sid, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    if (did_entry->level_read != UDS_SEC_LEVEL_LOCKED)
    {
        if (cur_level != did_entry->level_read)
        {
            uds_send_negative_response(req->sid, NRC_SECURITY_ACCESS_DENIED);
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

/**
 * @brief UDS 0x2E 写数据标识符服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_write_data_by_id(uds_request_t *req, uds_response_t *resp)
{
    uint16_t did;
    const uds_data_identifier_t *did_entry;
    uds_session_t        cur_session;
    uds_security_level_t cur_level;

    if (req->data_len < 2)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    did = uds_did_from_data(req->data);
    did_entry = uds_find_did(did);

    cur_session = uds_get_session();
    cur_level   = uds_get_security_level();

    if (did_entry == NULL)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (did_entry->session_write != cur_session)
    {
        uds_send_negative_response(req->sid, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
        return UDS_HANDLER_NRC_SENT;
    }

    if (did_entry->level_write != cur_level)
    {
        uds_send_negative_response(req->sid, NRC_SECURITY_ACCESS_DENIED);
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
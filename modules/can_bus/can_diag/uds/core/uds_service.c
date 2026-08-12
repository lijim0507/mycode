/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds_service.h"
#include "uds_defs.h"
#include "uds.h"
#include "uds_port.h"


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
/*							Global Variables								*/
/****************************************************************************/
static uint8_t data_buf_1[3]  = {0x10, 0x11, 0x12};
static uint8_t data_buf_2[10] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};

static const uds_data_identifier_t data_table[UDS_DATA_TABLE_MAX] = {
    {0xF3A0, data_buf_1, 3,  UDS_SESSION_DEFAULT,   UDS_SESSION_EXTENDED, UDS_SEC_LEVEL_LOCKED, UDS_SEC_LEVEL_1},
    {0xF3A1, data_buf_2, 10, UDS_SESSION_DEFAULT,   UDS_SESSION_EXTENDED, UDS_SEC_LEVEL_LOCKED, UDS_SEC_LEVEL_1},
};

/* ===== 刷写状态机 ===== */
static uds_flash_state_t g_flash_state         = UDS_FLASH_IDLE;
static uint32_t          g_flash_base_addr     = 0;
static uint32_t          g_flash_total_size    = 0;
static uint32_t          g_flash_write_offset  = 0;
static uint8_t           g_block_seq_expected  = 1;

#define UDS_ROUTINE_TABLE_MAX  2

/* 桥接函数：将 routine 回调签名适配到 port 空函数 */
static int uds_routine_erase_memory(const uint8_t *req_data, uint16_t req_len)
{
    uint32_t addr;
    uint32_t size;

    if (req_len < 8)
    {
        return -1;
    }

    addr = ((uint32_t)req_data[0] << 24) | ((uint32_t)req_data[1] << 16)
         | ((uint32_t)req_data[2] << 8)  |  (uint32_t)req_data[3];
    size = ((uint32_t)req_data[4] << 24) | ((uint32_t)req_data[5] << 16)
         | ((uint32_t)req_data[6] << 8)  |  (uint32_t)req_data[7];

    return uds_port_flash_erase(addr, size);
}

static int uds_routine_check_dependencies(const uint8_t *req_data, uint16_t req_len)
{
    (void)req_data;
    (void)req_len;

    /* 校验已写入区域 */
    return uds_port_flash_verify(g_flash_base_addr, g_flash_write_offset, NULL);
}

const uds_routine_t routine_table[UDS_ROUTINE_TABLE_MAX] = {
    {0xFF00, UDS_SESSION_PROGRAMMING, UDS_SEC_LEVEL_2, "EraseMemory",
     uds_routine_erase_memory},
    {0xFF01, UDS_SESSION_PROGRAMMING, UDS_SEC_LEVEL_2, "CheckProgrammingDependencies",
     uds_routine_check_dependencies},
};

const uint8_t routine_table_size = UDS_ROUTINE_TABLE_MAX;

const uds_service_t services_table[UDS_SERVICE_TABLE_SIZE] = {
    [0x10] = {UDS_SERVICE_ID_DIAGNOSTIC_SESSION_CONTROL, UDS_SERVICE_TYPE_SID_SUB,     uds_diag_session_control},
    [0x11] = {UDS_SERVICE_ID_ECU_RESET,                  UDS_SERVICE_TYPE_SID_SUB,     uds_ecu_reset},
    [0x22] = {UDS_SERVICE_ID_READ_DATA_BY_IDENTIFIER,    UDS_SERVICE_TYPE_SID_DATA,    uds_read_data_by_id},
    [0x27] = {UDS_SERVICE_ID_SECURITY_ACCESS,            UDS_SERVICE_TYPE_SID_SUB_DATA, uds_security_access},
    [0x28] = {UDS_SERVICE_ID_COMMUNICATION_CONTROL,      UDS_SERVICE_TYPE_SID_SUB_DATA, uds_communication_control},
    [0x2E] = {UDS_SERVICE_ID_WRITE_DATA_BY_IDENTIFIER,   UDS_SERVICE_TYPE_SID_DATA,    uds_write_data_by_id},
    [0x31] = {UDS_SERVICE_ID_ROUTINE_CONTROL,            UDS_SERVICE_TYPE_SID_SUB_DATA, uds_routine_control},
    [0x34] = {UDS_SERVICE_ID_REQUEST_DOWNLOAD,           UDS_SERVICE_TYPE_SID_DATA,     uds_request_download},
    [0x36] = {UDS_SERVICE_ID_TRANSFER_DATA,              UDS_SERVICE_TYPE_SID_DATA,     uds_transfer_data},
    [0x37] = {UDS_SERVICE_ID_REQUEST_TRANSFER_EXIT,      UDS_SERVICE_TYPE_SID_DATA,     uds_request_transfer_exit},
    [0x3E] = {UDS_SERVICE_ID_TESTER_PRESENT,             UDS_SERVICE_TYPE_SID_ONLY,    uds_tester_present},
    [0x85] = {UDS_SERVICE_ID_CONTROL_DTC_SETTING,        UDS_SERVICE_TYPE_SID_SUB,     uds_control_dtc_setting},
};

/**
 * @brief 复位刷写状态机到 IDLE
 */
void uds_flash_reset_state(void)
{
    g_flash_state        = UDS_FLASH_IDLE;
    g_flash_base_addr    = 0;
    g_flash_total_size   = 0;
    g_flash_write_offset = 0;
    g_block_seq_expected = 1;
}

/****************************************************************************/
/*					   	UDS 0x10 诊断会话控制服务处理函数					    */
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
    resp->data_ptr[0] = req->sub_sid;
    resp->data_len = 1;

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*				   		UDS 0x22 读数据标识符服务处理函数					    */
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

    did = uds_did_from_data(req->data_ptr);
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
    resp->data_ptr[0] = (uint8_t)(did >> 8);
    resp->data_ptr[1] = (uint8_t)(did & 0xFF);
    (void)memcpy(&resp->data_ptr[2], did_entry->data_ptr, did_entry->data_len);
    resp->data_len = 2 + did_entry->data_len;

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*				   		UDS 0x27 安全访问服务处理函数						    */
/****************************************************************************/

/**
 * @brief 生成一个伪随机字节，用于安全访问种子值
 *
 * @return uint8_t 伪随机字节
 */
static uint8_t generate_random_byte(void)
{
    return (uint8_t)(rand() % 256);
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
        resp->data_ptr[0] = seed_value;
        resp->data_len = 1;

        return UDS_HANDLER_DONE;
    }
    else if (req->sub_sid == 0x02)
    {
        if (req->data_len >= 1 && req->data_ptr[0] == (uint8_t)(seed_value + 5))
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
        if (req->data_len >= 1 && req->data_ptr[0] == (uint8_t)(seed_value + 10))
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

/****************************************************************************/
/*				   		UDS 0x2E 写数据标识符服务处理函数					    */
/****************************************************************************/

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

    did = uds_did_from_data(req->data_ptr);
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
        (void)memcpy(did_entry->data_ptr, &req->data_ptr[2], write_len);
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_len = 0;

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*				   		UDS 0x3E Tester Present 服务处理函数				    */
/****************************************************************************/

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

/****************************************************************************/
/*                      UDS 0x11 ECU Reset                                 */
/****************************************************************************/

uds_handler_result_t uds_ecu_reset(uds_request_t *req, uds_response_t *resp)
{
    if (req->sub_sid != 0x01 && req->sub_sid != 0x02 && req->sub_sid != 0x03)
    {
        uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = req->sub_sid;
    resp->data_len = 1;

    if (req->sub_sid == 0x01)
    {
        uds_port_ecu_reset();
    }

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*                   UDS 0x28 Communication Control                        */
/****************************************************************************/

uds_handler_result_t uds_communication_control(uds_request_t *req, uds_response_t *resp)
{
    uint8_t comm_type;

    if (req->sub_sid != 0x00 && req->sub_sid != 0x01 && req->sub_sid != 0x03)
    {
        uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 1)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    comm_type = req->data_ptr[0];

    if (comm_type < 0x01 || comm_type > 0x03)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = comm_type;
    resp->data_len = 1;

    uds_port_comm_control(req->sub_sid, comm_type);

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*                    UDS 0x31 Routine Control                             */
/****************************************************************************/

uds_handler_result_t uds_routine_control(uds_request_t *req, uds_response_t *resp)
{
    uint16_t routine_id;
    uint8_t  i;
    const uds_routine_t *routine = NULL;

    if (req->sub_sid != 0x01)
    {
        if (req->sub_sid == 0x02 || req->sub_sid == 0x03)
        {
            uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        }
        else
        {
            uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        }
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 2)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    routine_id = ((uint16_t)req->data_ptr[0] << 8) | (uint16_t)req->data_ptr[1];

    for (i = 0; i < routine_table_size; i++)
    {
        if (routine_table[i].routine_id == routine_id)
        {
            routine = &routine_table[i];
            break;
        }
    }

    if (routine == NULL)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (routine->session != UDS_SESSION_DEFAULT)
    {
        if (uds_get_session() != routine->session)
        {
            uds_send_negative_response(req->sid, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    if (routine->security != UDS_SEC_LEVEL_LOCKED)
    {
        if (uds_get_security_level() != routine->security)
        {
            uds_send_negative_response(req->sid, NRC_SECURITY_ACCESS_DENIED);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    {
        int ret;
        ret = routine->start(req->data_ptr + 2, req->data_len - 2);

        if (ret != 0)
        {
            uds_send_negative_response(req->sid, NRC_GENERAL_PROGRAMMING_FAILURE);
            return UDS_HANDLER_NRC_SENT;
        }
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = req->sub_sid;
    resp->data_ptr[1] = (uint8_t)(routine_id >> 8);
    resp->data_ptr[2] = (uint8_t)(routine_id & 0xFF);
    resp->data_len = 3;

    /* EraseMemory 万能复位 */
    if (routine_id == 0xFF00)
    {
        g_flash_state        = UDS_FLASH_IDLE;
        g_flash_base_addr    = 0;
        g_flash_total_size   = 0;
        g_flash_write_offset = 0;
        g_block_seq_expected = 1;
    }
    /* CheckProgrammingDependencies */
    else if (routine_id == 0xFF01)
    {
        if (g_flash_state != UDS_FLASH_TRANSFER_COMPLETE)
        {
            uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
            return UDS_HANDLER_NRC_SENT;
        }
        g_flash_state = UDS_FLASH_VERIFIED;
    }

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*                   UDS 0x34 Request Download                             */
/****************************************************************************/

uds_handler_result_t uds_request_download(uds_request_t *req, uds_response_t *resp)
{
    uint8_t  dfi;
    uint8_t  alfi;
    uint8_t  addr_len;
    uint8_t  size_len;
    uint32_t memory_address;
    uint32_t memory_size;
    uint32_t max_block_size;
    uint32_t sector_size;
    uint8_t  idx;

    if (g_flash_state != UDS_FLASH_IDLE)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 2)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    dfi  = req->data_ptr[0];
    alfi = req->data_ptr[1];

    if ((dfi & 0x0F) != 0x00)
    {
        /* 不支持压缩/加密 */
        uds_send_negative_response(req->sid, NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED);
        return UDS_HANDLER_NRC_SENT;
    }

    addr_len = (alfi >> 4) & 0x0F;
    size_len = alfi & 0x0F;

    if (addr_len == 0 || size_len == 0 || addr_len > 4 || size_len > 4)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < (uint16_t)(2 + addr_len + size_len))
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    /* 解析 memoryAddress（大端） */
    memory_address = 0;
    for (idx = 0; idx < addr_len; idx++)
    {
        memory_address = (memory_address << 8) | req->data_ptr[2 + idx];
    }

    /* 解析 memorySize（大端） */
    memory_size = 0;
    for (idx = 0; idx < size_len; idx++)
    {
        memory_size = (memory_size << 8) | req->data_ptr[2 + addr_len + idx];
    }

    if (memory_size == 0)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    g_flash_base_addr    = memory_address;
    g_flash_total_size   = memory_size;
    g_flash_write_offset = 0;
    g_block_seq_expected = 1;
    g_flash_state        = UDS_FLASH_DOWNLOAD_REQUESTED;

    /* 查询最大块大小（对齐粒度） */
    sector_size   = uds_port_flash_get_sector_size(memory_address);
    max_block_size = (sector_size > 0) ? sector_size : 4096;

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = 0x00;  /* lengthFormatIdentifier: 对齐=未指定, maxBlockLength 长度为 4 */
    resp->data_ptr[1] = (uint8_t)(max_block_size >> 24);
    resp->data_ptr[2] = (uint8_t)(max_block_size >> 16);
    resp->data_ptr[3] = (uint8_t)(max_block_size >> 8);
    resp->data_ptr[4] = (uint8_t)(max_block_size);
    resp->data_len = 5;

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*                     UDS 0x36 Transfer Data                              */
/****************************************************************************/

uds_handler_result_t uds_transfer_data(uds_request_t *req, uds_response_t *resp)
{
    uint8_t  block_seq;
    uint16_t payload_len;
    int      ret;

    if (g_flash_state != UDS_FLASH_DOWNLOAD_REQUESTED
        && g_flash_state != UDS_FLASH_TRANSFERRING)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    if (req->data_len < 1)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    block_seq   = req->data_ptr[0];
    payload_len = req->data_len - 1;

    if (block_seq == 0)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return UDS_HANDLER_NRC_SENT;
    }

    if (block_seq != g_block_seq_expected)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    if (g_flash_write_offset + payload_len > g_flash_total_size)
    {
        uds_send_negative_response(req->sid, NRC_INCORRECT_MESSAGE_LENGTH);
        return UDS_HANDLER_NRC_SENT;
    }

    ret = uds_port_flash_write(g_flash_base_addr + g_flash_write_offset,
                               req->data_ptr + 1, payload_len);

    if (ret != 0)
    {
        uds_send_negative_response(req->sid, NRC_GENERAL_PROGRAMMING_FAILURE);
        return UDS_HANDLER_NRC_SENT;
    }

    g_flash_write_offset += payload_len;

    g_block_seq_expected = (uint8_t)(block_seq + 1);
    if (g_block_seq_expected == 0)
    {
        g_block_seq_expected = 1;
    }

    g_flash_state = UDS_FLASH_TRANSFERRING;

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = block_seq;
    resp->data_len = 1;

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*                  UDS 0x37 Request Transfer Exit                         */
/****************************************************************************/

uds_handler_result_t uds_request_transfer_exit(uds_request_t *req, uds_response_t *resp)
{
    uint32_t crc;

    if (g_flash_state != UDS_FLASH_TRANSFERRING)
    {
        uds_send_negative_response(req->sid, NRC_REQUEST_SEQUENCE_ERROR);
        return UDS_HANDLER_NRC_SENT;
    }

    (void)uds_port_flash_verify(g_flash_base_addr, g_flash_write_offset, &crc);

    g_flash_state = UDS_FLASH_TRANSFER_COMPLETE;

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_len = 0;

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*                  UDS 0x85 Control DTC Setting                           */
/****************************************************************************/

uds_handler_result_t uds_control_dtc_setting(uds_request_t *req, uds_response_t *resp)
{
    if (req->sub_sid != 0x01 && req->sub_sid != 0x02)
    {
        uds_send_negative_response(req->sid, NRC_SUB_SERVICE_NOT_SUPPORTED);
        return UDS_HANDLER_NRC_SENT;
    }

    resp->sid = req->sid + UDS_RESPONSE_SID_OFFSET;
    resp->data_ptr[0] = req->sub_sid;
    resp->data_len = 1;

    uds_port_dtc_control(req->sub_sid);

    return UDS_HANDLER_DONE;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
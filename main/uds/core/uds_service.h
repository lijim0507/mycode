#ifndef __UDS_SERVICE_H_
#define __UDS_SERVICE_H_

/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds.h"
#include "uds_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct
{
    uint16_t did;
    uint8_t  *data_ptr;
    uint16_t data_len;
    uds_session_t        session_read;
    uds_session_t        session_write;
    uds_security_level_t level_read;
    uds_security_level_t level_write;
} uds_data_identifier_t;

/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/
const uds_service_t services_table[UDS_SERVICE_ID_RESERVED_MAX];


/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/

/**
 * @brief UDS 0x10 诊断会话控制服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_diag_session_control(uds_request_t *req, uds_response_t *resp);

/**
 * @brief UDS 0x27 安全访问服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_security_access(uds_request_t *req, uds_response_t *resp);

/**
 * @brief UDS 0x3E Tester Present 服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_tester_present(uds_request_t *req, uds_response_t *resp);

/**
 * @brief UDS 0x22 读数据标识符服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_read_data_by_id(uds_request_t *req, uds_response_t *resp);

/**
 * @brief UDS 0x2E 写数据标识符服务处理函数
 *
 * @param req  请求结构体指针
 * @param resp 响应结构体指针
 * @return uds_handler_result_t 处理结果
 */
uds_handler_result_t uds_write_data_by_id(uds_request_t *req, uds_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
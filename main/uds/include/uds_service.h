#ifndef __UDS_SERVICE_FUNC_H_
#define __UDS_SERVICE_FUNC_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "uds_service.h"

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
/*						Exported Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/

uds_handler_result_t uds_diag_session_control(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_security_access(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_tester_present(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_read_data_by_id(uds_request_t *req, uds_response_t *resp);
uds_handler_result_t uds_write_data_by_id(uds_request_t *req, uds_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
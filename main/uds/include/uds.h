#ifndef __UDS_SERVICE_H_
#define __UDS_SERVICE_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#define UDS_SERVICE_TABLE_SIZE      0x40
#define UDS_RESPONSE_SID_OFFSET     0x40
#define UDS_NEGATIVE_RESPONSE_SID   0x7F

#define UDS_BUF_SIZE                4096

/****************************************************************************/
/*						Negative Response Codes								*/
/****************************************************************************/
#define NRC_GENERAL_REJECT                              0x10
#define NRC_SERVICE_NOT_SUPPORTED                      0x11
#define NRC_SUB_SERVICE_NOT_SUPPORTED                  0x12
#define NRC_INCORRECT_MESSAGE_LENGTH                   0x13
#define NRC_RESPONSE_TOO_LONG                           0x14
#define NRC_BUSY_REPEAT_REQUEST                        0x21
#define NRC_CONDITIONS_NOT_CORRECT                     0x22
#define NRC_REQUEST_SEQUENCE_ERROR                     0x24
#define NRC_FAILURE_EXECUTION                          0x26
#define NRC_REQUEST_OUT_OF_RANGE                       0x31
#define NRC_SECURITY_ACCESS_DENIED                    0x35
#define NRC_INVALID_KEY                                0x35
#define NRC_EXCEEDED_ATTEMPTS                          0x36
#define NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED            0x37
#define NRC_REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING 0x78
#define NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION    0x7F

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef enum
{
    UDS_SESSION_DEFAULT     = 0x01,
    UDS_SESSION_PROGRAMMING = 0x02,
    UDS_SESSION_EXTENDED    = 0x03,
} uds_session_t;

typedef enum
{
    UDS_SEC_LEVEL_LOCKED = 0x00,
    UDS_SEC_LEVEL_1      = 0x01,
    UDS_SEC_LEVEL_2      = 0x02,
} uds_security_level_t;

typedef enum
{
    UDS_SERVICE_TYPE_NONE       = 0,
    UDS_SERVICE_TYPE_SID_ONLY,
    UDS_SERVICE_TYPE_SID_SUB,
    UDS_SERVICE_TYPE_SID_DATA,
    UDS_SERVICE_TYPE_SID_SUB_DATA,
} uds_service_type_t;

typedef enum
{
    UDS_HANDLER_DONE     = 0,
    UDS_HANDLER_NRC_SENT = 1,
    UDS_HANDLER_ERROR    = 2,
} uds_handler_result_t;

typedef enum
{
    UDS_SERVICE_ID_RESERVED_00 = 0x00,
    UDS_SERVICE_ID_RESERVED_01 = 0x01,
    UDS_SERVICE_ID_RESERVED_02 = 0x02,
    UDS_SERVICE_ID_RESERVED_03 = 0x03,
    UDS_SERVICE_ID_RESERVED_04 = 0x04,
    UDS_SERVICE_ID_RESERVED_05 = 0x05,
    UDS_SERVICE_ID_RESERVED_06 = 0x06,
    UDS_SERVICE_ID_RESERVED_07 = 0x07,
    UDS_SERVICE_ID_RESERVED_08 = 0x08,
    UDS_SERVICE_ID_RESERVED_09 = 0x09,
    UDS_SERVICE_ID_RESERVED_0A = 0x0A,
    UDS_SERVICE_ID_RESERVED_0B = 0x0B,
    UDS_SERVICE_ID_RESERVED_0C = 0x0C,
    UDS_SERVICE_ID_RESERVED_0D = 0x0D,
    UDS_SERVICE_ID_RESERVED_0E = 0x0E,
    UDS_SERVICE_ID_RESERVED_0F = 0x0F,
    UDS_SERVICE_ID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    UDS_SERVICE_ID_RESERVED_11 = 0x11,
    UDS_SERVICE_ID_RESERVED_12 = 0x12,
    UDS_SERVICE_ID_RESERVED_13 = 0x13,
    UDS_SERVICE_ID_RESERVED_14 = 0x14,
    UDS_SERVICE_ID_RESERVED_15 = 0x15,
    UDS_SERVICE_ID_RESERVED_16 = 0x16,
    UDS_SERVICE_ID_RESERVED_17 = 0x17,
    UDS_SERVICE_ID_RESERVED_18 = 0x18,
    UDS_SERVICE_ID_RESERVED_19 = 0x19,
    UDS_SERVICE_ID_RESERVED_1A = 0x1A,
    UDS_SERVICE_ID_RESERVED_1B = 0x1B,
    UDS_SERVICE_ID_RESERVED_1C = 0x1C,
    UDS_SERVICE_ID_RESERVED_1D = 0x1D,
    UDS_SERVICE_ID_RESERVED_1E = 0x1E,
    UDS_SERVICE_ID_RESERVED_1F = 0x1F,
    UDS_SERVICE_ID_RESERVED_20 = 0x20,
    UDS_SERVICE_ID_RESERVED_21 = 0x21,
    UDS_SERVICE_ID_READ_DATA_BY_IDENTIFIER = 0x22,
    UDS_SERVICE_ID_RESERVED_23 = 0x23,
    UDS_SERVICE_ID_RESERVED_24 = 0x24,
    UDS_SERVICE_ID_RESERVED_25 = 0x25,
    UDS_SERVICE_ID_RESERVED_26 = 0x26,
    UDS_SERVICE_ID_SECURITY_ACCESS = 0x27,
    UDS_SERVICE_ID_RESERVED_28 = 0x28,
    UDS_SERVICE_ID_RESERVED_29 = 0x29,
    UDS_SERVICE_ID_RESERVED_2A = 0x2A,
    UDS_SERVICE_ID_RESERVED_2B = 0x2B,
    UDS_SERVICE_ID_RESERVED_2C = 0x2C,
    UDS_SERVICE_ID_RESERVED_2D = 0x2D,
    UDS_SERVICE_ID_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    UDS_SERVICE_ID_RESERVED_2F = 0x2F,
    UDS_SERVICE_ID_RESERVED_30 = 0x30,
    UDS_SERVICE_ID_RESERVED_31 = 0x31,
    UDS_SERVICE_ID_RESERVED_32 = 0x32,
    UDS_SERVICE_ID_RESERVED_33 = 0x33,
    UDS_SERVICE_ID_RESERVED_34 = 0x34,
    UDS_SERVICE_ID_RESERVED_35 = 0x35,
    UDS_SERVICE_ID_RESERVED_36 = 0x36,
    UDS_SERVICE_ID_RESERVED_37 = 0x37,
    UDS_SERVICE_ID_RESERVED_38 = 0x38,
    UDS_SERVICE_ID_RESERVED_39 = 0x39,
    UDS_SERVICE_ID_RESERVED_3A = 0x3A,
    UDS_SERVICE_ID_RESERVED_3B = 0x3B,
    UDS_SERVICE_ID_RESERVED_3C = 0x3C,
    UDS_SERVICE_ID_RESERVED_3D = 0x3D,
    UDS_SERVICE_ID_TESTER_PRESENT = 0x3E,
    UDS_SERVICE_ID_RESERVED_3F = 0x3F,
} uds_service_id_t;

typedef struct
{
    uint8_t  sid;
    uint8_t  sub_sid;
    uint16_t data_len;
    uint8_t  data[UDS_BUF_SIZE];
} uds_request_t;

typedef struct
{
    uint8_t  sid;
    uint16_t data_len;
    uint8_t  data[UDS_BUF_SIZE];
} uds_response_t;

typedef uds_handler_result_t (*uds_service_handler_t)(uds_request_t *req, uds_response_t *resp);

typedef struct
{
    uds_service_id_t      service_id;
    uds_service_type_t    service_type;
    uds_service_handler_t handler;
} uds_command_t;

/****************************************************************************/
/*						Exported Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/

int  uds_service_init(void);
void uds_service_deinit(void);
void uds_service_on_can_frame(uint32_t id, const uint8_t *data, uint8_t len);
void uds_service_poll(void);
void uds_service_process(void);

void uds_service_send_response(const uint8_t *data, uint16_t len);
void uds_service_send_negative_response(uint8_t sid, uint8_t nrc_code);

uds_session_t        uds_service_get_session(void);
uds_security_level_t uds_service_get_security_level(void);
void                 uds_service_set_session(uds_session_t session);
void                 uds_service_set_security_level(uds_security_level_t level);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
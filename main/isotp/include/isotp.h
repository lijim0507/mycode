#ifndef __ISOTP_H_
#define __ISOTP_H_

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "isotp_config.h"
#include "isotp_defines.h"

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct isotp_port_driver
{
    int      (*init)(void);
    int      (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int      (*deinit)(void);
    uint32_t (*get_ms)(void);
    void     (*debug)(const char *message, ...);
} isotp_port_driver_t;

typedef struct
{
    /* sender parameters */
    uint32_t send_arbitration_id;
    /* message buffer */
    uint8_t* send_buffer;
    uint16_t send_buf_size;
    uint16_t send_size;
    uint16_t send_offset;
    /* multi-frame flags */
    uint8_t send_sn;
    uint16_t send_bs_remain;
    uint8_t send_st_min;
    uint8_t send_wtf_count;
    uint32_t send_timer_st;
    uint32_t send_timer_bs;
    int send_protocol_result;
    uint8_t send_status;

    /* receiver parameters */
    uint32_t receive_arbitration_id;
    /* message buffer */
    uint8_t* receive_buffer;
    uint16_t receive_buf_size;
    uint16_t receive_size;
    uint16_t receive_offset;
    /* multi-frame control */
    uint8_t receive_sn;
    uint8_t receive_bs_count;
    uint32_t receive_timer_cr;
    int receive_protocol_result;
    uint8_t receive_status;
} isotp_handle_t;

/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/

int  isotp_init(const isotp_port_driver_t *driver);
int  isotp_deinit(void);

void isotp_init_link(isotp_handle_t *handle, uint32_t sendid, uint32_t recvid,
                     uint8_t *sendbuf, uint16_t sendbufsize,
                     uint8_t *recvbuf, uint16_t recvbufsize);

void isotp_poll(isotp_handle_t *handle);
void isotp_on_can_message(isotp_handle_t *handle, uint8_t *data, uint8_t len);

int  isotp_send(isotp_handle_t *handle, const uint8_t payload[], uint16_t size);
int  isotp_send_with_id(isotp_handle_t *handle, uint32_t id, const uint8_t payload[], uint16_t size);

int  isotp_receive(isotp_handle_t *handle, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size);

#ifdef __cplusplus
}
#endif

#endif
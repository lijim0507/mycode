
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "isotp.h"

#include <stdint.h>

#include "assert.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/
/* st_min to microsecond */
static uint8_t isotp_ms_to_st_min(uint8_t ms)
{
    uint8_t st_min;

    st_min = ms;
    if (st_min > 0x7F)
    {
        st_min = 0x7F;
    }

    return st_min;
}

/* st_min to msec  */
static uint8_t isotp_st_min_to_ms(uint8_t st_min)
{
    uint8_t ms;

    if (st_min >= 0xF1 && st_min <= 0xF9)
    {
        ms = 1;
    }
    else if (st_min <= 0x7F)
    {
        ms = st_min;
    }
    else
    {
        ms = 0;
    }

    return ms;
}

static int isotp_send_flow_control(isotp_handle_t* handle, uint8_t flow_status, uint8_t block_size, uint8_t st_min_ms)
{
    isotp_can_message_t message;
    int ret;

    /* setup message  */
    message.as.flow_control.type = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.fs = flow_status;
    message.as.flow_control.bs = block_size;
    message.as.flow_control.st_min = isotp_ms_to_st_min(st_min_ms);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.flow_control.reserve, 0, sizeof(message.as.flow_control.reserve));
    ret = isotp_user_send_can(handle->send_arbitration_id, message.as.data_array.ptr, sizeof(message));
#else
    ret = isotp_user_send_can(handle->send_arbitration_id, message.as.data_array.ptr, 3);
#endif

    return ret;
}

static int isotp_send_single_frame(isotp_handle_t* handle, uint32_t id)
{
    isotp_can_message_t message;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(handle->send_size <= 7);

    /* setup message  */
    message.as.single_frame.type = ISOTP_PCI_TYPE_SINGLE;
    message.as.single_frame.sf_dl = (uint8_t)handle->send_size;
    (void)memcpy(message.as.single_frame.data, handle->send_buffer, handle->send_size);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.single_frame.data + handle->send_size, 0, sizeof(message.as.single_frame.data) - handle->send_size);
    ret = isotp_user_send_can(id, message.as.data_array.ptr, sizeof(message));
#else
    ret = isotp_user_send_can(id, message.as.data_array.ptr, handle->send_size + 1);
#endif

    return ret;
}

static int isotp_send_first_frame(isotp_handle_t* handle, uint32_t id)
{
    isotp_can_message_t message;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(handle->send_size > 7);

    /* setup message  */
    message.as.first_frame.type = ISOTP_PCI_TYPE_FIRST_FRAME;
    message.as.first_frame.ff_dl_low = (uint8_t)handle->send_size;
    message.as.first_frame.ff_dl_high = (uint8_t)(0x0F & (handle->send_size >> 8));
    (void)memcpy(message.as.first_frame.data, handle->send_buffer, sizeof(message.as.first_frame.data));

    /* send message */
    ret = isotp_user_send_can(id, message.as.data_array.ptr, sizeof(message));
    if (ISOTP_RET_OK == ret)
    {
        handle->send_offset += sizeof(message.as.first_frame.data);
        handle->send_sn = 1;
    }

    return ret;
}

static int isotp_send_consecutive_frame(isotp_handle_t* handle)
{
    isotp_can_message_t message;
    uint16_t data_length;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(handle->send_size > 7);

    /* setup message  */
    message.as.consecutive_frame.type = ISOTP_PCI_TYPE_CONSECUTIVE_FRAME;
    message.as.consecutive_frame.sn = handle->send_sn;
    data_length = handle->send_size - handle->send_offset;
    if (data_length > sizeof(message.as.consecutive_frame.data))
    {
        data_length = sizeof(message.as.consecutive_frame.data);
    }
    (void)memcpy(message.as.consecutive_frame.data, handle->send_buffer + handle->send_offset, data_length);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.consecutive_frame.data + data_length, 0, sizeof(message.as.consecutive_frame.data) - data_length);
    ret = isotp_user_send_can(handle->send_arbitration_id, message.as.data_array.ptr, sizeof(message));
#else
    ret = isotp_user_send_can(handle->send_arbitration_id, message.as.data_array.ptr, data_length + 1);
#endif
    if (ISOTP_RET_OK == ret)
    {
        handle->send_offset += data_length;
        if (++(handle->send_sn) > 0x0F)
        {
            handle->send_sn = 0;
        }
    }

    return ret;
}

static int isotp_receive_single_frame(isotp_handle_t* handle, isotp_can_message_t* message, uint8_t len)
{
    /* check data length */
    if ((0 == message->as.single_frame.sf_dl) || (message->as.single_frame.sf_dl > (len - 1)))
    {
        isotp_user_debug("Single-frame length too small.");
        return ISOTP_RET_LENGTH;
    }

    /* copying data */
    (void)memcpy(handle->receive_buffer, message->as.single_frame.data, message->as.single_frame.sf_dl);
    handle->receive_size = message->as.single_frame.sf_dl;

    return ISOTP_RET_OK;
}

static int isotp_receive_first_frame(isotp_handle_t* handle, isotp_can_message_t* message, uint8_t len)
{
    uint16_t payload_length;

    if (8 != len)
    {
        isotp_user_debug("First frame should be 8 bytes in length.");
        return ISOTP_RET_LENGTH;
    }

    /* check data length */
    payload_length = message->as.first_frame.ff_dl_high;
    payload_length = (payload_length << 8) + message->as.first_frame.ff_dl_low;

    /* should not use multiple frame transmition */
    if (payload_length <= 7)
    {
        isotp_user_debug("Should not use multiple frame transmission.");
        return ISOTP_RET_LENGTH;
    }

    if (payload_length > handle->receive_buf_size)
    {
        isotp_user_debug("Multi-frame response too large for receiving buffer.");
        return ISOTP_RET_OVERFLOW;
    }

    /* copying data */
    (void)memcpy(handle->receive_buffer, message->as.first_frame.data, sizeof(message->as.first_frame.data));
    handle->receive_size = payload_length;
    handle->receive_offset = sizeof(message->as.first_frame.data);
    handle->receive_sn = 1;

    return ISOTP_RET_OK;
}

static int isotp_receive_consecutive_frame(isotp_handle_t* handle, isotp_can_message_t* message, uint8_t len)
{
    uint16_t remaining_bytes;

    /* check sn */
    if (handle->receive_sn != message->as.consecutive_frame.sn)
    {
        return ISOTP_RET_WRONG_SN;
    }

    /* check data length */
    remaining_bytes = handle->receive_size - handle->receive_offset;
    if (remaining_bytes > sizeof(message->as.consecutive_frame.data))
    {
        remaining_bytes = sizeof(message->as.consecutive_frame.data);
    }
    if (remaining_bytes > len - 1)
    {
        isotp_user_debug("Consecutive frame too short.");
        return ISOTP_RET_LENGTH;
    }

    /* copying data */
    (void)memcpy(handle->receive_buffer + handle->receive_offset, message->as.consecutive_frame.data, remaining_bytes);

    handle->receive_offset += remaining_bytes;
    if (++(handle->receive_sn) > 0x0F)
    {
        handle->receive_sn = 0;
    }

    return ISOTP_RET_OK;
}

static int isotp_receive_flow_control_frame(isotp_handle_t* handle, isotp_can_message_t* message, uint8_t len)
{
    /* check message length */
    if (len < 3)
    {
        isotp_user_debug("Flow control frame too short.");
        return ISOTP_RET_LENGTH;
    }

    return ISOTP_RET_OK;
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int isotp_send(isotp_handle_t* handle, const uint8_t payload[], uint16_t size)
{
    return isotp_send_with_id(handle, handle->send_arbitration_id, payload, size);
}

int isotp_send_with_id(isotp_handle_t* handle, uint32_t id, const uint8_t payload[], uint16_t size)
{
    int ret;

    if (handle == 0x0)
    {
        isotp_user_debug("handle is null!");
        return ISOTP_RET_ERROR;
    }

    if (size > handle->send_buf_size)
    {
        isotp_user_debug("Message size too large. Increase ISO_TP_MAX_MESSAGE_SIZE to set a larger buffer\n");
        char message[128];
        sprintf(&message[0], "Attempted to send %d bytes; max size is %d!\n", size, handle->send_buf_size);
        return ISOTP_RET_OVERFLOW;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == handle->send_status)
    {
        isotp_user_debug("Abort previous message, transmission in progress.\n");
        return ISOTP_RET_INPROGRESS;
    }

    /* copy into local buffer */
    handle->send_size = size;
    handle->send_offset = 0;
    (void)memcpy(handle->send_buffer, payload, size);

    if (handle->send_size < 8)
    {
        /* send single frame */
        ret = isotp_send_single_frame(handle, id);
    }
    else
    {
        /* send multi-frame */
        ret = isotp_send_first_frame(handle, id);

        /* init multi-frame control flags */
        if (ISOTP_RET_OK == ret)
        {
            handle->send_bs_remain = 0;
            handle->send_st_min = 0;
            handle->send_wtf_count = 0;
            handle->send_timer_st = isotp_user_get_ms();
            handle->send_timer_bs = isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            handle->send_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            handle->send_status = ISOTP_SEND_STATUS_INPROGRESS;
        }
    }

    return ret;
}

void isotp_on_can_message(isotp_handle_t* handle, uint8_t* data, uint8_t len)
{
    isotp_can_message_t message;
    int ret;

    if (len < 2 || len > 8)
    {
        return;
    }

    memcpy(message.as.data_array.ptr, data, len);
    memset(message.as.data_array.ptr + len, 0, sizeof(message.as.data_array.ptr) - len);

    switch (message.as.common_frame.type)
    {
        case ISOTP_PCI_TYPE_SINGLE:
        {
            /* update protocol result */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == handle->receive_status)
            {
                handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            }
            else
            {
                handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }

            /* handle message */
            ret = isotp_receive_single_frame(handle, &message, len);

            if (ISOTP_RET_OK == ret)
            {
                /* change status */
                handle->receive_status = ISOTP_RECEIVE_STATUS_FULL;
            }
            break;
        }
        case ISOTP_PCI_TYPE_FIRST_FRAME:
        {
            /* update protocol result */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == handle->receive_status)
            {
                handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            }
            else
            {
                handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }

            /* handle message */
            ret = isotp_receive_first_frame(handle, &message, len);

            /* if overflow happened */
            if (ISOTP_RET_OVERFLOW == ret)
            {
                /* update protocol result */
                handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                /* change status */
                handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                /* send error message */
                isotp_send_flow_control(handle, PCI_FLOW_STATUS_OVERFLOW, 0, 0);
                break;
            }

            /* if receive successful */
            if (ISOTP_RET_OK == ret)
            {
                /* change status */
                handle->receive_status = ISOTP_RECEIVE_STATUS_INPROGRESS;
                /* send fc frame */
                handle->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                isotp_send_flow_control(handle, PCI_FLOW_STATUS_CONTINUE, handle->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                /* refresh timer cs */
                handle->receive_timer_cr = isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            }

            break;
        }
        case ISOTP_PCI_TYPE_CONSECUTIVE_FRAME:
        {
            /* check if in receiving status */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS != handle->receive_status)
            {
                handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
                break;
            }

            /* handle message */
            ret = isotp_receive_consecutive_frame(handle, &message, len);

            /* if wrong sn */
            if (ISOTP_RET_WRONG_SN == ret)
            {
                handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_WRONG_SN;
                handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                break;
            }

            /* if success */
            if (ISOTP_RET_OK == ret)
            {
                /* refresh timer cs */
                handle->receive_timer_cr = isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

                /* receive finished */
                if (handle->receive_offset >= handle->receive_size)
                {
                    handle->receive_status = ISOTP_RECEIVE_STATUS_FULL;
                }
                else
                {
                    /* send fc when bs reaches limit */
                    if (0 == --handle->receive_bs_count)
                    {
                        handle->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                        isotp_send_flow_control(handle, PCI_FLOW_STATUS_CONTINUE, handle->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                    }
                }
            }

            break;
        }
        case ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME:
            /* handle fc frame only when sending in progress  */
            if (ISOTP_SEND_STATUS_INPROGRESS != handle->send_status)
            {
                break;
            }

            /* handle message */
            ret = isotp_receive_flow_control_frame(handle, &message, len);

            if (ISOTP_RET_OK == ret)
            {
                /* refresh bs timer */
                handle->send_timer_bs = isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

                /* overflow */
                if (PCI_FLOW_STATUS_OVERFLOW == message.as.flow_control.fs)
                {
                    handle->send_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                    handle->send_status = ISOTP_SEND_STATUS_ERROR;
                }

                /* wait */
                else if (PCI_FLOW_STATUS_WAIT == message.as.flow_control.fs)
                {
                    handle->send_wtf_count += 1;
                    /* wait exceed allowed count */
                    if (handle->send_wtf_count > ISO_TP_MAX_WFT_NUMBER)
                    {
                        handle->send_protocol_result = ISOTP_PROTOCOL_RESULT_WFT_OVRN;
                        handle->send_status = ISOTP_SEND_STATUS_ERROR;
                    }
                }

                /* permit send */
                else if (PCI_FLOW_STATUS_CONTINUE == message.as.flow_control.fs)
                {
                    if (0 == message.as.flow_control.bs)
                    {
                        handle->send_bs_remain = ISOTP_INVALID_BS;
                    }
                    else
                    {
                        handle->send_bs_remain = message.as.flow_control.bs;
                    }
                    handle->send_st_min = isotp_st_min_to_ms(message.as.flow_control.st_min);
                    handle->send_wtf_count = 0;
                }
            }
            break;
        default:
            break;
    };

    return;
}

int isotp_receive(isotp_handle_t* handle, uint8_t* payload, const uint16_t payload_size, uint16_t* out_size)
{
    uint16_t copylen;

    if (ISOTP_RECEIVE_STATUS_FULL != handle->receive_status)
    {
        return ISOTP_RET_NO_DATA;
    }

    copylen = handle->receive_size;
    if (copylen > payload_size)
    {
        copylen = payload_size;
    }

    memcpy(payload, handle->receive_buffer, copylen);
    *out_size = copylen;

    handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;

    return ISOTP_RET_OK;
}

void isotp_init_handle(isotp_handle_t* handle, uint32_t sendid, uint8_t* sendbuf, uint16_t sendbufsize, uint8_t* recvbuf,
                       uint16_t recvbufsize)
{
    memset(handle, 0, sizeof(*handle));
    handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    handle->send_status = ISOTP_SEND_STATUS_IDLE;
    handle->send_arbitration_id = sendid;
    handle->send_buffer = sendbuf;
    handle->send_buf_size = sendbufsize;
    handle->receive_buffer = recvbuf;
    handle->receive_buf_size = recvbufsize;

    return;
}

void isotp_poll(isotp_handle_t* handle)
{
    int ret;
    // 发送剩余的连续帧数据
    /* only polling when operation in progress */
    if (ISOTP_SEND_STATUS_INPROGRESS == handle->send_status)
    {
        /* continue send data */
        if (/* send data if bs_remain is invalid or bs_remain large than zero */
            (ISOTP_INVALID_BS == handle->send_bs_remain || handle->send_bs_remain > 0) &&
            /* and if st_min is zero or go beyond interval time */
            (0 == handle->send_st_min || (0 != handle->send_st_min && IsoTpTimeAfter(isotp_user_get_ms(), handle->send_timer_st))))
        {
            ret = isotp_send_consecutive_frame(handle);
            if (ISOTP_RET_OK == ret)
            {
                if (ISOTP_INVALID_BS != handle->send_bs_remain)
                {
                    handle->send_bs_remain -= 1;
                }
                handle->send_timer_bs = isotp_user_get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
                handle->send_timer_st = isotp_user_get_ms() + handle->send_st_min;

                /* check if send finish */
                if (handle->send_offset >= handle->send_size)
                {
                    handle->send_status = ISOTP_SEND_STATUS_IDLE;
                }
            }
            else
            {
                handle->send_status = ISOTP_SEND_STATUS_ERROR;
            }
        }

        /* check timeout */
        if (IsoTpTimeAfter(isotp_user_get_ms(), handle->send_timer_bs))
        {
            handle->send_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_BS;
            handle->send_status = ISOTP_SEND_STATUS_ERROR;
        }
    }

    // 接收剩余的连续帧的超时处理
    /* only polling when operation in progress */
    if (ISOTP_RECEIVE_STATUS_INPROGRESS == handle->receive_status)
    {
        /* check timeout */
        if (IsoTpTimeAfter(isotp_user_get_ms(), handle->receive_timer_cr))
        {
            handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_CR;
            handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
        }
    }

    return;
}

void iso_process(isotp_handle_t* handle)
{
    if (1)
    {
    }

    if (1)
    {
    }
}
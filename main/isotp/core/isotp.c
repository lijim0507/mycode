/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "isotp.h"
#include "isotp_port.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static uint8_t isotp_ms_to_st_min(uint8_t ms);
static uint8_t isotp_st_min_to_ms(uint8_t st_min);

static int isotp_send_flow_control(isotp_handle_t *handle, uint8_t flow_status, uint8_t block_size, uint8_t st_min_ms);
static int isotp_send_single_frame(isotp_handle_t *handle, uint32_t id);
static int isotp_send_first_frame(isotp_handle_t *handle, uint32_t id);
static int isotp_send_consecutive_frame(isotp_handle_t *handle);

static int isotp_receive_single_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len);
static int isotp_receive_first_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len);
static int isotp_receive_consecutive_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len);
static int isotp_receive_flow_control_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len);

static void isotp_rx_process(isotp_handle_t *handle, uint8_t *data, uint8_t len);
static void isotp_debug(const char *message);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const isotp_port_driver_t *g_isotp_driver;

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/
static void isotp_debug(const char *message)
{
    if (g_isotp_driver && g_isotp_driver->debug)
    {
        g_isotp_driver->debug(message);
    }
}

/* st_min unit: millisecond, to STmin format */
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

/* STmin format to millisecond */
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
        ms = 127;
    }

    return ms;
}

static int isotp_send_flow_control(isotp_handle_t *handle, uint8_t flow_status, uint8_t block_size, uint8_t st_min_ms)
{
    isotp_can_message_t message;
    int ret;

    if (!g_isotp_driver || !g_isotp_driver->send)
    {
        return ISOTP_RET_ERROR;
    }

    /* setup message */
    message.as.flow_control.type = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.fs = flow_status;
    message.as.flow_control.bs = block_size;
    message.as.flow_control.st_min = isotp_ms_to_st_min(st_min_ms);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.flow_control.reserve, 0, sizeof(message.as.flow_control.reserve));
    ret = g_isotp_driver->send(handle->send_arbitration_id, message.as.data_array.ptr, sizeof(message));
#else
    ret = g_isotp_driver->send(handle->send_arbitration_id, message.as.data_array.ptr, 3);
#endif

    return ret;
}

static int isotp_send_single_frame(isotp_handle_t *handle, uint32_t id)
{
    isotp_can_message_t message;
    int ret;

    if (!g_isotp_driver || !g_isotp_driver->send)
    {
        return ISOTP_RET_ERROR;
    }

    /* single frame message length must not exceed 7 */
    assert(handle->send_size <= 7);

    /* setup message */
    message.as.single_frame.type = ISOTP_PCI_TYPE_SINGLE;
    message.as.single_frame.sf_dl = (uint8_t)handle->send_size;
    (void)memcpy(message.as.single_frame.data, handle->send_buffer, handle->send_size);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.single_frame.data + handle->send_size, 0, sizeof(message.as.single_frame.data) - handle->send_size);
    ret = g_isotp_driver->send(id, message.as.data_array.ptr, sizeof(message));
#else
    ret = g_isotp_driver->send(id, message.as.data_array.ptr, handle->send_size + 1);
#endif

    return ret;
}

static int isotp_send_first_frame(isotp_handle_t *handle, uint32_t id)
{
    isotp_can_message_t message;
    int ret;

    if (!g_isotp_driver || !g_isotp_driver->send)
    {
        return ISOTP_RET_ERROR;
    }

    /* multi frame message length must greater than 7 */
    assert(handle->send_size > 7);

    /* setup message */
    message.as.first_frame.type = ISOTP_PCI_TYPE_FIRST_FRAME;
    message.as.first_frame.ff_dl_low = (uint8_t)handle->send_size;
    message.as.first_frame.ff_dl_high = (uint8_t)(0x0F & (handle->send_size >> 8));
    (void)memcpy(message.as.first_frame.data, handle->send_buffer, sizeof(message.as.first_frame.data));

    /* send message */
    ret = g_isotp_driver->send(id, message.as.data_array.ptr, sizeof(message));
    if (ISOTP_RET_OK == ret)
    {
        handle->send_offset += sizeof(message.as.first_frame.data);
        handle->send_sn = 1;
    }

    return ret;
}

static int isotp_send_consecutive_frame(isotp_handle_t *handle)
{
    isotp_can_message_t message;
    uint16_t data_length;
    int ret;

    if (!g_isotp_driver || !g_isotp_driver->send)
    {
        return ISOTP_RET_ERROR;
    }

    /* multi frame message length must greater than 7 */
    assert(handle->send_size > 7);

    /* setup message */
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
    ret = g_isotp_driver->send(handle->send_arbitration_id, message.as.data_array.ptr, sizeof(message));
#else
    ret = g_isotp_driver->send(handle->send_arbitration_id, message.as.data_array.ptr, data_length + 1);
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

static int isotp_receive_single_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len)
{
    /* check data length */
    if ((0 == message->as.single_frame.sf_dl) || (message->as.single_frame.sf_dl > (len - 1)))
    {
        isotp_debug("Single-frame length too small.");
        return ISOTP_RET_LENGTH;
    }

    /* check buffer overflow */
    if (message->as.single_frame.sf_dl > handle->receive_buf_size)
    {
        isotp_debug("Single-frame length exceeds receive buffer.");
        return ISOTP_RET_OVERFLOW;
    }

    /* copying data */
    (void)memcpy(handle->receive_buffer, message->as.single_frame.data, message->as.single_frame.sf_dl);
    handle->receive_size = message->as.single_frame.sf_dl;

    return ISOTP_RET_OK;
}

static int isotp_receive_first_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len)
{
    uint16_t payload_length;

    if (8 != len)
    {
        isotp_debug("First frame should be 8 bytes in length.");
        return ISOTP_RET_LENGTH;
    }

    /* check data length */
    payload_length = message->as.first_frame.ff_dl_high;
    payload_length = (payload_length << 8) + message->as.first_frame.ff_dl_low;

    /* should not use multiple frame transmission */
    if (payload_length <= 7)
    {
        isotp_debug("Should not use multiple frame transmission.");
        return ISOTP_RET_LENGTH;
    }

    if (payload_length > handle->receive_buf_size)
    {
        isotp_debug("Multi-frame response too large for receiving buffer.");
        return ISOTP_RET_OVERFLOW;
    }

    /* copying data */
    (void)memcpy(handle->receive_buffer, message->as.first_frame.data, sizeof(message->as.first_frame.data));
    handle->receive_size = payload_length;
    handle->receive_offset = sizeof(message->as.first_frame.data);
    handle->receive_sn = 1;

    return ISOTP_RET_OK;
}

static int isotp_receive_consecutive_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len)
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
        isotp_debug("Consecutive frame too short.");
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

static int isotp_receive_flow_control_frame(isotp_handle_t *handle, isotp_can_message_t *message, uint8_t len)
{
    /* check message length */
    if (len < 3)
    {
        isotp_debug("Flow control frame too short.");
        return ISOTP_RET_LENGTH;
    }

    return ISOTP_RET_OK;
}

static void isotp_rx_process(isotp_handle_t *handle, uint8_t *data, uint8_t len)
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
                /* refresh timer cr */
                handle->receive_timer_cr = g_isotp_driver->get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
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
                /* refresh timer cr */
                handle->receive_timer_cr = g_isotp_driver->get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

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
            /* handle fc frame only when sending in progress */
            if (ISOTP_SEND_STATUS_INPROGRESS != handle->send_status)
            {
                break;
            }

            /* handle message */
            ret = isotp_receive_flow_control_frame(handle, &message, len);

            if (ISOTP_RET_OK == ret)
            {
                /* refresh bs timer */
                handle->send_timer_bs = g_isotp_driver->get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

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
}

void isotp_feed(isotp_handle_t *handle, uint32_t id, uint8_t *data, uint8_t len)
{
    if (handle == NULL || data == NULL)
    {
        return;
    }

    if (handle->receive_arbitration_id != id)
    {
        return;
    }

    isotp_rx_process(handle, data, len);

    /* In callback mode: auto-notify when message is fully assembled */
    if (handle->recv_cb && ISOTP_RECEIVE_STATUS_FULL == handle->receive_status)
    {
        handle->recv_cb(handle->receive_buffer, handle->receive_size);
        handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    }
}

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int isotp_init(void)
{
    const isotp_port_driver_t *driver = isotp_port_get_driver();
    return isotp_init_with_driver(driver);
}

int isotp_init_with_driver(const isotp_port_driver_t *driver)
{
    if (!driver || !driver->send || !driver->get_ms)
    {
        return ISOTP_RET_ERROR;
    }

    if (g_isotp_driver)
    {
        isotp_deinit();
    }

    if (driver->init && driver->init() != 0)
    {
        return ISOTP_RET_ERROR;
    }

    g_isotp_driver = driver;

    return ISOTP_RET_OK;
}

int isotp_deinit(void)
{
    if (g_isotp_driver && g_isotp_driver->deinit)
    {
        g_isotp_driver->deinit();
    }

    g_isotp_driver = NULL;

    return ISOTP_RET_OK;
}

void isotp_init_handle(isotp_handle_t *handle, uint32_t recvid, uint32_t sendid,
                     uint8_t *sendbuf, uint16_t sendbufsize,
                     uint8_t *recvbuf, uint16_t recvbufsize)
{
    memset(handle, 0, sizeof(*handle));
    handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    handle->send_status = ISOTP_SEND_STATUS_IDLE;
    handle->send_arbitration_id = sendid;
    handle->receive_arbitration_id = recvid;
    handle->send_buffer = sendbuf;
    handle->send_buf_size = sendbufsize;
    handle->receive_buffer = recvbuf;
    handle->receive_buf_size = recvbufsize;
}



int isotp_send(isotp_handle_t *handle, const uint8_t payload[], uint16_t size)
{
    return isotp_send_with_id(handle, handle->send_arbitration_id, payload, size);
}

int isotp_send_with_id(isotp_handle_t *handle, uint32_t id, const uint8_t payload[], uint16_t size)
{
    int ret;

    if (handle == NULL)
    {
        return ISOTP_RET_ERROR;
    }

    if (payload == NULL || size == 0)
    {
        return ISOTP_RET_ERROR;
    }

    if (size > handle->send_buf_size)
    {
        isotp_debug("Message size too large for send buffer.");
        return ISOTP_RET_OVERFLOW;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == handle->send_status)
    {
        isotp_debug("Abort previous message, transmission in progress.");
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
            handle->send_timer_st = g_isotp_driver->get_ms();
            handle->send_timer_bs = g_isotp_driver->get_ms() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            handle->send_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            handle->send_status = ISOTP_SEND_STATUS_INPROGRESS;
        }
    }

    return ret;
}

int isotp_read(isotp_handle_t *handle, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size)
{
    uint16_t copylen;

    if (handle == NULL || payload == NULL || out_size == NULL)
    {
        return ISOTP_RET_ERROR;
    }

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

    if (handle->receive_size > payload_size)
    {
        return ISOTP_RET_OVERFLOW;
    }

    return ISOTP_RET_OK;
}



void isotp_poll(isotp_handle_t *handle)
{
    int ret;
    uint32_t now;
    uint32_t id;
    uint8_t data[8];
    uint8_t len;

    if (handle == NULL || !g_isotp_driver)
    {
        return;
    }

    now = g_isotp_driver->get_ms();

    /* pull all available CAN frames from port driver */
    if (g_isotp_driver->receive)
    {
        while (g_isotp_driver->receive(&id, data, &len))
        {
            isotp_feed(handle, id, data, len);
        }
    }

    /* send remaining consecutive frames */
    if (ISOTP_SEND_STATUS_INPROGRESS == handle->send_status)
    {
        /* continue send data */
        if (/* send data if bs_remain is invalid or bs_remain large than zero */
            (ISOTP_INVALID_BS == handle->send_bs_remain || handle->send_bs_remain > 0) &&
            /* and if st_min is zero or go beyond interval time */
            (0 == handle->send_st_min || (0 != handle->send_st_min && IsoTpTimeAfter(now, handle->send_timer_st))))
        {
            ret = isotp_send_consecutive_frame(handle);
            if (ISOTP_RET_OK == ret)
            {
                if (ISOTP_INVALID_BS != handle->send_bs_remain)
                {
                    handle->send_bs_remain -= 1;
                }
                handle->send_timer_bs = now + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
                handle->send_timer_st = now + handle->send_st_min;

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
        if (ISOTP_SEND_STATUS_INPROGRESS == handle->send_status &&
            IsoTpTimeAfter(now, handle->send_timer_bs))
        {
            handle->send_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_BS;
            handle->send_status = ISOTP_SEND_STATUS_ERROR;
        }
    }

    /* receive remaining consecutive frames timeout handling */
    if (ISOTP_RECEIVE_STATUS_INPROGRESS == handle->receive_status)
    {
        /* check timeout */
        if (IsoTpTimeAfter(now, handle->receive_timer_cr))
        {
            handle->receive_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_CR;
            handle->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
        }
    }
}

void isotp_register_recv_cb(isotp_handle_t *handle, isotp_recv_cb_t cb)
{
    if (handle == NULL)
    {
        return;
    }
    handle->recv_cb = cb;
}
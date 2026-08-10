/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "odrive_can_port.h"

#include <string.h>

#include "driver/twai.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#ifndef ODRIVE_CAN_TWAI_TX_GPIO
#define ODRIVE_CAN_TWAI_TX_GPIO    GPIO_NUM_21
#endif

#ifndef ODRIVE_CAN_TWAI_RX_GPIO
#define ODRIVE_CAN_TWAI_RX_GPIO    GPIO_NUM_22
#endif

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int esp32_odrive_can_init(void);
static int esp32_odrive_can_deinit(void);
static int esp32_odrive_can_send(uint32_t id, const uint8_t *data, uint8_t len);
static int esp32_odrive_can_receive(uint32_t *id, uint8_t *data, uint8_t *len,
                                     uint32_t timeout_ms);

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

const can_port_driver_t *odrive_can_port_get_driver(void)
{
    static const can_port_driver_t driver = {
        .init    = esp32_odrive_can_init,
        .deinit  = esp32_odrive_can_deinit,
        .send    = esp32_odrive_can_send,
        .receive = esp32_odrive_can_receive,
    };
    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 TWAI 外设
 * @return 0: 成功, -1: 安装失败, -2: 启动失败
 */
static int esp32_odrive_can_init(void)
{
    twai_general_config_t g_cfg = {
        .mode          = TWAI_MODE_NORMAL,
        .tx_io         = ODRIVE_CAN_TWAI_TX_GPIO,
        .rx_io         = ODRIVE_CAN_TWAI_RX_GPIO,
        .clkout_io     = GPIO_NUM_NC,
        .bus_off_io    = GPIO_NUM_NC,
        .tx_queue_len  = 16,
        .rx_queue_len  = 16,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0,
    };

    twai_timing_config_t t_cfg = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK)
    {
        return -1;
    }

    if (twai_start() != ESP_OK)
    {
        twai_driver_uninstall();
        return -2;
    }

    return 0;
}

/**
 * @brief  释放 TWAI 外设
 * @return 0: 成功
 */
static int esp32_odrive_can_deinit(void)
{
    twai_stop();
    twai_driver_uninstall();
    return 0;
}

/**
 * @brief  发送 CAN 帧
 * @param  id    32-bit CAN ID
 * @return 0: 成功, -1: 失败
 */
static int esp32_odrive_can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    twai_message_t tx_msg;

    tx_msg.identifier       = id;
    tx_msg.data_length_code = (len > 8) ? 8 : len;
    tx_msg.extd             = 0;
    tx_msg.rtr              = 0;

    (void)memset(tx_msg.data, 0, sizeof(tx_msg.data));
    if (data && len > 0)
    {
        (void)memcpy(tx_msg.data, data, tx_msg.data_length_code);
    }

    return (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) == ESP_OK) ? 0 : -1;
}

/**
 * @brief  接收 CAN 帧
 * @param  timeout_ms  0: 非阻塞, > 0: 阻塞超时
 * @return 1: 有帧, 0: 无帧
 */
static int esp32_odrive_can_receive(uint32_t *id, uint8_t *data, uint8_t *len,
                                     uint32_t timeout_ms)
{
    twai_message_t rx_msg;

    if (twai_receive(&rx_msg, pdMS_TO_TICKS(timeout_ms)) != ESP_OK)
    {
        return 0;
    }

    if (id)
    {
        *id = rx_msg.identifier;
    }

    if (data && len)
    {
        *len = rx_msg.data_length_code;
        (void)memcpy(data, rx_msg.data, rx_msg.data_length_code);
    }

    return 1;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

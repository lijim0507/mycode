/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "isotp_port.h"

#include <string.h>

#include "stm32f0xx_hal.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#ifndef ISOTP_STM32_CAN_HANDLE
#define ISOTP_STM32_CAN_HANDLE         hcan
#endif

#define ISOTP_STM32_TX_TIMEOUT_MS      100

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int      stm32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len);
static uint32_t stm32_isotp_get_ms(void);
static void     stm32_isotp_debug(const char *message, ...);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

extern CAN_HandleTypeDef ISOTP_STM32_CAN_HANDLE;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

const isotp_port_driver_t *isotp_port_get_driver(void)
{
    static const isotp_port_driver_t driver = {
        .send   = stm32_isotp_send,
        .get_ms = stm32_isotp_get_ms,
        .debug  = stm32_isotp_debug,
    };
    return &driver;
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int stm32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    CAN_HandleTypeDef *hcan = &ISOTP_STM32_CAN_HANDLE;
    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;

    if (len > 8)
    {
        len = 8;
    }

    tx_header.StdId              = id & 0x7FF;
    tx_header.ExtId              = 0;
    tx_header.IDE                = CAN_ID_STD;
    tx_header.RTR                = CAN_RTR_DATA;
    tx_header.DLC                = len;
    tx_header.TransmitGlobalTime = DISABLE;

    uint8_t tx_data[8] = {0};
    if (data && len > 0)
    {
        (void)memcpy(tx_data, data, len);
    }

    if (HAL_CAN_AddTxMessage(hcan, &tx_header, tx_data, &tx_mailbox) != HAL_OK)
    {
        return -2;
    }

    uint32_t start = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
    {
        if ((HAL_GetTick() - start) >= ISOTP_STM32_TX_TIMEOUT_MS)
        {
            return -3;
        }
    }

    return 0;
}

static uint32_t stm32_isotp_get_ms(void)
{
    return HAL_GetTick();
}

static void stm32_isotp_debug(const char *message, ...)
{
    (void)message;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
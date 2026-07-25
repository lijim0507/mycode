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

#ifndef ISOTP_STM32_CAN_TX_GPIO_PORT
#define ISOTP_STM32_CAN_TX_GPIO_PORT   GPIOA
#endif

#ifndef ISOTP_STM32_CAN_TX_GPIO_PIN
#define ISOTP_STM32_CAN_TX_GPIO_PIN    GPIO_PIN_12
#endif

#ifndef ISOTP_STM32_CAN_TX_GPIO_AF
#define ISOTP_STM32_CAN_TX_GPIO_AF     GPIO_AF4_CAN
#endif

#ifndef ISOTP_STM32_CAN_RX_GPIO_PORT
#define ISOTP_STM32_CAN_RX_GPIO_PORT   GPIOA
#endif

#ifndef ISOTP_STM32_CAN_RX_GPIO_PIN
#define ISOTP_STM32_CAN_RX_GPIO_PIN    GPIO_PIN_11
#endif

#ifndef ISOTP_STM32_CAN_RX_GPIO_AF
#define ISOTP_STM32_CAN_RX_GPIO_AF     GPIO_AF4_CAN
#endif

#ifndef ISOTP_STM32_CAN_BAUDRATE
#define ISOTP_STM32_CAN_BAUDRATE       500
#endif

#ifndef ISOTP_STM32_RX_BUF_SIZE
#define ISOTP_STM32_RX_BUF_SIZE        16
#endif

#define ISOTP_STM32_TX_TIMEOUT_MS      100

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct
{
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
} isotp_stm32_rx_frame_t;

typedef struct
{
    volatile uint32_t head;
    volatile uint32_t tail;
    isotp_stm32_rx_frame_t frames[ISOTP_STM32_RX_BUF_SIZE];
} isotp_stm32_rx_buf_t;

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static int      stm32_isotp_init(void);
static int      stm32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len);
static int      stm32_isotp_deinit(void);
static uint32_t stm32_isotp_get_ms(void);
static void     stm32_isotp_debug(const char *message, ...);

int  stm32_isotp_can_rx_isr(uint32_t id, const uint8_t *data, uint8_t len);
int  stm32_isotp_receive(uint32_t *id, uint8_t *data, uint8_t *len);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

extern CAN_HandleTypeDef ISOTP_STM32_CAN_HANDLE;

static volatile uint8_t g_initialized;
static isotp_stm32_rx_buf_t g_rx_buf;

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/
/*
 * Usage:
 *
 * 1) ISR context — read CAN frame and push to ring buffer:
 *
 *    void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
 *    {
 *        CAN_RxHeaderTypeDef rx_header;
 *        uint8_t data[8];
 *        if (HAL_CAN_GetRxMessage(hcan, CAN_FILTER_FIFO0, &rx_header, data) == HAL_OK)
 *        {
 *            stm32_isotp_can_rx_isr(rx_header.StdId, data, rx_header.DLC);
 *        }
 *    }
 *
 * 2) Main loop — drain ring buffer and feed to ISOTP:
 *
 *    uint32_t id;
 *    uint8_t  data[8];
 *    uint8_t  len;
 *    while (stm32_isotp_receive(&id, data, &len))
 *    {
 *        isotp_feed(&handle, id, data, len);
 *    }
 *    isotp_poll(&handle);
 */

const isotp_port_driver_t *isotp_port_get_driver(void)
{
    static const isotp_port_driver_t driver = {
        .init   = stm32_isotp_init,
        .send   = stm32_isotp_send,
        .receive = stm32_isotp_receive,
        .deinit = stm32_isotp_deinit,
        .get_ms = stm32_isotp_get_ms,
        .debug  = stm32_isotp_debug,
    };
    return &driver;
}

int stm32_isotp_can_rx_isr(uint32_t id, const uint8_t *data, uint8_t len)
{
    uint32_t next;

    if (len > 8)
    {
        len = 8;
    }

    next = (g_rx_buf.head + 1) % ISOTP_STM32_RX_BUF_SIZE;

    if (next == g_rx_buf.tail)
    {
        return -1;
    }

    g_rx_buf.frames[g_rx_buf.head].id = id;
    (void)memcpy(g_rx_buf.frames[g_rx_buf.head].data, data, len);
    g_rx_buf.frames[g_rx_buf.head].dlc = len;

    g_rx_buf.head = next;

    return 0;
}


/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int stm32_isotp_init(void)
{
    CAN_HandleTypeDef *hcan = &ISOTP_STM32_CAN_HANDLE;

    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull       = GPIO_NOPULL;
    gpio.Alternate  = ISOTP_STM32_CAN_TX_GPIO_AF;

    gpio.Pin       = ISOTP_STM32_CAN_TX_GPIO_PIN;
    HAL_GPIO_Init(ISOTP_STM32_CAN_TX_GPIO_PORT, &gpio);

    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull       = GPIO_PULLUP;
    gpio.Alternate  = ISOTP_STM32_CAN_RX_GPIO_AF;

    gpio.Pin       = ISOTP_STM32_CAN_RX_GPIO_PIN;
    HAL_GPIO_Init(ISOTP_STM32_CAN_RX_GPIO_PORT, &gpio);

    hcan->Instance = CAN1;

#if (ISOTP_STM32_CAN_BAUDRATE == 500)
    hcan->Init.Prescaler     = 8;
    hcan->Init.TimeSeg1      = CAN_TIMESEG1_12TQ;
    hcan->Init.TimeSeg2      = CAN_TIMESEG2_3TQ;
#elif (ISOTP_STM32_CAN_BAUDRATE == 250)
    hcan->Init.Prescaler     = 16;
    hcan->Init.TimeSeg1      = CAN_TIMESEG1_12TQ;
    hcan->Init.TimeSeg2      = CAN_TIMESEG2_3TQ;
#elif (ISOTP_STM32_CAN_BAUDRATE == 1000)
    hcan->Init.Prescaler     = 4;
    hcan->Init.TimeSeg1      = CAN_TIMESEG1_12TQ;
    hcan->Init.TimeSeg2      = CAN_TIMESEG2_3TQ;
#else
    hcan->Init.Prescaler     = 8;
    hcan->Init.TimeSeg1      = CAN_TIMESEG1_12TQ;
    hcan->Init.TimeSeg2      = CAN_TIMESEG2_3TQ;
#endif

    hcan->Init.SyncJumpWidth   = CAN_SJW_1TQ;
    hcan->Init.TimeTriggeredMode = DISABLE;
    hcan->Init.AutoBusOff      = ENABLE;
    hcan->Init.AutoWakeUp       = DISABLE;
    hcan->Init.AutoRetx        = ENABLE;
    hcan->Init.ReceiveFifoLocked = DISABLE;
    hcan->Init.TransmitFifoPriority = DISABLE;
    hcan->Init.Mode            = CAN_MODE_NORMAL;

    if (HAL_CAN_Init(hcan) != HAL_OK)
    {
        return -1;
    }

    CAN_FilterTypeDef filter = {0};
    filter.FilterBank          = 0;
    filter.FilterMode          = CAN_FILTERMODE_IDMASK;
    filter.FilterScale         = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh        = 0x0000;
    filter.FilterIdLow         = 0x0000;
    filter.FilterMaskIdHigh    = 0x0000;
    filter.FilterMaskIdLow     = 0x0000;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation    = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(hcan, &filter) != HAL_OK)
    {
        return -2;
    }

    if (HAL_CAN_Start(hcan) != HAL_OK)
    {
        return -3;
    }

    if (HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        return -4;
    }

    g_rx_buf.head = 0;
    g_rx_buf.tail = 0;
    g_initialized = 1;

    return 0;
}

static int stm32_isotp_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    CAN_HandleTypeDef *hcan = &ISOTP_STM32_CAN_HANDLE;
    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;

    if (!g_initialized)
    {
        return -1;
    }

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

int stm32_isotp_receive(uint32_t *id, uint8_t *data, uint8_t *len)
{
    if (g_rx_buf.head == g_rx_buf.tail)
    {
        return 0;
    }

    if (id != NULL)
    {
        *id = g_rx_buf.frames[g_rx_buf.tail].id;
    }

    if (data != NULL && len != NULL)
    {
        *len = g_rx_buf.frames[g_rx_buf.tail].dlc;
        (void)memcpy(data, g_rx_buf.frames[g_rx_buf.tail].data, g_rx_buf.frames[g_rx_buf.tail].dlc);
    }

    g_rx_buf.tail = (g_rx_buf.tail + 1) % ISOTP_STM32_RX_BUF_SIZE;

    return 1;
}


static int stm32_isotp_deinit(void)
{
    CAN_HandleTypeDef *hcan = &ISOTP_STM32_CAN_HANDLE;

    if (!g_initialized)
    {
        return 0;
    }

    HAL_CAN_DeactivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_Stop(hcan);
    HAL_CAN_DeInit(hcan);

    __HAL_RCC_CAN1_CLK_DISABLE();

    g_initialized = 0;
    g_rx_buf.head = 0;
    g_rx_buf.tail = 0;

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
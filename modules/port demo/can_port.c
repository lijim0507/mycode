/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "can_port.h"

#include <stdbool.h>
#include <string.h>

/*
 * 以下平台头文件在实际移植时按需替换：
 *
 * ESP32:
 *   #include "driver/twai.h"
 *   #include "freertos/FreeRTOS.h"
 *   #include "freertos/queue.h"
 *
 * STM32:
 *   #include "stm32f1xx_hal.h"
 */

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/*
 * CAN 驱动模式：
 *   CAN_MODE_POLLING      — 裸机：receive 直接读 RX mailbox/FIFO 寄存器
 *   CAN_MODE_INTERRUPT    — 裸机：RX ISR → ring buffer，receive 从 ring 取
 *   CAN_MODE_RTOS         — RTOS：RX ISR → FreeRTOS queue，receive 从 queue 取
 */

#ifndef CAN_DRIVER_MODE
#define CAN_DRIVER_MODE     CAN_MODE_POLLING
#endif

#define CAN_MODE_POLLING    0
#define CAN_MODE_INTERRUPT  1
#define CAN_MODE_RTOS       2

#ifndef CAN_RX_RING_SIZE
#define CAN_RX_RING_SIZE    64
#endif

/* --- 每帧缓冲区单元 --- */
typedef struct
{
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
} can_frame_buf_t;

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/*
 * 内部上下文 —— 移植时用实际的平台类型替换 PLATFORM_CAN_HANDLE：
 *
 * ESP32:   typedef twai_handle_t PLATFORM_CAN_HANDLE;  // 或直接用 CAN 单元号
 * STM32:   typedef CAN_HandleTypeDef * PLATFORM_CAN_HANDLE;
 */
typedef void * PLATFORM_CAN_HANDLE;

typedef struct
{
    PLATFORM_CAN_HANDLE  handle;

#if CAN_DRIVER_MODE == CAN_MODE_INTERRUPT
    /*
     * 裸机 ISR 模式 —— 简单 ring buffer
     *
     * ISR 写入 can_frame_buf_t 到 ring[rx_head]，head 推进。
     * receive() 从 ring[rx_tail] 读取，tail 推进。
     * 没有 RTOS 信号量 —— receive() 直接忙等或超时返回。
     */
#if 0
    can_frame_buf_t      rx_ring[CAN_RX_RING_SIZE];
    volatile uint16_t    rx_head;
    volatile uint16_t    rx_tail;
#endif
#endif

#if CAN_DRIVER_MODE == CAN_MODE_RTOS
    /*
     * RTOS 模式 —— FreeRTOS queue
     *
     * ISR 发送 can_frame_buf_t 到 rx_queue (xQueueSendFromISR)。
     * receive() 从 rx_queue 取数据 (xQueueReceive)。
     *
     * QueueHandle_t        rx_queue;
     *                      队列深度 = CAN_RX_RING_SIZE
     *                      元素大小 = sizeof(can_frame_buf_t)
     */
    void                 *rx_queue;
#endif
} can_port_ctx_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  platform_can_init(void);
static int  platform_can_deinit(void);
static int  platform_can_send(uint32_t id, const uint8_t *data, uint8_t len);
static int  platform_can_receive(uint32_t *id, uint8_t *data, uint8_t *len,
                                 uint32_t timeout_ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static can_port_ctx_t g_can_ctx;
static bool           g_can_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取平台 CAN 驱动实例
 * @return 指向静态 const can_port_driver_t 的指针
 */
const can_port_driver_t *can_port_get_driver(void)
{
    static const can_port_driver_t driver = {
        .init    = platform_can_init,
        .deinit  = platform_can_deinit,
        .send    = platform_can_send,
        .receive = platform_can_receive,
    };

    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/******************************************************************************/
/*  init                                                                      */
/******************************************************************************/

/**
 * @brief  初始化 CAN 外设
 *
 * @return 0: 成功, -1: 失败
 */
static int platform_can_init(void)
{
    /*
     * ESP32 实现步骤（TWAI）：
     *   1. twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
     *        TX_GPIO, RX_GPIO, TWAI_MODE_NORMAL);
     *   2. twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
     *   3. twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
     *   4. twai_driver_install(&g_config, &t_config, &f_config);
     *   5. twai_start();
     *
     * STM32 实现步骤（bxCAN）：
     *   1. g_can_ctx.handle = &hcan1;  // CubeMX 生成的句柄
     *   2. HAL_CAN_Init(g_can_ctx.handle);
     *   3. HAL_CAN_ConfigFilter(g_can_ctx.handle, &filter);  // 配置接收过滤器
     *   4. HAL_CAN_Start(g_can_ctx.handle);
     */

#if CAN_DRIVER_MODE == CAN_MODE_INTERRUPT
    /*
     *   4. 初始化 ring buffer：rx_head = rx_tail = 0
     *   5. 使能 CAN RX 中断：
     *      ESP32: twai_register_rx_cb(rx_isr_handler)、twai_set_rx_isr_enable()
     *      STM32: HAL_CAN_ActivateNotification(g_can_ctx.handle,
     *              CAN_IT_RX_FIFO0_MSG_PENDING) + HAL_NVIC 配置
     *   6. ISR 中调用 CAN_RX_ISR() 存帧到 ring buffer
     */

#elif CAN_DRIVER_MODE == CAN_MODE_RTOS
    /*
     *   4. g_can_ctx.rx_queue = xQueueCreate(CAN_RX_RING_SIZE, sizeof(can_frame_buf_t));
     *   5. if (g_can_ctx.rx_queue == NULL) return -1;
     *   6. 使能 CAN RX 中断，ISR 中调用 xQueueSendFromISR(rx_queue, &frame, &woken)
     */
#endif

    g_can_initialized = true;
    return 0;
}

/******************************************************************************/
/*  deinit                                                                    */
/******************************************************************************/

/**
 * @brief  释放 CAN 外设资源
 *
 * @return 0: 成功
 */
static int platform_can_deinit(void)
{
    if (!g_can_initialized)
    {
        return 0;
    }

    /*
     * ESP32:   twai_stop(); twai_driver_uninstall();
     * STM32:   HAL_CAN_Stop(g_can_ctx.handle);
     *           HAL_CAN_DeInit(g_can_ctx.handle);
     */

#if CAN_DRIVER_MODE == CAN_MODE_RTOS
    /*
     *   vQueueDelete(g_can_ctx.rx_queue);
     */
#endif

    g_can_initialized = false;
    return 0;
}

/******************************************************************************/
/*  send                                                                      */
/******************************************************************************/

/**
 * @brief  发送 CAN 帧
 *
 * CAN 发送始终是轮询或硬件队列 —— 不区分 MODE。
 * 三个 mailbox 用完后返回忙（本次不实现异步发送队列）。
 *
 * @param  id    CAN 标识符（11-bit 或 29-bit）
 * @param  data  数据（最多 8 字节）
 * @param  len   数据长度（0-8）
 * @return 0: 成功, -1: 参数错误
 */
static int platform_can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (len > 8)
    {
        return -1;
    }

    /*
     * ESP32 (TWAI)：
     *   twai_message_t msg  = { .identifier = id, .data_length_code = len,
     *                           .extd = (id > 0x7FF) ? 1 : 0 };
     *   memcpy(msg.data, data, len);
     *   return (twai_transmit(&msg, pdMS_TO_TICKS(10)) == ESP_OK) ? 0 : -1;
     *
     * STM32 (bxCAN)：
     *   CAN_TxHeaderTypeDef header = { .StdId = id & 0x7FF, .ExtId = id,
     *                                   .IDE = (id > 0x7FF) ? CAN_ID_EXT : CAN_ID_STD,
     *                                   .DLC = len };
     *   uint32_t mailbox;
     *   return (HAL_CAN_AddTxMessage(g_can_ctx.handle, &header, (uint8_t *)data,
     *                                &mailbox) == HAL_OK) ? 0 : -1;
     */
    return 0;
}

/******************************************************************************/
/*  receive                                                                   */
/******************************************************************************/

/**
 * @brief  接收 CAN 帧
 *
 * @param  id          输出：CAN 标识符
 * @param  data        输出：数据缓冲区（至少 8 字节）
 * @param  len         输出：实际数据长度（0-8）
 * @param  timeout_ms  超时时间（0 = 非阻塞，立即返回）
 * @return 0: 成功, -1: 超时 / 无数据
 */
static int platform_can_receive(uint32_t *id, uint8_t *data, uint8_t *len,
                                 uint32_t timeout_ms)
{
#if CAN_DRIVER_MODE == CAN_MODE_POLLING
    /*
     * 裸机轮询：
     *
     * ESP32:
     *   twai_message_t msg;
     *   esp_err_t ret = twai_receive(&msg, pdMS_TO_TICKS(timeout_ms));
     *   if (ret != ESP_OK) return -1;
     *   *id = msg.identifier; *len = msg.data_length_code;
     *   memcpy(data, msg.data, msg.data_length_code);
     *   return 0;
     *
     * STM32:
     *   CAN_RxHeaderTypeDef header;
     *   uint32_t start = HAL_GetTick();
     *   while (HAL_CAN_GetRxFifoFillLevel(g_can_ctx.handle, CAN_RX_FIFO0) == 0)
     *   {
     *       if (timeout_ms > 0 && (HAL_GetTick() - start) >= timeout_ms)
     *           return -1;   // 超时
     *   }
     *   HAL_CAN_GetRxMessage(g_can_ctx.handle, CAN_RX_FIFO0, &header, data);
     *   *id = header.IDE == CAN_ID_EXT ? header.ExtId : header.StdId;
     *   *len = header.DLC;
     *   return 0;
     */

#elif CAN_DRIVER_MODE == CAN_MODE_INTERRUPT
    /*
     * 裸机 ISR 模式 —— 从 ring buffer 取帧：
     *
     *   uint32_t start = platform_get_tick();
     *   while (rx_tail == rx_head)                     // ring buffer 空
     *   {
     *       if (timeout_ms > 0 &&
     *           (platform_get_tick() - start) >= timeout_ms)
     *           return -1;                              // 超时
     *   }
     *   can_frame_buf_t *frame = &rx_ring[rx_tail];
     *   *id = frame->id;
     *   *len = frame->dlc;
     *   memcpy(data, frame->data, frame->dlc);
     *   rx_tail = (rx_tail + 1) % CAN_RX_RING_SIZE;
     *   return 0;
     *
     * ISR 实现（平台相关）：
     *   void CAN_RX_ISR(void)
     *   {
     *       can_frame_buf_t frame;
     *       // 从硬件寄存器读帧到 frame
     *       uint16_t next = (rx_head + 1) % CAN_RX_RING_SIZE;
     *       if (next != rx_tail)                        // 防溢出
     *       {
     *           memcpy(&rx_ring[rx_head], &frame, sizeof(can_frame_buf_t));
     *           rx_head = next;
     *       }
     *   }
     */

#else   /* CAN_MODE_RTOS */
    /*
     * RTOS 模式 —— 从 FreeRTOS queue 取帧：
     *
     *   can_frame_buf_t frame;
     *   if (xQueueReceive(g_can_ctx.rx_queue, &frame,
     *                     pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
     *       return -1;                                  // 超时
     *   *id = frame.id;
     *   *len = frame.dlc;
     *   memcpy(data, frame.data, frame.dlc);
     *   return 0;
     *
     * ISR 实现：
     *   void CAN_RX_ISR(void)
     *   {
     *       can_frame_buf_t frame;
     *       BaseType_t woken = pdFALSE;
     *       // 从硬件寄存器读帧到 frame
     *       xQueueSendFromISR(g_can_ctx.rx_queue, &frame, &woken);
     *       if (woken) portYIELD_FROM_ISR();
     *   }
     */
#endif

    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

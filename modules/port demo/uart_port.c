/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "uart_port.h"

#include <stdbool.h>
#include <string.h>

/*
 * 以下平台头文件在实际移植时按需替换：
 *
 * ESP32:
 *   #include "driver/uart.h"
 *   #include "freertos/FreeRTOS.h"
 *   #include "freertos/semphr.h"
 *
 * STM32:
 *   #include "stm32f1xx_hal.h"
 */

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/*
 * UART 驱动模式：
 *   UART_MODE_POLLING      — 裸机：send 轮询 TXE，receive 轮询 RXNE
 *   UART_MODE_INTERRUPT    — 裸机：RX ISR → ring buffer，receive 从 ring 取
 *   UART_MODE_RTOS         — RTOS：RX ISR → ring buffer → sem give，receive sem take
 */

#ifndef UART_DRIVER_MODE
#define UART_DRIVER_MODE    UART_MODE_POLLING
#endif

#define UART_MODE_POLLING   0
#define UART_MODE_INTERRUPT 1
#define UART_MODE_RTOS      2

#ifndef UART_RX_RING_SIZE
#define UART_RX_RING_SIZE   512
#endif

#ifndef UART_TX_TIMEOUT_MS
#define UART_TX_TIMEOUT_MS  100
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/*
 * 内部上下文 —— 移植时用实际的平台类型替换 PLATFORM_UART_HANDLE：
 *
 * ESP32:   typedef uart_port_t PLATFORM_UART_HANDLE;
 * STM32:   typedef UART_HandleTypeDef * PLATFORM_UART_HANDLE;
 */
typedef void * PLATFORM_UART_HANDLE;

typedef struct
{
    PLATFORM_UART_HANDLE  handle;

#if (UART_DRIVER_MODE == UART_MODE_INTERRUPT) || \
    (UART_DRIVER_MODE == UART_MODE_RTOS)
    /*
     * ISR / RTOS 共用 ring buffer
     *
     * ISR 在 UART RX 中断里调用 UART_RX_ISR() 把数据写入 rx_ring。
     * POLLING 模式不需要 ring buffer —— receive 直接从硬件 FIFO 读。
     */
#if 0
    uint8_t              rx_ring[UART_RX_RING_SIZE];
    volatile uint16_t    rx_head;           /* ISR 写，receive 读 */
    volatile uint16_t    rx_tail;
#endif
#endif

#if UART_DRIVER_MODE == UART_MODE_RTOS
    /*
     * RTOS 接收信号量
     *
     * ISR 在存入数据后 xSemaphoreGiveFromISR(rx_sem)。
     * receive() 中 xSemaphoreTake(rx_sem, timeout)。
     *
     * 不需要 mutex：UART 是点对点的，不存在多主机竞争。
     *
     * SemaphoreHandle_t    rx_sem;          // 二进制信号量
     */
    void                 *rx_sem;
#endif
} uart_port_ctx_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  platform_uart_init(void);
static int  platform_uart_deinit(void);
static int  platform_uart_send(const uint8_t *data, uint32_t len);
static int  platform_uart_receive(uint8_t *data, uint32_t len, uint32_t timeout_ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static uart_port_ctx_t g_uart_ctx;
static bool            g_uart_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取平台 UART 驱动实例
 * @return 指向静态 const uart_port_driver_t 的指针
 */
const uart_port_driver_t *uart_port_get_driver(void)
{
    static const uart_port_driver_t driver = {
        .init    = platform_uart_init,
        .deinit  = platform_uart_deinit,
        .send    = platform_uart_send,
        .receive = platform_uart_receive,
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
 * @brief  初始化 UART 外设
 *
 * @return 0: 成功, -1: 失败
 */
static int platform_uart_init(void)
{
    /*
     * ESP32 实现步骤：
     *   1. uart_config_t uart_config = { .baud_rate = 115200,
     *        .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
     *        .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE };
     *   2. uart_param_config(UART_NUM_1, &uart_config);
     *   3. uart_set_pin(UART_NUM_1, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE,
     *                   UART_PIN_NO_CHANGE);
     *   4. uart_driver_install(UART_NUM_1, UART_RX_RING_SIZE * 2,
     *                          UART_RX_RING_SIZE * 2, 0, NULL, 0);
     *   5. g_uart_ctx.handle = UART_NUM_1;
     *   注意：ESP32 的 uart_driver_install 内部自带 ring buffer 和 ISR，
     *   可以直接用 uart_read_bytes(timeout) 而不需要手动 ring buffer。
     *
     * STM32 实现步骤：
     *   1. g_uart_ctx.handle = &huart1;  // CubeMX 生成的句柄
     *   2. HAL_UART_Init(g_uart_ctx.handle);
     *   3. GPIO 模式 + 中断已在 CubeMX 中配置
     */

#if (UART_DRIVER_MODE == UART_MODE_INTERRUPT) || \
    (UART_DRIVER_MODE == UART_MODE_RTOS)
    /*
     *   4. 初始化 ring buffer：rx_head = rx_tail = 0
     *   5. 使能 RX 中断：
     *      STM32: HAL_UART_Receive_IT(g_uart_ctx.handle, &rx_byte, 1)
     *            （每次收 1 字节到 rx_byte，RX 完成中断里存入 ring，再重新启动）
     */

#if UART_DRIVER_MODE == UART_MODE_RTOS
    /*
     *   6. g_uart_ctx.rx_sem = xSemaphoreCreateBinary();
     *   7. if (g_uart_ctx.rx_sem == NULL) return -1;
     */
#endif
#endif

    g_uart_initialized = true;
    return 0;
}

/******************************************************************************/
/*  deinit                                                                    */
/******************************************************************************/

/**
 * @brief  释放 UART 外设资源
 *
 * @return 0: 成功
 */
static int platform_uart_deinit(void)
{
    if (!g_uart_initialized)
    {
        return 0;
    }

    /*
     * ESP32:   uart_driver_delete(UART_NUM_1);
     * STM32:   HAL_UART_DeInit(g_uart_ctx.handle);
     */

#if UART_DRIVER_MODE == UART_MODE_RTOS
    /*
     *   vSemaphoreDelete(g_uart_ctx.rx_sem);
     */
#endif

    g_uart_initialized = false;
    return 0;
}

/******************************************************************************/
/*  send                                                                      */
/******************************************************************************/

/**
 * @brief  UART 发送数据
 *
 * 发送始终是同步阻塞 —— 等待所有数据发送完毕才返回。
 * 三种模式的实现相同（都用 TXE 轮询或 TX FIFO 超时等待）。
 *
 * @param  data  发送数据
 * @param  len   发送字节数
 * @return 0: 成功, -1: 参数错误
 */
static int platform_uart_send(const uint8_t *data, uint32_t len)
{
    if (!data || len == 0)
    {
        return -1;
    }

    /*
     * ESP32:
     *   return (uart_write_bytes(g_uart_ctx.handle, data, len) == len) ? 0 : -1;
     *   // 或 uart_wait_tx_done(g_uart_ctx.handle, pdMS_TO_TICKS(100));
     *
     * STM32 (polling):
     *   return (HAL_UART_Transmit(g_uart_ctx.handle, (uint8_t *)data, len,
     *                             UART_TX_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
     *
     * STM32 (IT 模式下 send 也可以用 IT，但阻塞等待完成更简单)：
     *   HAL_UART_Transmit_IT(g_uart_ctx.handle, (uint8_t *)data, len);
     *   // 等待传输完成标志或信号量
     */
    return 0;
}

/******************************************************************************/
/*  receive                                                                   */
/******************************************************************************/

/**
 * @brief  UART 接收数据
 *
 * @param  data        输出：接收缓冲区
 * @param  len         期望读取的字节数
 * @param  timeout_ms  超时时间（0 = 非阻塞，立即返回已有数据）
 * @return 实际读取的字节数, 超时返回 -1
 */
static int platform_uart_receive(uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    if (!data || len == 0)
    {
        return -1;
    }

#if UART_DRIVER_MODE == UART_MODE_POLLING
    /*
     * 裸机轮询：
     *
     * ESP32（自带 ring buffer，即使 polling 也有效）：
     *   int ret = uart_read_bytes(g_uart_ctx.handle, data, len,
     *                             pdMS_TO_TICKS(timeout_ms));
     *   return ret;   // 返回实际读取字节数
     *
     * STM32：
     *   uint32_t count = 0;
     *   uint32_t start = HAL_GetTick();
     *   while (count < len)
     *   {
     *       while (__HAL_UART_GET_FLAG(g_uart_ctx.handle, UART_FLAG_RXNE) == RESET)
     *       {
     *           if (timeout_ms > 0 &&
     *               (HAL_GetTick() - start) >= timeout_ms)
     *               return (count > 0) ? (int)count : -1;
     *       }
     *       data[count++] = (uint8_t)(g_uart_ctx.handle->Instance->DR);
     *   }
     *   return (int)count;
     */

#elif UART_DRIVER_MODE == UART_MODE_INTERRUPT
    /*
     * 裸机 ISR 模式 —— 从 ring buffer 取：
     *
     *   uint32_t count = 0;
     *   uint32_t start = platform_get_tick();
     *   while (count < len)
     *   {
     *       if (rx_tail == rx_head)                    // ring buffer 空
     *       {
     *           if (timeout_ms > 0 &&
     *               (platform_get_tick() - start) >= timeout_ms)
     *               return (count > 0) ? (int)count : -1;
     *           continue;
     *       }
     *       data[count++] = rx_ring[rx_tail];
     *       rx_tail = (rx_tail + 1) % UART_RX_RING_SIZE;
     *       start = platform_get_tick();               // 有数据就重置超时
     *   }
     *   return (int)count;
     *
     * ISR 实现：
     *   void UART_RX_ISR(void)
     *   {
     *       uint8_t byte = (uint8_t)(UART->DR);        // 读数据寄存器（清标志）
     *       uint16_t next = (rx_head + 1) % UART_RX_RING_SIZE;
     *       if (next != rx_tail)                       // 还有空间
     *       {
     *           rx_ring[rx_head] = byte;
     *           rx_head = next;
     *       }
     *   }
     */

#else   /* UART_MODE_RTOS */
    /*
     * RTOS 模式 —— 环形缓冲区 + 信号量：
     *
     * 接收流程：
     *
     *   uint32_t count = 0;
     *   while (count < len)
     *   {
     *       // 1. 等待至少 1 字节数据
     *       if (xSemaphoreTake(g_uart_ctx.rx_sem, timeout_ticks) != pdTRUE)
     *           return (count > 0) ? (int)count : -1;
     *
     *       // 2. 从 ring buffer 读数据（关闭中断防止竞争）
     *       taskENTER_CRITICAL();
     *       while (rx_tail != rx_head && count < len)
     *       {
     *           data[count++] = rx_ring[rx_tail];
     *           rx_tail = (rx_tail + 1) % UART_RX_RING_SIZE;
     *       }
     *       taskEXIT_CRITICAL();
     *
     *       // 3. 重置超时（因为刚刚收到了数据）
     *       if (count < len)
     *           timeout_ticks = pdMS_TO_TICKS(timeout_ms);
     *   }
     *   return (int)count;
     *
     * ISR 实现：
     *   void UART_RX_ISR(void)
     *   {
     *       BaseType_t woken = pdFALSE;
     *       uint8_t byte = (uint8_t)(UART->DR);
     *       uint16_t next = (rx_head + 1) % UART_RX_RING_SIZE;
     *       if (next != rx_tail)
     *       {
     *           rx_ring[rx_head] = byte;
     *           rx_head = next;
     *       }
     *       xSemaphoreGiveFromISR(g_uart_ctx.rx_sem, &woken);
     *       if (woken) portYIELD_FROM_ISR();
     *   }
     */
#endif

    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

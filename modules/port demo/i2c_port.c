/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "i2c_port.h"

#include <stdbool.h>

/*
 * 以下平台头文件在实际移植时按需替换：
 *
 * ESP32 (硬件 I2C):
 *   #include "driver/i2c.h"
 *   #include "freertos/FreeRTOS.h"
 *   #include "freertos/semphr.h"
 *
 * ESP32 (软件 I2C / GPIO 位带):
 *   #include "driver/gpio.h"
 *   #include "esp_rom_sys.h"
 *
 * STM32:
 *   #include "stm32f1xx_hal.h"
 */

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/*
 * I2C 驱动模式：
 *   I2C_MODE_POLLING    — 裸机：读 / 写直接轮询硬件标志（TXE/RXNE/BTF）
 *   I2C_MODE_INTERRUPT  — 裸机：TX/RX ISR 搬运数据，主循环信号量/标志同步
 *   I2C_MODE_RTOS       — RTOS：ISR + FreeRTOS semaphore/mutex
 */

#ifndef I2C_DRIVER_MODE
#define I2C_DRIVER_MODE     I2C_MODE_POLLING
#endif

#define I2C_MODE_POLLING    0
#define I2C_MODE_INTERRUPT  1
#define I2C_MODE_RTOS       2

#ifndef I2C_RX_RING_SIZE
#define I2C_RX_RING_SIZE    256
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/*
 * 内部上下文 —— 移植时用实际的平台类型替换 PLATFORM_I2C_HANDLE：
 *
 * ESP32:   typedef i2c_port_t PLATFORM_I2C_HANDLE;
 * STM32:   typedef I2C_HandleTypeDef * PLATFORM_I2C_HANDLE;
 */
typedef void * PLATFORM_I2C_HANDLE;

typedef struct
{
    PLATFORM_I2C_HANDLE  handle;

#if I2C_DRIVER_MODE == I2C_MODE_INTERRUPT
    /*
     * 裸机 ISR 模式 —— 传输完成标志
     *
     * ISR 设置此标志，主循环的 write/read 等待它。
     * volatile int  transfer_done;
     */
#endif

#if I2C_DRIVER_MODE == I2C_MODE_RTOS
    /*
     * RTOS 模式 —— 多任务互斥 + 传输完成信号量
     *
     * SemaphoreHandle_t    mutex;          // 多任务互斥锁
     * SemaphoreHandle_t    transfer_sem;   // 传输完成信号量（ISR give）
     */
    void                 *mutex;
    void                 *transfer_sem;
#endif
} i2c_port_ctx_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  platform_i2c_init(void);
static int  platform_i2c_deinit(void);
static int  platform_i2c_write(uint8_t dev_addr, const uint8_t *data,
                               uint16_t len, uint32_t timeout_ms);
static int  platform_i2c_read(uint8_t dev_addr, uint8_t *data,
                              uint16_t len, uint32_t timeout_ms);
static int  platform_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data,
                                    uint16_t wr_len, uint8_t *rd_data,
                                    uint16_t rd_len, uint32_t timeout_ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static i2c_port_ctx_t g_i2c_ctx;
static bool           g_i2c_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取平台 I2C 驱动实例
 * @return 指向静态 const i2c_port_driver_t 的指针
 */
const i2c_port_driver_t *i2c_port_get_driver(void)
{
    static const i2c_port_driver_t driver = {
        .init       = platform_i2c_init,
        .deinit     = platform_i2c_deinit,
        .write      = platform_i2c_write,
        .read       = platform_i2c_read,
        .write_read = platform_i2c_write_read,
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
 * @brief  初始化 I2C 外设
 *
 * @return 0: 成功, -1: 失败
 */
static int platform_i2c_init(void)
{
    /*
     * ESP32 实现步骤（硬件 I2C）：
     *   1. i2c_config_t conf = { .mode = I2C_MODE_MASTER, .sda_io_num = ...,
     *                            .scl_io_num = ..., .master.clk_speed = 400000, ... };
     *   2. i2c_param_config(I2C_NUM_0, &conf);
     *   3. i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
     *   4. g_i2c_ctx.handle = I2C_NUM_0;
     *
     * STM32 实现步骤：
     *   1. g_i2c_ctx.handle = &hi2c1;  // CubeMX 生成的句柄
     *   2. HAL_I2C_Init(g_i2c_ctx.handle);
     */

#if I2C_DRIVER_MODE == I2C_MODE_RTOS
    /*
     *   3. g_i2c_ctx.mutex = xSemaphoreCreateMutex();
     *   4. g_i2c_ctx.transfer_sem = xSemaphoreCreateBinary();
     */
#endif

    g_i2c_initialized = true;
    return 0;
}

/******************************************************************************/
/*  deinit                                                                    */
/******************************************************************************/

/**
 * @brief  释放 I2C 外设资源
 *
 * @return 0: 成功
 */
static int platform_i2c_deinit(void)
{
    if (!g_i2c_initialized)
    {
        return 0;
    }

    /*
     * ESP32:   i2c_driver_delete(I2C_NUM_0);
     *           gpio_reset_pin(SDA_GPIO); gpio_reset_pin(SCL_GPIO);
     * STM32:   HAL_I2C_DeInit(g_i2c_ctx.handle);
     */

#if I2C_DRIVER_MODE == I2C_MODE_RTOS
    /*
     *   vSemaphoreDelete(g_i2c_ctx.transfer_sem);
     *   vSemaphoreDelete(g_i2c_ctx.mutex);
     */
#endif

    g_i2c_initialized = false;
    return 0;
}

/******************************************************************************/
/*  ACQUIRE / RELEASE (RTOS)                                                  */
/******************************************************************************/

#if I2C_DRIVER_MODE == I2C_MODE_RTOS
/*
 * RTOS 下每次 I2C 事务前加锁，防止多任务交错发送 START 条件导致总线混乱。
 *
 * 与 SPI 不同：SPI 有 CS 隔离多设备，I2C 是所有设备共享同一条总线，
 * 不加锁可能导致：taskA 发完地址还没发数据，taskB 切进来发了一个 START。
 */
#define I2C_LOCK()   /* xSemaphoreTake(g_i2c_ctx.mutex, portMAX_DELAY) */
#define I2C_UNLOCK() /* xSemaphoreGive(g_i2c_ctx.mutex) */
#else
#define I2C_LOCK()
#define I2C_UNLOCK()
#endif

/******************************************************************************/
/*  write                                                                     */
/******************************************************************************/

/**
 * @brief  I2C 主机发送
 *
 * 序列: START → dev_addr(W) → data → STOP
 *
 * @return 0: 成功, -1: 参数错误, -3: NACK / 超时
 */
static int platform_i2c_write(uint8_t dev_addr, const uint8_t *data,
                               uint16_t len, uint32_t timeout_ms)
{
    I2C_LOCK();

#if I2C_DRIVER_MODE == I2C_MODE_POLLING
    /*
     * ESP32 (i2c_master_write_to_device):
     *   esp_err_t ret = i2c_master_write_to_device(g_i2c_ctx.handle, dev_addr,
     *       data, len, pdMS_TO_TICKS(timeout_ms));
     *   result = (ret == ESP_OK) ? 0 : -3;
     *
     * STM32:
     *   HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(g_i2c_ctx.handle,
     *       (uint16_t)(dev_addr << 1), (uint8_t *)data, len, timeout_ms);
     *   result = (ret == HAL_OK) ? 0 : -3;
     */

#elif I2C_DRIVER_MODE == I2C_MODE_INTERRUPT
    /*
     * 裸机 ISR 模式：
     *   1. 调用 HAL_I2C_Master_Transmit_IT() 启动 ISR 传输
     *   2. 等待传输完成标志（while (!transfer_done && !timeout)）
     *   3. 超时返回 -3，完成返回 0
     */

#else   /* I2C_MODE_RTOS */
    /*
     * RTOS 模式：
     *   1. 调用 HAL_I2C_Master_Transmit_IT() 启动传输
     *   2. xSemaphoreTake(transfer_sem, pdMS_TO_TICKS(timeout_ms))
     *   3. ISR 完成后 xSemaphoreGiveFromISR(transfer_sem)
     *   4. 信号量超时 → -3，成功 → 0
     */
#endif

    I2C_UNLOCK();
    return 0;
}

/******************************************************************************/
/*  read                                                                      */
/******************************************************************************/

/**
 * @brief  I2C 主机接收
 *
 * 序列: START → dev_addr(R) → read data → STOP
 *
 * @return 0: 成功, -1: 参数错误, -3: NACK / 超时
 */
static int platform_i2c_read(uint8_t dev_addr, uint8_t *data,
                              uint16_t len, uint32_t timeout_ms)
{
    I2C_LOCK();

    /*
     * 三种模式的实现思路与 write() 对称：
     *
     * ESP32 (polling):
     *   ret = i2c_master_read_from_device(g_i2c_ctx.handle, dev_addr,
     *       data, len, pdMS_TO_TICKS(timeout_ms));
     *
     * STM32 (polling):
     *   ret = HAL_I2C_Master_Receive(g_i2c_ctx.handle, (uint16_t)(dev_addr << 1),
     *       data, len, timeout_ms);
     *
     * INTERRUPT / RTOS 模式复用 write() 的同步机制（标志/信号量 + _IT 函数）。
     */

    I2C_UNLOCK();
    return 0;
}

/******************************************************************************/
/*  write_read                                                                */
/******************************************************************************/

/**
 * @brief  I2C 先写后读（repeated start）
 *
 * 序列: START → dev_addr(W) → wr_data → RESTART → dev_addr(R) → rd_data → STOP
 *
 * 典型用途：SMBus Read Word / Read Block、EEPROM 随机地址读
 *
 * @return 0: 成功, -1: 参数错误, -3: NACK / 超时
 */
static int platform_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data,
                                    uint16_t wr_len, uint8_t *rd_data,
                                    uint16_t rd_len, uint32_t timeout_ms)
{
    I2C_LOCK();

    /*
     * ESP32 (polling):
     *   ret = i2c_master_write_read_device(g_i2c_ctx.handle, dev_addr,
     *       wr_data, wr_len, rd_data, rd_len, pdMS_TO_TICKS(timeout_ms));
     *
     * STM32 (polling):
     *   裸 HAL 没有直接的 write_read API，需要手动分两步：
     *   1. HAL_I2C_Master_Sequential_Transmit_IT()  — 发送 wr_data (NO STOP)
     *   2. HAL_I2C_Master_Sequential_Receive_IT()   — 发送 repeated start + 读 (AUTO STOP)
     *
     * INTERRUPT / RTOS 模式同理，用 _IT 函数 + 同步机制。
     */

    I2C_UNLOCK();
    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "spi_port.h"

#include <stdbool.h>

/*
 * 以下平台头文件在实际移植时按需替换：
 *
 * ESP32:
 *   #include "driver/spi_master.h"
 *   #include "driver/gpio.h"
 *   #include "esp_rom_sys.h"               // esp_rom_delay_us()
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
 * SPI 驱动模式：
 *   SPI_MODE_POLLING  — 裸机：spi_write/read/transceive 轮询，delay 忙等
 *   SPI_MODE_RTOS     — RTOS：多任务互斥保护，delay_ms 用 vTaskDelay
 *
 * SPI 没有 ISR 接收模式 —— 它是主机驱动的同步总线，所有传输由主机主动发起。
 */

#ifndef SPI_DRIVER_MODE
#define SPI_DRIVER_MODE   SPI_MODE_POLLING
#endif

#define SPI_MODE_POLLING   0
#define SPI_MODE_RTOS      1

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/*
 * 内部上下文 —— 移植时用实际的平台类型替换 PLATFORM_SPI_HANDLE：
 *
 * ESP32:
 *   typedef spi_device_handle_t PLATFORM_SPI_HANDLE;
 *
 * STM32:
 *   typedef SPI_HandleTypeDef * PLATFORM_SPI_HANDLE;
 */
typedef void * PLATFORM_SPI_HANDLE;

typedef struct
{
    PLATFORM_SPI_HANDLE  handle;
    uint8_t              cs_gpio;

#if SPI_DRIVER_MODE == SPI_MODE_RTOS
    /* SemaphoreHandle_t    mutex; */
    void                 *mutex;
#endif
} spi_port_ctx_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  platform_spi_init(void);
static int  platform_spi_deinit(void);
static void platform_spi_cs_set(uint8_t level);
static int  platform_spi_write(const uint8_t *data, uint16_t len);
static int  platform_spi_read(uint8_t *data, uint16_t len);
static int  platform_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void platform_spi_delay_us(uint32_t us);
static void platform_spi_delay_ms(uint32_t ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static spi_port_ctx_t g_spi_ctx;
static bool           g_spi_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取平台 SPI 驱动实例
 * @return 指向静态 const spi_port_driver_t 的指针
 */
const spi_port_driver_t *spi_port_get_driver(void)
{
    static const spi_port_driver_t driver = {
        .init           = platform_spi_init,
        .deinit         = platform_spi_deinit,
        .cs_set         = platform_spi_cs_set,
        .spi_write      = platform_spi_write,
        .spi_read       = platform_spi_read,
        .spi_transceive = platform_spi_transceive,
        .delay_us       = platform_spi_delay_us,
        .delay_ms       = platform_spi_delay_ms,
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
 * @brief  初始化 SPI 外设
 *
 * @return 0: 成功, -1: 失败
 */
static int platform_spi_init(void)
{
    /*
     * ESP32 实现步骤：
     *   1. spi_bus_config_t bus_cfg = { .mosi_io_num = ..., .miso_io_num = ...,
     *                                    .sclk_io_num = ..., ... };
     *   2. spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_DISABLED);
     *   3. spi_device_interface_config_t dev_cfg = { .clock_speed_hz = ...,
     *                                                .mode = 0, .spics_io_num = -1, ... };
     *   4. spi_bus_add_device(SPI3_HOST, &dev_cfg, &g_spi_ctx.handle);
     *   5. gpio_config() 配置 CS 为推挽输出，初始拉高
     *
     * STM32 实现步骤：
     *   1. g_spi_ctx.handle = &hspi1;  // CubeMX 生成的句柄
     *   2. HAL_SPI_Init(g_spi_ctx.handle);
     *   3. GPIO 初始化 CS 引脚（推挽输出，上拉，初始高）
     */

#if SPI_DRIVER_MODE == SPI_MODE_RTOS
    /*
     * RTOS:
     *   4. g_spi_ctx.mutex = xSemaphoreCreateMutex();
     *   5. if (g_spi_ctx.mutex == NULL) return -1;
     */
#endif

    g_spi_initialized = true;
    return 0;
}

/******************************************************************************/
/*  deinit                                                                    */
/******************************************************************************/

/**
 * @brief  释放 SPI 外设资源
 *
 * @return 0: 成功
 */
static int platform_spi_deinit(void)
{
    if (!g_spi_initialized)
    {
        return 0;
    }

    /*
     * ESP32:
     *   spi_bus_remove_device(g_spi_ctx.handle);
     *   spi_bus_free(SPI3_HOST);
     *   gpio_reset_pin(CS_GPIO);
     *
     * STM32:
     *   HAL_SPI_DeInit(g_spi_ctx.handle);
     *   HAL_GPIO_DeInit(CS_PORT, CS_PIN);
     */

#if SPI_DRIVER_MODE == SPI_MODE_RTOS
    /*
     *   vSemaphoreDelete(g_spi_ctx.mutex);
     */
#endif

    g_spi_initialized = false;
    return 0;
}

/******************************************************************************/
/*  cs_set                                                                    */
/******************************************************************************/

/**
 * @brief  控制 CS 引脚
 * @param  level  0: 选中（拉低）, 1: 释放（拉高）
 */
static void platform_spi_cs_set(uint8_t level)
{
    /*
     * ESP32:   gpio_set_level(CS_GPIO, level);
     * STM32:   HAL_GPIO_WritePin(CS_PORT, CS_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
     */
}

/******************************************************************************/
/*  spi_write / spi_read / spi_transceive                                     */
/******************************************************************************/

/**
 * @brief  SPI 只写传输
 */
static int platform_spi_write(const uint8_t *data, uint16_t len)
{
    /*
     * ESP32:
     *   spi_transaction_t trans = { .length = len * 8, .tx_buffer = data,
     *                               .rx_buffer = NULL };
     *   return spi_device_polling_transmit(g_spi_ctx.handle, &trans) == ESP_OK ? 0 : -1;
     *
     * STM32:
     *   return HAL_SPI_Transmit(g_spi_ctx.handle, (uint8_t *)data, len,
     *                           HAL_MAX_DELAY) == HAL_OK ? 0 : -1;
     *
     * 注意：SPI 事务短（微秒级），轮询已足够高效，不需要 RTOS 异步化。
     */
    return 0;
}

/**
 * @brief  SPI 只读传输
 */
static int platform_spi_read(uint8_t *data, uint16_t len)
{
    /*
     * ESP32:
     *   spi_transaction_t trans = { .length = len * 8, .tx_buffer = NULL,
     *                               .rx_buffer = data };
     *   return spi_device_polling_transmit(g_spi_ctx.handle, &trans) == ESP_OK ? 0 : -1;
     *
     * STM32:
     *   return HAL_SPI_Receive(g_spi_ctx.handle, data, len,
     *                          HAL_MAX_DELAY) == HAL_OK ? 0 : -1;
     */
    return 0;
}

/**
 * @brief  SPI 全双工传输
 */
static int platform_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    /*
     * ESP32:
     *   spi_transaction_t trans = { .length = len * 8, .tx_buffer = tx,
     *                               .rx_buffer = rx };
     *   return spi_device_polling_transmit(g_spi_ctx.handle, &trans) == ESP_OK ? 0 : -1;
     *
     * STM32:
     *   return HAL_SPI_TransmitReceive(g_spi_ctx.handle, (uint8_t *)tx, rx, len,
     *                                  HAL_MAX_DELAY) == HAL_OK ? 0 : -1;
     */
    return 0;
}

/******************************************************************************/
/*  delay                                                                     */
/******************************************************************************/

/**
 * @brief  微秒延时
 */
static void platform_spi_delay_us(uint32_t us)
{
#if SPI_DRIVER_MODE == SPI_MODE_POLLING
    /*
     * ESP32:   esp_rom_delay_us(us);
     * STM32:   DWT cycle counter 延时或软件 NOP 循环
     */
#else
    /*
     * RTOS:
     *   微秒延时通常依然用硬件延时（SPI 时序是微秒级的，不能切任务）。
     *   不用 vTaskDelay —— 那最小粒度为 1 tick (通常 1ms)。
     *
     * ESP32:   esp_rom_delay_us(us);
     * STM32:   DWT cycle counter
     */
#endif
}

/**
 * @brief  毫秒延时
 */
static void platform_spi_delay_ms(uint32_t ms)
{
#if SPI_DRIVER_MODE == SPI_MODE_POLLING
    /*
     * ESP32:   vTaskDelay(pdMS_TO_TICKS(ms));   // 即使裸机也可以用 FreeRTOS
     * STM32:   HAL_Delay(ms);
     */
#else
    /*
     * RTOS:
     *   vTaskDelay(pdMS_TO_TICKS(ms));           // 让出 CPU，不忙等
     */
#endif
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

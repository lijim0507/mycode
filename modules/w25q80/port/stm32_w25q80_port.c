/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "w25q80_port.h"

#include "stm32f1xx_hal.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#ifndef W25Q80_STM32_SPI_HANDLE
#define W25Q80_STM32_SPI_HANDLE     (&hspi1)
#endif

#ifndef W25Q80_STM32_CS_GPIO_PORT
#define W25Q80_STM32_CS_GPIO_PORT    GPIOA
#endif

#ifndef W25Q80_STM32_CS_GPIO_PIN
#define W25Q80_STM32_CS_GPIO_PIN     GPIO_PIN_4
#endif

#ifndef W25Q80_STM32_TIMEOUT_MS
#define W25Q80_STM32_TIMEOUT_MS      100
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  stm32_w25q80_init(void);
static int  stm32_w25q80_deinit(void);
static void stm32_w25q80_cs_set(uint8_t level);
static int  stm32_w25q80_spi_write(const uint8_t *data, uint16_t len);
static int  stm32_w25q80_spi_read(uint8_t *data, uint16_t len);
static int  stm32_w25q80_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void stm32_w25q80_delay_us(uint32_t us);
static void stm32_w25q80_delay_ms(uint32_t ms);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static SPI_HandleTypeDef *g_spi_handle;
static bool                g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 STM32 W25Q80 port driver 实例
 *
 * @return 指向静态 driver 结构体的指针
 */
const w25q80_port_driver_t *w25q80_port_get_driver(void)
{
    static const w25q80_port_driver_t driver = {
        .init           = stm32_w25q80_init,
        .deinit         = stm32_w25q80_deinit,
        .cs_set         = stm32_w25q80_cs_set,
        .spi_write      = stm32_w25q80_spi_write,
        .spi_read       = stm32_w25q80_spi_read,
        .spi_transceive = stm32_w25q80_spi_transceive,
        .delay_us       = stm32_w25q80_delay_us,
        .delay_ms       = stm32_w25q80_delay_ms,
    };

    return &driver;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  初始化 STM32 SPI 外设和 CS GPIO
 *
 * 假设 SPI handle 已由 CubeMX 生成并完成底层初始化。
 * 此处调用 HAL_SPI_Init() 完成参数配置。
 *
 * @return 0: 成功, -1: HAL 初始化失败
 */
static int stm32_w25q80_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    g_spi_handle = W25Q80_STM32_SPI_HANDLE;

    if (HAL_SPI_Init(g_spi_handle) != HAL_OK)
    {
        return -1;
    }

    __HAL_RCC_GPIO_CLK_ENABLE(W25Q80_STM32_CS_GPIO_PORT);

    gpio_init.Pin   = W25Q80_STM32_CS_GPIO_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(W25Q80_STM32_CS_GPIO_PORT, &gpio_init);
    HAL_GPIO_WritePin(W25Q80_STM32_CS_GPIO_PORT, W25Q80_STM32_CS_GPIO_PIN, GPIO_PIN_SET);

    g_initialized = true;
    return 0;
}

/**
 * @brief  释放 STM32 SPI 外设资源
 *
 * @return 0: 成功
 */
static int stm32_w25q80_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    HAL_SPI_DeInit(g_spi_handle);
    HAL_GPIO_WritePin(W25Q80_STM32_CS_GPIO_PORT, W25Q80_STM32_CS_GPIO_PIN, GPIO_PIN_RESET);
    g_initialized = false;
    return 0;
}

/**
 * @brief  控制 CS 引脚电平
 *
 * @param  level  0: 拉低（选中）, 1: 拉高（释放）
 */
static void stm32_w25q80_cs_set(uint8_t level)
{
    HAL_GPIO_WritePin(W25Q80_STM32_CS_GPIO_PORT, W25Q80_STM32_CS_GPIO_PIN,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  SPI 只写传输（忽略 MISO）
 *
 * @param  data  发送数据缓冲区
 * @param  len   发送字节数
 * @return 0: 成功, -1: HAL 传输失败
 */
static int stm32_w25q80_spi_write(const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef ret;

    ret = HAL_SPI_Transmit(g_spi_handle, (uint8_t *)data, len, W25Q80_STM32_TIMEOUT_MS);
    return (ret == HAL_OK) ? 0 : -1;
}

/**
 * @brief  SPI 只读传输（MOSI 自动发 0xFF）
 *
 * @param  data  接收数据缓冲区
 * @param  len   接收字节数
 * @return 0: 成功, -1: HAL 传输失败
 */
static int stm32_w25q80_spi_read(uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef ret;

    ret = HAL_SPI_Receive(g_spi_handle, data, len, W25Q80_STM32_TIMEOUT_MS);
    return (ret == HAL_OK) ? 0 : -1;
}

/**
 * @brief  SPI 全双工传输（同时发送和接收）
 *
 * @param  tx   发送数据缓冲区
 * @param  rx   接收数据缓冲区
 * @param  len  传输字节数
 * @return 0: 成功, -1: HAL 传输失败
 */
static int stm32_w25q80_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_StatusTypeDef ret;

    ret = HAL_SPI_TransmitReceive(g_spi_handle, (uint8_t *)tx, rx, len, W25Q80_STM32_TIMEOUT_MS);
    return (ret == HAL_OK) ? 0 : -1;
}

/**
 * @brief  微秒延时（基于 DWT cycle counter）
 *
 * @param  us  微秒数
 */
static void stm32_w25q80_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}

/**
 * @brief  毫秒延时
 *
 * @param  ms  毫秒数
 */
static void stm32_w25q80_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

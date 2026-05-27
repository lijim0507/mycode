/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "swi2c_port.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_rom_sys.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* 默认 I2C 标准模式 (100kHz) 半周期：5us */
#define SWI2C_DEFAULT_DELAY_US  5

/* 默认 GPIO 引脚 */
#define SWI2C_DEFAULT_SCL_PIN   GPIO_NUM_22
#define SWI2C_DEFAULT_SDA_PIN   GPIO_NUM_21

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  SWI2C GPIO 位带驱动上下文
 */
typedef struct {
    gpio_num_t scl_pin;
    gpio_num_t sda_pin;
    uint32_t   delay_us;
} swi2c_port_ctx_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int swi2c_port_init(void *config);
static int swi2c_port_deinit(void);
static void swi2c_port_sda_set(uint8_t level);
static void swi2c_port_scl_set(uint8_t level);
static uint8_t swi2c_port_sda_get(void);
static void swi2c_port_delay_us(void);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static swi2c_port_ctx_t g_swi2c_ctx;

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  SWI2C GPIO 位带硬件初始化
 * @param  config 指向 swi2c_port_cfg_t 的指针，传 NULL 使用默认值
 * @return 0: 成功, -1: GPIO 配置失败
 */
static int swi2c_port_init(void *config)
{
    swi2c_port_cfg_t *cfg = (swi2c_port_cfg_t *)config;

    if (cfg != NULL)
    {
        g_swi2c_ctx.scl_pin  = (gpio_num_t)cfg->scl_pin;
        g_swi2c_ctx.sda_pin  = (gpio_num_t)cfg->sda_pin;
        g_swi2c_ctx.delay_us = cfg->delay_us;
    }
    else
    {
        g_swi2c_ctx.scl_pin  = SWI2C_DEFAULT_SCL_PIN;
        g_swi2c_ctx.sda_pin  = SWI2C_DEFAULT_SDA_PIN;
        g_swi2c_ctx.delay_us = SWI2C_DEFAULT_DELAY_US;
    }

    /*
     * I2C 总线要求开漏输出 + 上拉电阻。
     * 内部上拉约 45kohm，长线或高速场景建议外加 2.2k~4.7kohm 上拉。
     */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << g_swi2c_ctx.scl_pin) | (1ULL << g_swi2c_ctx.sda_pin),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        return -1;
    }

    /* 空闲状态：SCL 和 SDA 均置高 */
    gpio_set_level(g_swi2c_ctx.scl_pin, 1);
    gpio_set_level(g_swi2c_ctx.sda_pin, 1);

    return 0;
}

/**
 * @brief  反初始化，释放 GPIO 资源
 * @return 0: 成功
 */
static int swi2c_port_deinit(void)
{
    gpio_reset_pin(g_swi2c_ctx.scl_pin);
    gpio_reset_pin(g_swi2c_ctx.sda_pin);
    memset(&g_swi2c_ctx, 0, sizeof(g_swi2c_ctx));
    return 0;
}

/**
 * @brief  设置 SDA 电平
 * @param  level 0: 低电平, 非零: 高电平（开漏释放）
 */
static void swi2c_port_sda_set(uint8_t level)
{
    gpio_set_level(g_swi2c_ctx.sda_pin, level ? 1 : 0);
}

/**
 * @brief  设置 SCL 电平
 * @param  level 0: 低电平, 非零: 高电平
 */
static void swi2c_port_scl_set(uint8_t level)
{
    gpio_set_level(g_swi2c_ctx.scl_pin, level ? 1 : 0);
}

/**
 * @brief  读取 SDA 电平
 * @return 0: 低电平, 1: 高电平
 */
static uint8_t swi2c_port_sda_get(void)
{
    return (uint8_t)gpio_get_level(g_swi2c_ctx.sda_pin);
}

/**
 * @brief  微秒级延时（I2C 半周期）
 * @note   使用 ESP32 ROM 延时函数，精度约 1us，无需 FreeRTOS
 */
static void swi2c_port_delay_us(void)
{
    esp_rom_delay_us(g_swi2c_ctx.delay_us);
}

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  获取 SWI2C GPIO 位带驱动实例
 * @return swi2c_driver_t 结构体指针
 */
const swi2c_driver_t *swi2c_port_get_driver(void)
{
    static const swi2c_driver_t driver = {
        .init      = swi2c_port_init,
        .deinit    = swi2c_port_deinit,
        .sda_set   = swi2c_port_sda_set,
        .scl_set   = swi2c_port_scl_set,
        .sda_get   = swi2c_port_sda_get,
        .delay_us  = swi2c_port_delay_us,
    };
    return &driver;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

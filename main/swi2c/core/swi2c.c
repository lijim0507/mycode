/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "swi2c.h"
#include "swi2c_port.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static void    swi2c_send_bit(uint8_t bit);
static uint8_t swi2c_receive_bit(void);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const swi2c_driver_t *g_driver;
static bool                   g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 SWI2C 驱动库
 * @param  driver   平台驱动指针
 * @param  port_cfg 平台相关配置，传 NULL 使用默认值
 * @return 0: 成功, -1: 参数错误, -2: 硬件初始化失败
 */
int swi2c_init(const swi2c_driver_t *driver, void *port_cfg)
{
    if (!driver || !driver->init || !driver->deinit
        || !driver->sda_set || !driver->scl_set || !driver->sda_get
        || !driver->delay_us)
    {
        return -1;
    }

    if (g_initialized)
    {
        swi2c_deinit();
    }

    if (driver->init(port_cfg) != 0)
    {
        return -2;
    }

    g_driver      = driver;
    g_initialized = true;
    return 0;
}

/**
 * @brief  反初始化 SWI2C 驱动库
 * @return 0: 成功
 */
int swi2c_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    if (g_driver && g_driver->deinit)
    {
        g_driver->deinit();
    }

    g_driver      = NULL;
    g_initialized = false;
    return 0;
}

/**
 * @brief  发送 I2C 起始信号
 * @return 0: 成功, -1: 未初始化
 */
int swi2c_start(void)
{
    if (!g_initialized)
    {
        return -1;
    }

    /* SDA 高->低，SCL 保持高 */
    g_driver->sda_set(1);
    g_driver->scl_set(1);
    g_driver->delay_us();
    g_driver->sda_set(0);
    g_driver->delay_us();
    g_driver->scl_set(0);
    return 0;
}

/**
 * @brief  发送 I2C 停止信号
 * @return 0: 成功, -1: 未初始化
 */
int swi2c_stop(void)
{
    if (!g_initialized)
    {
        return -1;
    }

    /* SDA 低->高，SCL 保持高 */
    g_driver->sda_set(0);
    g_driver->scl_set(1);
    g_driver->delay_us();
    g_driver->sda_set(1);
    g_driver->delay_us();
    return 0;
}

/**
 * @brief  发送一个字节（MSB 优先）
 * @param  byte 要发送的字节
 * @return 0: 成功, -1: 未初始化
 */
int swi2c_send_byte(uint8_t byte)
{
    if (!g_initialized)
    {
        return -1;
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        swi2c_send_bit(byte & (0x80 >> i));
    }
    return 0;
}

/**
 * @brief  接收一个字节（MSB 优先）
 * @return 接收到的字节
 */
uint8_t swi2c_receive_byte(void)
{
    uint8_t byte = 0;

    if (!g_initialized)
    {
        return 0;
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        if (swi2c_receive_bit())
        {
            byte |= (0x80 >> i);
        }
    }

    return byte;
}

/**
 * @brief  发送应答位
 * @param  bit 0: ACK, 1: NACK
 * @return 0: 成功, -1: 未初始化
 */
int swi2c_send_ack(uint8_t bit)
{
    if (!g_initialized)
    {
        return -1;
    }

    swi2c_send_bit(bit);
    return 0;
}

/**
 * @brief  接收应答位
 * @return 0: ACK, 1: NACK
 */
uint8_t swi2c_receive_ack(void)
{
    if (!g_initialized)
    {
        return 1;
    }

    return swi2c_receive_bit();
}

/**
 * @brief  获取 I2C 传输接口实例
 * @return i2c_transport_t 结构体指针
 * @note   将 core 层的公共函数封装为传输接口，供上层模块依赖注入
 */
const i2c_transport_t *swi2c_get_transport(void)
{
    static const i2c_transport_t transport = {
        .start        = swi2c_start,
        .stop         = swi2c_stop,
        .send_byte    = swi2c_send_byte,
        .receive_byte = swi2c_receive_byte,
        .send_ack     = swi2c_send_ack,
        .receive_ack  = swi2c_receive_ack,
    };
    return &transport;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  发送单个数据位
 * @param  bit 0 或 1（非零值均视为 1）
 * @note   标准 I2C 时序：先置 SDA，SCL 产生上升沿锁存数据
 */
static void swi2c_send_bit(uint8_t bit)
{
    g_driver->scl_set(0);
    if (bit > 0)
    {
        g_driver->sda_set(1);
    }
    else
    {
        g_driver->sda_set(0);
    }
    g_driver->delay_us();
    g_driver->scl_set(1);
    g_driver->delay_us();
    g_driver->scl_set(0);
    g_driver->sda_set(0);
}

/**
 * @brief  接收单个数据位
 * @return 0 或 1
 * @note   释放 SDA 后由从机驱动数据线，SCL 上升沿后读取
 */
static uint8_t swi2c_receive_bit(void)
{
    uint8_t bit;

    g_driver->sda_set(1);
    g_driver->scl_set(0);
    g_driver->delay_us();
    g_driver->scl_set(1);
    bit = g_driver->sda_get();
    g_driver->delay_us();
    g_driver->scl_set(0);

    return bit;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

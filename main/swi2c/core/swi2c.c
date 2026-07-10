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

static int  swi2c_start(void);
static int  swi2c_stop(void);
static int  swi2c_send_byte(uint8_t byte);
static uint8_t swi2c_receive_byte(void);
static int  swi2c_send_ack(uint8_t bit);
static uint8_t swi2c_receive_ack(void);

static int swi2c_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int swi2c_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
static int swi2c_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms);


/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const swi2c_driver_t *g_driver;
static bool                   g_initialized;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/


/**
 * @brief  获取软件 I2C 的事务级传输接口实例
 * @return i2c_transport_t 结构体指针
 */
const i2c_transport_t *swi2c_get_transport(void)
{
    static const i2c_transport_t transport = {
        .init       = swi2c_init,
        .deinit     = swi2c_deinit,
        .write      = swi2c_i2c_write,
        .read       = swi2c_i2c_read,
        .write_read = swi2c_i2c_write_read,
    };
    return &transport;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/


/**
 * @brief  初始化 SWI2C 驱动库
 * @return 0: 成功, -1: 参数错误, -2: 硬件初始化失败
 */
static int swi2c_init(void)
{

    g_driver = swi2c_port_get_driver();
    
    if (!g_driver || !g_driver->init || !g_driver->deinit
        || !g_driver->sda_set || !g_driver->scl_set || !g_driver->sda_get
        || !g_driver->delay_us)
    {
        g_driver = NULL;
        return -1;
    }

    if (g_initialized)
    {
        swi2c_deinit();
    }

    if (g_driver->init() != 0)
    {
        return -2;
    }
    g_initialized = true;
    return 0;
}

/**
 * @brief  反初始化 SWI2C 驱动库
 * @return 0: 成功
 */
static int swi2c_deinit(void)
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

/* --- 信号级 --- */

static int swi2c_start(void)
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

static int swi2c_stop(void)
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

/* --- 位级 --- */

static void swi2c_send_bit(uint8_t bit)
{
    g_driver->scl_set(0);
    g_driver->sda_set(bit ? 1 : 0);
    g_driver->delay_us();
    g_driver->scl_set(1);
    g_driver->delay_us();
    g_driver->scl_set(0);
    g_driver->sda_set(1);
}

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

/* --- 字节级 --- */

static int swi2c_send_byte(uint8_t byte)
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

static uint8_t swi2c_receive_byte(void)
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

/* --- 应答 --- */

static int swi2c_send_ack(uint8_t bit)
{
    if (!g_initialized)
    {
        return -1;
    }

    swi2c_send_bit(bit);
    return 0;
}

static uint8_t swi2c_receive_ack(void)
{
    if (!g_initialized)
    {
        return 1;
    }

    return swi2c_receive_bit();
}

/* --- 事务级 --- */

/**
 * @brief  事务级 I2C write
 * @note   START → dev_addr(W) → [send byte → check ACK] × N → STOP
 */
static int swi2c_i2c_write(uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (swi2c_start() != 0)
    {
        return -1;
    }

    if (swi2c_send_byte((uint8_t)(dev_addr << 1)) != 0
        || swi2c_receive_ack() != 0)
    {
        swi2c_stop();
        return -3;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        if (swi2c_send_byte(data[i]) != 0
            || swi2c_receive_ack() != 0)
        {
            swi2c_stop();
            return -3;
        }
    }

    swi2c_stop();
    return 0;
}

/**
 * @brief  事务级 I2C read
 * @note   START → dev_addr(R) → [receive byte → send ACK] × (N-1)
 *         → receive last byte → send NACK → STOP
 */
static int swi2c_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (swi2c_start() != 0)
    {
        return -1;
    }

    if (swi2c_send_byte((uint8_t)((dev_addr << 1) | 0x01)) != 0
        || swi2c_receive_ack() != 0)
    {
        swi2c_stop();
        return -3;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = swi2c_receive_byte();
        /* ACK for all bytes except the last, NACK for the last */
        uint8_t ack = (i < (uint16_t)(len - 1)) ? 0 : 1;
        swi2c_send_ack(ack);
    }

    swi2c_stop();
    return 0;
}

/**
 * @brief  事务级 I2C write + repeated start + read
 * @note   START → dev_addr(W) → write → START(r) → dev_addr(R) → read → STOP
 */
static int swi2c_i2c_write_read(uint8_t dev_addr, const uint8_t *wr_data, uint16_t wr_len,
                                 uint8_t *rd_data, uint16_t rd_len, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (swi2c_start() != 0)
    {
        return -1;
    }

    /* --- write phase --- */
    if (swi2c_send_byte((uint8_t)(dev_addr << 1)) != 0
        || swi2c_receive_ack() != 0)
    {
        swi2c_stop();
        return -3;
    }

    for (uint16_t i = 0; i < wr_len; i++)
    {
        if (swi2c_send_byte(wr_data[i]) != 0
            || swi2c_receive_ack() != 0)
        {
            swi2c_stop();
            return -3;
        }
    }

    /* --- repeated start + read phase --- */
    if (swi2c_start() != 0)
    {
        swi2c_stop();
        return -1;
    }

    if (swi2c_send_byte((uint8_t)((dev_addr << 1) | 0x01)) != 0
        || swi2c_receive_ack() != 0)
    {
        swi2c_stop();
        return -3;
    }

    for (uint16_t i = 0; i < rd_len; i++)
    {
        rd_data[i] = swi2c_receive_byte();
        uint8_t ack = (i < (uint16_t)(rd_len - 1)) ? 0 : 1;
        swi2c_send_ack(ack);
    }

    swi2c_stop();
    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

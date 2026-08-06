/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "w25q80.h"
#include "w25q80_port.h"

#include <stdbool.h>

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

/* --- Opcodes --- */
#define W25Q80_CMD_WREN              0x06
#define W25Q80_CMD_WRDI              0x04
#define W25Q80_CMD_RDSR1             0x05
#define W25Q80_CMD_RDSR2             0x35
#define W25Q80_CMD_RDSR3             0x15
#define W25Q80_CMD_WRSR              0x01
#define W25Q80_CMD_READ              0x03
#define W25Q80_CMD_FAST_READ         0x0B
#define W25Q80_CMD_PAGE_PROGRAM      0x02
#define W25Q80_CMD_SECTOR_ERASE      0x20
#define W25Q80_CMD_BLOCK32_ERASE     0x52
#define W25Q80_CMD_BLOCK64_ERASE     0xD8
#define W25Q80_CMD_CHIP_ERASE        0xC7
#define W25Q80_CMD_RDID              0x9F
#define W25Q80_CMD_READ_UID          0x4B
#define W25Q80_CMD_POWER_DOWN        0xB9
#define W25Q80_CMD_RELEASE_PD        0xAB

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/

static int  validate_address_range(uint32_t address, uint32_t size);
static int  check_initialized(void);
static void send_3byte_address(uint32_t address);
static int  wait_busy(void);
static int  do_erase(uint8_t opcode, uint32_t address, uint32_t align_mask);
static int  do_simple_cmd(uint8_t opcode);
static int  do_read_with_address(uint8_t opcode, uint32_t address,
                                 uint8_t *data, uint32_t size, uint8_t dummy_bytes);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static const w25q80_port_driver_t *g_driver;
static bool                         g_initialized;
static w25q80_config_t              g_config;

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 W25Q80 模块
 *
 * @param  config  模块配置指针（可为 NULL 使用默认值）
 * @return 0: 成功, -1: 参数错误, -4: port driver 无效
 */
int w25q80_init(const w25q80_config_t *config)
{
    const w25q80_port_driver_t *driver;
    int                          ret;

    driver = w25q80_port_get_driver();
    if (!driver)
    {
        return -4;
    }

    if (!driver->init || !driver->deinit ||
        !driver->cs_set || !driver->spi_write ||
        !driver->spi_read || !driver->spi_transceive ||
        !driver->delay_us || !driver->delay_ms)
    {
        return -4;
    }

    ret = driver->init();
    if (ret != 0)
    {
        return -4;
    }

    if (config)
    {
        g_config = *config;
    }
    else
    {
        g_config.read_timeout_ms = W25Q80_DEFAULT_TIMEOUT_MS;
    }

    g_driver      = driver;
    g_initialized = true;
    return 0;
}

/**
 * @brief  反初始化 W25Q80 模块
 *
 * @return 0: 成功
 */
int w25q80_deinit(void)
{
    if (!g_initialized)
    {
        return 0;
    }

    g_driver->deinit();
    g_driver      = NULL;
    g_initialized = false;
    return 0;
}

/**
 * @brief  读取 JEDEC 制造商 / 器件 ID
 *
 * @param  info  输出的芯片信息结构体指针
 * @return 0: 成功, -1: 参数错误, -2: 未初始化
 */
int w25q80_read_jedec_id(w25q80_info_t *info)
{
    int ret;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    if (!info)
    {
        return -1;
    }

    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_RDID}, 1);
    g_driver->spi_read(&info->manufacturer_id, 1);
    g_driver->spi_read(&info->memory_type, 1);
    g_driver->spi_read(&info->capacity, 1);
    g_driver->cs_set(1);

    return 0;
}

/**
 * @brief  读取 64-bit 唯一 ID
 *
 * @param  uid  输出缓冲区（至少 8 字节）
 * @return 0: 成功, -1: 参数错误, -2: 未初始化
 */
int w25q80_read_unique_id(uint8_t *uid)
{
    int      ret;
    uint8_t  dummy[4];

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    if (!uid)
    {
        return -1;
    }

    /* Read Unique ID: opcode + 4 dummy bytes, then 8 bytes of UID */
    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_READ_UID}, 1);
    g_driver->delay_us(1);
    g_driver->spi_read(dummy, 4);       /* discard 4 dummy bytes */
    g_driver->spi_read(uid, 8);         /* read actual UID */
    g_driver->cs_set(1);

    return 0;
}

/**
 * @brief  从 Flash 读取数据（普通读，opcode 0x03）
 *
 * @param  address  起始地址
 * @param  data     输出缓冲区
 * @param  size     读取字节数
 * @return 0: 成功, -1: 参数错误/地址越界, -2: 未初始化
 */
int w25q80_read(uint32_t address, uint8_t *data, uint32_t size)
{
    return do_read_with_address(W25Q80_CMD_READ, address, data, size, 0);
}

/**
 * @brief  从 Flash 快速读取数据（opcode 0x0B）
 *
 * @param  address  起始地址
 * @param  data     输出缓冲区
 * @param  size     读取字节数
 * @return 0: 成功, -1: 参数错误/地址越界, -2: 未初始化
 */
int w25q80_fast_read(uint32_t address, uint8_t *data, uint32_t size)
{
    return do_read_with_address(W25Q80_CMD_FAST_READ, address, data, size, 1);
}

/**
 * @brief  向 Flash 写入数据（自动 WREN + 跨页拆分 + 轮询 BUSY）
 *
 * @param  address  起始地址
 * @param  data     写入数据
 * @param  size     写入字节数
 * @return 0: 成功, -1: 参数错误/地址越界, -2: 未初始化, -3: 写入超时
 */
int w25q80_write(uint32_t address, const uint8_t *data, uint32_t size)
{
    uint32_t remaining;
    uint32_t offset;
    uint32_t chunk;
    int      ret;

    ret = validate_address_range(address, size);
    if (ret != 0)
    {
        return ret;
    }

    if (!data || size == 0)
    {
        return -1;
    }

    remaining = size;
    offset    = 0;

    while (remaining > 0)
    {
        chunk = W25Q80_PAGE_SIZE - (address & (W25Q80_PAGE_SIZE - 1));
        if (chunk > remaining)
        {
            chunk = remaining;
        }

        w25q80_write_enable();

        g_driver->cs_set(0);
        g_driver->spi_write((const uint8_t[]){W25Q80_CMD_PAGE_PROGRAM}, 1);
        send_3byte_address(address);
        g_driver->spi_write(&data[offset], (uint16_t)chunk);
        g_driver->cs_set(1);

        ret = wait_busy();
        if (ret != 0)
        {
            return ret;
        }

        address   += chunk;
        offset    += chunk;
        remaining -= chunk;
    }

    return 0;
}

/**
 * @brief  擦除 4KB 扇区
 *
 * @param  address  扇区首地址（必须 4KB 对齐）
 * @return 0: 成功, -1: 地址未对齐, -2: 未初始化, -3: 超时
 */
int w25q80_sector_erase(uint32_t address)
{
    return do_erase(W25Q80_CMD_SECTOR_ERASE, address, W25Q80_SECTOR_SIZE - 1);
}

/**
 * @brief  擦除 32KB 块
 *
 * @param  address  块首地址（必须 32KB 对齐）
 * @return 0: 成功, -1: 地址未对齐, -2: 未初始化, -3: 超时
 */
int w25q80_block32_erase(uint32_t address)
{
    return do_erase(W25Q80_CMD_BLOCK32_ERASE, address, W25Q80_BLOCK32_SIZE - 1);
}

/**
 * @brief  擦除 64KB 块
 *
 * @param  address  块首地址（必须 64KB 对齐）
 * @return 0: 成功, -1: 地址未对齐, -2: 未初始化, -3: 超时
 */
int w25q80_block64_erase(uint32_t address)
{
    return do_erase(W25Q80_CMD_BLOCK64_ERASE, address, W25Q80_BLOCK64_SIZE - 1);
}

/**
 * @brief  整片擦除（全 1MB 恢复为 0xFF）
 *
 * @return 0: 成功, -2: 未初始化, -3: 超时
 */
int w25q80_chip_erase(void)
{
    int ret;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    w25q80_write_enable();

    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_CHIP_ERASE}, 1);
    g_driver->cs_set(1);

    return wait_busy();
}

/**
 * @brief  读取全部三个状态寄存器
 *
 * @param  status  输出的状态寄存器结构体指针
 * @return 0: 成功, -1: 参数错误, -2: 未初始化
 */
int w25q80_read_status(w25q80_status_t *status)
{
    int ret;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    if (!status)
    {
        return -1;
    }

    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_RDSR1}, 1);
    g_driver->spi_read(&status->sr1, 1);
    g_driver->cs_set(1);

    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_RDSR2}, 1);
    g_driver->spi_read(&status->sr2, 1);
    g_driver->cs_set(1);

    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_RDSR3}, 1);
    g_driver->spi_read(&status->sr3, 1);
    g_driver->cs_set(1);

    return 0;
}

/**
 * @brief  写入状态寄存器（三个字节一次性写入，内部自动 WREN + 轮询 BUSY）
 *
 * @param  sr1  SR1 值
 * @param  sr2  SR2 值
 * @param  sr3  SR3 值
 * @return 0: 成功, -2: 未初始化, -3: 超时
 */
int w25q80_write_status(uint8_t sr1, uint8_t sr2, uint8_t sr3)
{
    int ret;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    w25q80_write_enable();

    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_WRSR}, 1);
    g_driver->spi_write(&sr1, 1);
    g_driver->spi_write(&sr2, 1);
    g_driver->spi_write(&sr3, 1);
    g_driver->cs_set(1);

    return wait_busy();
}

/**
 * @brief  发送 Write Enable 命令（0x06）
 *
 * @return 0: 成功, -2: 未初始化
 */
int w25q80_write_enable(void)
{
    return do_simple_cmd(W25Q80_CMD_WREN);
}

/**
 * @brief  发送 Write Disable 命令（0x04）
 *
 * @return 0: 成功, -2: 未初始化
 */
int w25q80_write_disable(void)
{
    return do_simple_cmd(W25Q80_CMD_WRDI);
}

/**
 * @brief  查询芯片忙状态
 *
 * @return 0: 空闲, 1: 忙, -2: 未初始化
 */
int w25q80_is_busy(void)
{
    int     ret;
    uint8_t sr1;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    g_driver->cs_set(0);
    g_driver->spi_write((const uint8_t[]){W25Q80_CMD_RDSR1}, 1);
    g_driver->spi_read(&sr1, 1);
    g_driver->cs_set(1);

    return (sr1 & W25Q80_SR1_BUSY) ? 1 : 0;
}

/**
 * @brief  进入掉电模式（电流降至 ~1µA）
 *
 * @return 0: 成功, -2: 未初始化
 */
int w25q80_power_down(void)
{
    return do_simple_cmd(W25Q80_CMD_POWER_DOWN);
}

/**
 * @brief  退出掉电模式（唤醒后自动等待 3µs）
 *
 * @return 0: 成功, -2: 未初始化
 */
int w25q80_release_power_down(void)
{
    int ret;

    ret = do_simple_cmd(W25Q80_CMD_RELEASE_PD);
    if (ret != 0)
    {
        return ret;
    }

    g_driver->delay_us(3);  /* tRES1: 3 µs wake-up time */
    return 0;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  校验模块是否已初始化
 *
 * @return 0: 已初始化, -2: 未初始化
 */
static int check_initialized(void)
{
    if (!g_initialized)
    {
        return -2;
    }

    return 0;
}

/**
 * @brief  校验地址范围是否在 Flash 容量内
 *
 * @param  address  起始地址
 * @param  size     操作长度
 * @return 0: 合法, -1: 越界, -2: 未初始化
 */
static int validate_address_range(uint32_t address, uint32_t size)
{
    int ret;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    if (size == 0 || address + size > W25Q80_CAPACITY)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief  发送 3 字节地址（MSB 优先）
 *
 * @param  address  24-bit 地址
 */
static void send_3byte_address(uint32_t address)
{
    uint8_t addr_buf[3];

    addr_buf[0] = (uint8_t)(address >> 16);
    addr_buf[1] = (uint8_t)(address >> 8);
    addr_buf[2] = (uint8_t)(address);

    g_driver->spi_write(addr_buf, 3);
}

/**
 * @brief  轮询 BUSY 位直到擦/写完成或超时
 *
 * @return 0: 完成, -3: 超时
 */
static int wait_busy(void)
{
    uint32_t elapsed;
    uint8_t  sr1;

    elapsed = 0;

    do
    {
        g_driver->cs_set(0);
        g_driver->spi_write((const uint8_t[]){W25Q80_CMD_RDSR1}, 1);
        g_driver->spi_read(&sr1, 1);
        g_driver->cs_set(1);

        if (!(sr1 & W25Q80_SR1_BUSY))
        {
            return 0;
        }

        g_driver->delay_ms(1);
        elapsed++;
    }
    while (elapsed < g_config.read_timeout_ms);

    return -3;
}

/**
 * @brief  通用擦除操作（sector / block32 / block64）
 *
 * @param  opcode      擦除命令 opcode
 * @param  address     擦除起始地址
 * @param  align_mask  对齐掩码（例如 4095 表示 4KB 对齐）
 * @return 0: 成功, -1: 地址未对齐, -2: 未初始化, -3: 超时
 */
static int do_erase(uint8_t opcode, uint32_t address, uint32_t align_mask)
{
    int ret;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    if (address & align_mask)
    {
        return -1;
    }

    w25q80_write_enable();

    g_driver->cs_set(0);
    g_driver->spi_write(&opcode, 1);
    send_3byte_address(address);
    g_driver->cs_set(1);

    return wait_busy();
}

/**
 * @brief  发送单字节命令（WREN / WRDI / Power-down / Release PD）
 *
 * @param  opcode  命令 opcode
 * @return 0: 成功, -2: 未初始化
 */
static int do_simple_cmd(uint8_t opcode)
{
    int ret;

    ret = check_initialized();
    if (ret != 0)
    {
        return ret;
    }

    g_driver->cs_set(0);
    g_driver->spi_write(&opcode, 1);
    g_driver->cs_set(1);

    return 0;
}

/**
 * @brief  通用读操作（支持普通读和 Fast Read）
 *
 * @param  opcode       读命令 opcode (0x03 或 0x0B)
 * @param  address      起始地址
 * @param  data         输出缓冲区
 * @param  size         读取字节数
 * @param  dummy_bytes  dummy 字节数（普通读 = 0, Fast Read = 1）
 * @return 0: 成功, -1: 参数错误/地址越界, -2: 未初始化
 */
static int do_read_with_address(uint8_t opcode, uint32_t address,
                                 uint8_t *data, uint32_t size, uint8_t dummy_bytes)
{
    int ret;

    ret = validate_address_range(address, size);
    if (ret != 0)
    {
        return ret;
    }

    if (!data)
    {
        return -1;
    }

    g_driver->cs_set(0);
    g_driver->spi_write(&opcode, 1);
    send_3byte_address(address);

    if (dummy_bytes > 0)
    {
        uint8_t dummy;
        g_driver->spi_read(&dummy, dummy_bytes); /* discard dummy bytes */
    }

    g_driver->spi_read(data, (uint16_t)size);
    g_driver->cs_set(1);

    return 0;
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

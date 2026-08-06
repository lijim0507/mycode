

#ifndef __W25Q80_H_
#define __W25Q80_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/

#define W25Q80_PAGE_SIZE            256
#define W25Q80_SECTOR_SIZE          4096
#define W25Q80_BLOCK32_SIZE         32768
#define W25Q80_BLOCK64_SIZE         65536
#define W25Q80_CAPACITY             1048576     /* 8 Mbit = 1 MB */

#define W25Q80_DEFAULT_TIMEOUT_MS   1000

/* --- Status Register 1 bit masks --- */
#define W25Q80_SR1_BUSY             0x01
#define W25Q80_SR1_WEL              0x02
#define W25Q80_SR1_BP0              0x04
#define W25Q80_SR1_BP1              0x08
#define W25Q80_SR1_BP2              0x10
#define W25Q80_SR1_TB               0x20
#define W25Q80_SR1_SEC              0x40
#define W25Q80_SR1_SRP0             0x80

/* --- Status Register 2 bit masks --- */
#define W25Q80_SR2_SRP1             0x01
#define W25Q80_SR2_QUAD_ENABLE      0x02
#define W25Q80_SR2_LB1              0x08
#define W25Q80_SR2_LB2              0x10
#define W25Q80_SR2_LB3              0x20
#define W25Q80_SR2_CMP              0x40
#define W25Q80_SR2_SUS              0x80

/* --- Status Register 3 bit masks --- */
#define W25Q80_SR3_WPS              0x04
#define W25Q80_SR3_DRV0             0x40
#define W25Q80_SR3_DRV1             0x80

/* --- JEDEC ID expected values --- */
#define W25Q80_JEDEC_MANUFACTURER   0xEF
#define W25Q80_JEDEC_MEMORY_TYPE    0x13
#define W25Q80_JEDEC_CAPACITY       0x14

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/**
 * @brief  W25Q80 芯片识别信息
 */
typedef struct
{
    uint8_t  manufacturer_id;
    uint8_t  memory_type;
    uint8_t  capacity;
    uint8_t  unique_id[8];
} w25q80_info_t;

/**
 * @brief  W25Q80 模块配置
 */
typedef struct
{
    uint32_t read_timeout_ms;
} w25q80_config_t;

/**
 * @brief  W25Q80 状态寄存器组（SR1 + SR2 + SR3）
 */
typedef struct
{
    uint8_t sr1;
    uint8_t sr2;
    uint8_t sr3;
} w25q80_status_t;

/****************************************************************************/
/*                          Exported Variables                              */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  初始化 W25Q80 模块
 *
 * 内部调用 w25q80_port_get_driver() 获取平台 SPI 驱动并校验完整性。
 * config 为 NULL 时使用默认值（read_timeout_ms = 1000）。
 *
 * @param  config  模块配置指针（可为 NULL）
 * @return 0: 成功, -1: 参数错误, -4: port driver 无效
 */
int  w25q80_init(const w25q80_config_t *config);

/**
 * @brief  反初始化 W25Q80 模块
 *
 * 调用 port driver 的 deinit 释放 SPI 硬件资源。
 *
 * @return 0: 成功
 */
int  w25q80_deinit(void);

/**
 * @brief  读取 JEDEC 制造商 / 器件 ID
 *
 * @param  info  输出的芯片信息结构体指针
 * @return 0: 成功, -1: 参数错误, -2: 未初始化
 */
int  w25q80_read_jedec_id(w25q80_info_t *info);

/**
 * @brief  读取 64-bit 唯一 ID
 *
 * @param  uid  输出缓冲区（至少 8 字节）
 * @return 0: 成功, -1: 参数错误, -2: 未初始化
 */
int  w25q80_read_unique_id(uint8_t *uid);

/**
 * @brief  从 Flash 读取数据（普通读，opcode 0x03，最高 40 MHz）
 *
 * @param  address  起始地址
 * @param  data     输出缓冲区
 * @param  size     读取字节数
 * @return 0: 成功, -1: 参数错误/地址越界, -2: 未初始化
 */
int  w25q80_read(uint32_t address, uint8_t *data, uint32_t size);

/**
 * @brief  从 Flash 快速读取数据（opcode 0x0B，1 dummy byte，最高 80 MHz）
 *
 * @param  address  起始地址
 * @param  data     输出缓冲区
 * @param  size     读取字节数
 * @return 0: 成功, -1: 参数错误/地址越界, -2: 未初始化
 */
int  w25q80_fast_read(uint32_t address, uint8_t *data, uint32_t size);

/**
 * @brief  向 Flash 写入数据（自动 WREN + 跨页拆分 + 轮询 BUSY）
 *
 * 内部按 256 字节页边界自动拆分，每页自动执行：
 *   WREN → Page Program (0x02) → 轮询 BUSY
 *
 * @param  address  起始地址
 * @param  data     写入数据
 * @param  size     写入字节数
 * @return 0: 成功, -1: 参数错误/地址越界, -2: 未初始化, -3: 写入超时
 */
int  w25q80_write(uint32_t address, const uint8_t *data, uint32_t size);

/**
 * @brief  擦除 4KB 扇区
 *
 * @param  address  扇区首地址（必须 4KB 对齐）
 * @return 0: 成功, -1: 地址未对齐, -2: 未初始化, -3: 超时
 */
int  w25q80_sector_erase(uint32_t address);

/**
 * @brief  擦除 32KB 块
 *
 * @param  address  块首地址（必须 32KB 对齐）
 * @return 0: 成功, -1: 地址未对齐, -2: 未初始化, -3: 超时
 */
int  w25q80_block32_erase(uint32_t address);

/**
 * @brief  擦除 64KB 块
 *
 * @param  address  块首地址（必须 64KB 对齐）
 * @return 0: 成功, -1: 地址未对齐, -2: 未初始化, -3: 超时
 */
int  w25q80_block64_erase(uint32_t address);

/**
 * @brief  整片擦除（全 1MB 恢复为 0xFF）
 *
 * @return 0: 成功, -2: 未初始化, -3: 超时
 */
int  w25q80_chip_erase(void);

/**
 * @brief  读取全部三个状态寄存器
 *
 * @param  status  输出的状态寄存器结构体指针
 * @return 0: 成功, -1: 参数错误, -2: 未初始化
 */
int  w25q80_read_status(w25q80_status_t *status);

/**
 * @brief  写入状态寄存器（三个字节一次性写入，内部自动 WREN + 轮询 BUSY）
 *
 * @param  sr1  SR1 值
 * @param  sr2  SR2 值
 * @param  sr3  SR3 值
 * @return 0: 成功, -2: 未初始化, -3: 超时
 */
int  w25q80_write_status(uint8_t sr1, uint8_t sr2, uint8_t sr3);

/**
 * @brief  发送 Write Enable 命令（0x06）
 *
 * @return 0: 成功, -2: 未初始化
 */
int  w25q80_write_enable(void);

/**
 * @brief  发送 Write Disable 命令（0x04）
 *
 * @return 0: 成功, -2: 未初始化
 */
int  w25q80_write_disable(void);

/**
 * @brief  查询芯片忙状态
 *
 * @return 0: 空闲, 1: 忙, -2: 未初始化
 */
int  w25q80_is_busy(void);

/**
 * @brief  进入掉电模式（电流降至 ~1µA）
 *
 * @return 0: 成功, -2: 未初始化
 */
int  w25q80_power_down(void);

/**
 * @brief  退出掉电模式（唤醒后自动等待 3µs）
 *
 * @return 0: 成功, -2: 未初始化
 */
int  w25q80_release_power_down(void);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

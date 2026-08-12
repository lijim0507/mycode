#ifndef __UDS_PORT_H_
#define __UDS_PORT_H_

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

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Exported Functions                                  */
/****************************************************************************/

/**
 * @brief 查询指定地址的扇区大小（对齐粒度）
 *
 * @param addr Flash 地址
 * @return uint32_t 扇区大小（字节）
 */
uint32_t uds_port_flash_get_sector_size(uint32_t addr);

/**
 * @brief 擦除指定范围的 Flash（同步阻塞直到完成）
 *
 * @param addr  起始地址
 * @param size  擦除大小（字节）
 * @return int  0 成功，负值失败
 */
int      uds_port_flash_erase(uint32_t addr, uint32_t size);

/**
 * @brief 写入数据到 Flash（实现层负责内部 RMW 对齐）
 *
 * @param addr  起始地址
 * @param data  数据指针
 * @param len   数据长度（字节）
 * @return int  0 成功，负值失败
 */
int      uds_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);

/**
 * @brief 校验已写入数据的 CRC32
 *
 * @param addr     起始地址
 * @param size     校验范围（字节）
 * @param out_crc  输出 CRC32 值
 * @return int     0 校验通过，负值失败
 */
int      uds_port_flash_verify(uint32_t addr, uint32_t size, uint32_t *out_crc);

/**
 * @brief ECU 复位（HardReset）
 */
void uds_port_ecu_reset(void);

/**
 * @brief 通信控制（启用/禁用指定类型的通信）
 *
 * @param sub_func  控制类型 0x00=EnableAll 0x01=EnableRxDisableTx 0x03=DisableAll
 * @param comm_type 通信类型 0x01=Normal 0x02=NetworkMgmt 0x03=Both
 */
void uds_port_comm_control(uint8_t sub_func, uint8_t comm_type);

/**
 * @brief DTC 设置控制
 *
 * @param on_off 0x01=On 0x02=Off
 */
void uds_port_dtc_control(uint8_t on_off);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

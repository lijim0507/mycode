#ifndef __ISOTP_H_
#define __ISOTP_H_

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "isotp_config.h"
#include "isotp_defines.h"

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct isotp_port_driver
{
    int      (*init)(void);
    int      (*send)(uint32_t id, const uint8_t *data, uint8_t len);
    int      (*receive)(uint32_t *id, uint8_t *data, uint8_t *len);
    int      (*deinit)(void);
    uint32_t (*get_ms)(void);
    void     (*debug)(const char *message, ...);
} isotp_port_driver_t;

typedef void (*isotp_recv_cb_t)(uint8_t *data, uint16_t len);

typedef struct
{
    /* sender parameters */
    uint32_t send_arbitration_id;
    uint32_t send_current_id;
    /* message buffer */
    uint8_t* send_buffer;
    uint16_t send_buf_size;
    uint16_t send_size;
    uint16_t send_offset;
    /* multi-frame flags */
    uint8_t send_sn;
    uint16_t send_bs_remain;
    uint8_t send_st_min;
    uint8_t send_wtf_count;
    uint32_t send_timer_st;
    uint32_t send_timer_bs;
    int send_protocol_result;
    uint8_t send_status;

    /* receiver parameters */
    uint32_t receive_arbitration_id;
    /* message buffer */
    uint8_t* receive_buffer;
    uint16_t receive_buf_size;
    uint16_t receive_size;
    uint16_t receive_offset;
    /* multi-frame control */
    uint8_t receive_sn;
    uint8_t receive_bs_count;
    uint8_t receive_block_size;
    uint8_t receive_st_min;
    uint32_t receive_timer_cr;
    int receive_protocol_result;
    uint8_t receive_status;

    /* receive callback (NULL for polling mode) */
    isotp_recv_cb_t recv_cb;
} isotp_handle_t;

/****************************************************************************/
/*						Exported Functions								*/
/****************************************************************************/

/**
 * @brief 使用默认 port 驱动初始化 ISO-TP 模块
 *
 * @return int ISOTP_RET_OK 成功，ISOTP_RET_ERROR 失败
 */
int  isotp_init(void);

/**
 * @brief 使用指定 port 驱动初始化 ISO-TP 模块，若之前已初始化则先反初始化
 *
 * @param driver port 驱动指针，send 和 get_ms 不可为 NULL
 * @return int ISOTP_RET_OK 成功，ISOTP_RET_ERROR 失败
 */
int  isotp_init_with_driver(const isotp_port_driver_t *driver);

/**
 * @brief 反初始化 ISO-TP 模块，释放 port 层资源
 *
 * @return int ISOTP_RET_OK
 */
int  isotp_deinit(void);

/**
 * @brief 初始化 ISO-TP 句柄，设置收发 CAN ID 并分配内部缓冲区
 *
 * @param handle ISO-TP 句柄指针
 * @param recvid 接收滤波 CAN 仲裁 ID
 * @param sendid 默认发送 CAN 仲裁 ID
 */
void isotp_init_handle(isotp_handle_t *handle, uint32_t recvid, uint32_t sendid);

/**
 * @brief 注册接收完成回调函数，设为 NULL 则切换为轮询模式
 *
 * @param handle ISO-TP 句柄指针
 * @param cb 回调函数指针，接收完成时被调用
 */
void isotp_register_recv_cb(isotp_handle_t *handle, isotp_recv_cb_t cb);

/**
 * @brief 轮询处理 ISO-TP 接收与发送状态机，需在主循环中周期调用
 *
 * @param handle ISO-TP 句柄指针
 */
void isotp_poll(isotp_handle_t *handle);

/**
 * @brief 使用句柄中默认 CAN ID 发送 ISO-TP 消息
 *
 * @param handle ISO-TP 句柄指针
 * @param payload 发送数据
 * @param size 数据长度
 * @return int ISOTP_RET_OK 成功，ISOTP_RET_ERROR 失败，ISOTP_RET_INPROGRESS 正在发送中，ISOTP_RET_OVERFLOW 数据过长
 */
int  isotp_send(isotp_handle_t *handle, const uint8_t payload[], uint16_t size);

/**
 * @brief 使用指定 CAN ID 发送 ISO-TP 消息，若当前有发送正在进行则返回 ISOTP_RET_INPROGRESS
 *
 * @param handle ISO-TP 句柄指针
 * @param id 目标 CAN 仲裁 ID
 * @param payload 发送数据
 * @param size 数据长度
 * @return int ISOTP_RET_OK 成功，ISOTP_RET_ERROR 失败，ISOTP_RET_INPROGRESS 正在发送中，ISOTP_RET_OVERFLOW 数据过长
 */
int  isotp_send_with_id(isotp_handle_t *handle, uint32_t id, const uint8_t payload[], uint16_t size);

/**
 * @brief 读取已接收完成的 ISO-TP 消息（轮询模式）
 *
 * @param handle ISO-TP 句柄指针
 * @param payload 接收缓冲区
 * @param payload_size 缓冲区大小
 * @param out_size 实际接收到的数据长度
 * @return int ISOTP_RET_OK 成功，ISOTP_RET_NO_DATA 无数据，ISOTP_RET_OVERFLOW 数据超出缓冲区，ISOTP_RET_ERROR 参数错误
 */
int  isotp_read(isotp_handle_t *handle, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size);


#ifdef __cplusplus
}
#endif

#endif
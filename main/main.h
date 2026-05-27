
#ifndef __MAIN_H_
#define __MAIN_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/
typedef enum
{
    SEMAPHORE_OP_NULL = 0,
    SEMAPHORE_OP_GIVE = 1,
    SEMAPHORE_OP_TAKE = 2
} semaphore_operation_t;

/**
 * @brief 信号量 ID 枚举
 */
typedef enum {
    SEMAPHORE_ID_NULL = 0,
    SEMAPHORE_ID_IMU,
    SEMAPHORE_ID_CAN,
    SEMAPHORE_ID_UDISK,
    SEMAPHORE_ID_CONTROL,
    SEMAPHORE_ID_STATUS,
    SEMAPHORE_ID_SLEEP,
    SEMAPHORE_ID_COUNT
} semaphore_id_t;

/**
 * @brief 队列操作枚举 (与 semaphore_operate 风格统一)
 */
typedef enum {
    QUEUE_OP_NULL      = 0,
    QUEUE_OP_SEND      = 1,
    QUEUE_OP_RECEIVE   = 2,
    QUEUE_OP_SEND_FROM_ISR = 3,
    QUEUE_OP_RECEIVE_FROM_ISR = 4,
    QUEUE_OP_RESET     = 5
} queue_operation_t;

/**
 * @brief 队列 ID 枚举 (与 semaphore_id_t 同模块一一对照，按需扩展)
 */
typedef enum {
    QUEUE_ID_NULL    = 0,

    /* 与信号量同名模块 */
    QUEUE_ID_IMU,           /* 传感器数据队列          */
    QUEUE_ID_CAN,           /* CAN 报文队列            */
    QUEUE_ID_UDISK,         /* U 盘通信队列            */
    QUEUE_ID_CONTROL,       /* 控制指令队列            */
    QUEUE_ID_STATUS,        /* 状态上报队列            */
    QUEUE_ID_SLEEP,         /* 休眠管理队列            */

    /* 队列专属模块 */
    QUEUE_ID_BLE,           /* 蓝牙数据透传队列        */
    QUEUE_ID_KEY,           /* 按键事件队列            */
    QUEUE_ID_FOC,           /* 电机控制指令队列        */
    QUEUE_ID_EEPROM,        /* EEPROM 读写请求队列     */
    QUEUE_ID_SWI2C,         /* SWI2C 总线事务队列      */
    QUEUE_ID_UDS,           /* UDS 诊断请求/响应队列   */
    QUEUE_ID_WS2812,        /* 灯效指令队列            */
    QUEUE_ID_LOG,           /* 日志输出队列            */
    QUEUE_ID_CLI,           /* CLI 命令队列            */

    QUEUE_ID_COUNT
} queue_id_t;
/****************************************************************************/
/*							Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*                      Exproted Functions                                   */
/****************************************************************************/

int8_t semaphore_operate(semaphore_id_t id, semaphore_operation_t op, uint32_t timeout);
int8_t queue_operate(queue_id_t id, queue_operation_t op, void *item, uint32_t timeout);

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
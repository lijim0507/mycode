#ifndef __SYS_MANAGER_H_
#define __SYS_MANAGER_H_
/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/
typedef uint32_t sys_error_mask_t;

/**
 * @brief 产品错误定义
 * @note 每个错误占一位，最多 32 个，按实际产品增删。
 */
typedef enum
{
    SYS_ERR_NONE        = 0x0000,
    SYS_ERR_OVERCURRENT = 0x0001,
    SYS_ERR_OVERTEMP    = 0x0002,
    SYS_ERR_COMM_LOST   = 0x0004,
    SYS_ERR_SENSOR      = 0x0008,
} sys_error_t;


typedef enum
{
    SYS_STATE_INITIALIZING = 0,         // 初始化
    SYS_STATE_WORKING = 1,              // 工作
    SYS_STATE_SHUTDOWN = 2,             // 关机
    SYS_STATE_WARNING = 3,              // 警告
    SYS_STATE_ERROR = 4,                // 错误
} sys_state_t;


/**
 * @brief 错误处理函数
 *
 * @param error 触发的错误位
 * @param detail 错误附加信息
 */
typedef void (*sys_manager_error_handler_t)(sys_error_t error, int8_t detail);

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief 上报错误
 *
 * @param error 单个错误位
 * @param detail 该错误的附加信息
 * @note 一次上报一个错误；多个错误可分别调用，内部掩码会同时保留
 * @note 已置位的错误不会重复触发 handler，先 clear 再上报可重新触发
 * @note handler 在调用者上下文同步执行；模块不加锁
 */
void sys_manager_report_error(sys_error_t error, int8_t detail);

/**
 * @brief 清除错误位
 *
 * @param error 要清除的错误位或组合
 */
void sys_manager_clear_error(sys_error_mask_t error);

/**
 * @brief 清除全部错误位
 */
void sys_manager_clear_all_errors(void);

/**
 * @brief 获取当前错误位
 *
 * @return 当前错误掩码
 */
sys_error_mask_t sys_manager_get_error_mask(void);

/**
 * @brief 查询错误位是否置位
 *
 * @param error 错误位或组合，任意一位匹配返回 true
 * @return true 置位，false 未置位
 */
bool sys_manager_has_error(sys_error_mask_t error);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

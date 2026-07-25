/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "main.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/
void app_main(void)
{
    printf("Hello world!\n");
}



//----------------------------------------------------------------------------
// 二值信号量句柄--demo
osSemaphoreId_t demo_semaphore_handle;

/**
 * @brief 统一信号量操作接口
 * @param id      信号量ID
 * @param op      操作类型 (GIVE/TAKE)
 * @param timeout TAKE 时最大等待节拍数 (GIVE 时忽略)
 * @return 0=成功, -1=无效ID, -2=无效操作, -3=操作失败
 */
int8_t semaphore_operate(semaphore_id_t id, semaphore_operation_t op, uint32_t timeout)
{
    osSemaphoreId_t handle = NULL;

    switch (id)
    {
    case SEMAPHORE_ID_IMU:     handle = IMUBinarySemHandle;        break;
    case SEMAPHORE_ID_CAN:     handle = CANBinarySemHandle;        break;
    case SEMAPHORE_ID_UDISK:   handle = UDISKBinarySemHandle;      break;
    case SEMAPHORE_ID_CONTROL: handle = CONTROLBinarySemHandle;    break;
    case SEMAPHORE_ID_STATUS:  handle = STATUSBinarySemHandle;     break;
    case SEMAPHORE_ID_SLEEP:   handle = SleepCtrlBinarySemHandle;  break;
    default:
        return -1;
    }

    osStatus_t rc;
    switch (op)
    {
    case SEMAPHORE_OP_GIVE:
        rc = osSemaphoreRelease(handle);
        break;
    case SEMAPHORE_OP_TAKE:
        timeout = (timeout != 0U) ? timeout : portMAX_DELAY;
        rc = osSemaphoreAcquire(handle, timeout);
        break;
    default:
        return -2;
    }

    if (rc != osOK)
    {
        return -3;
    }
    return 0;
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// 队列句柄--demo (按需注册)
// QueueHandle_t demo_queue_handle;

/**
 * @brief 统一队列操作接口
 * @param id      队列 ID
 * @param op      操作类型 (SEND/RECEIVE/RESET 等)
 * @param item    发送时指向数据，接收时指向接收缓冲区
 * @param timeout 等待节拍数 (0 = 不等待, portMAX_DELAY = 无限等待)
 * @return 0=成功, -1=无效ID, -2=无效操作, -3=无效句柄, -4=操作失败
 */
int8_t queue_operate(queue_id_t id, queue_operation_t op, void *item, uint32_t timeout)
{
    QueueHandle_t handle = NULL;

    switch (id)
    {
    /* 与 semaphore_operate 同名模块对照 */
    // case QUEUE_ID_IMU:     handle = imu_queue_handle;          break;
    // case QUEUE_ID_CAN:     handle = can_queue_handle;          break;
    // case QUEUE_ID_UDISK:   handle = udisk_queue_handle;        break;
    // case QUEUE_ID_CONTROL: handle = control_queue_handle;      break;
    // case QUEUE_ID_STATUS:  handle = status_queue_handle;       break;
    // case QUEUE_ID_SLEEP:   handle = sleep_queue_handle;        break;

    default:
        return -1;
    }

    if (handle == NULL)
    {
        return -3;
    }

    BaseType_t rc;
    switch (op)
    {
    case QUEUE_OP_SEND:
        rc = xQueueSend(handle, item, (TickType_t)timeout);
        break;

    case QUEUE_OP_RECEIVE:
        rc = xQueueReceive(handle, item, (TickType_t)timeout);
        break;

    case QUEUE_OP_SEND_FROM_ISR:
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            rc = xQueueSendFromISR(handle, item, &xHigherPriorityTaskWoken);
            if (rc == pdPASS && xHigherPriorityTaskWoken == pdTRUE) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
        break;

    case QUEUE_OP_RECEIVE_FROM_ISR:
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            rc = xQueueReceiveFromISR(handle, item, &xHigherPriorityTaskWoken);
            if (rc == pdPASS && xHigherPriorityTaskWoken == pdTRUE) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
        break;

    case QUEUE_OP_RESET:
        rc = xQueueReset(handle);
        break;

    default:
        return -2;
    }

    if (rc != pdPASS)
    {
        return -4;
    }
    return 0;
}
//----------------------------------------------------------------------------

/* CPU 占用率 + 任务状态 (由 app_calc_cpu_usage() 每 100ms 刷新) */
#define TOTAL_TASK_NUMBER   16

typedef struct _task_status_t
{
    char   name[configMAX_TASK_NAME_LEN];  /* 任务名 */
    float  cpu_usage;                      /* CPU 占用率 (%) */
    eTaskState state;                      /* 任务状态 */
    UBaseType_t priority;                  /* 当前优先级 */
    uint16_t stack_watermark;              /* 栈高水位 (字) */
} task_status_t;

static task_status_t g_task_status[TOTAL_TASK_NUMBER];
static UBaseType_t   g_task_count;  /* 当前有效任务数（供外部读取 g_task_status 边界） */

/** @brief 上一轮快照：关联 task_number 与 run_time */
typedef struct {
    UBaseType_t task_number;
    uint32_t    run_time;
} prev_snapshot_t;

/* 初始化RUN TIME的TIMER */
__weak void configureTimerForRunTimeStats(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* 获取RUN TIME */
__weak unsigned long getRunTimeCounterValue(void)
{
    return DWT->CYCCNT;
}


void calc_cpu_usage(void)
{
    static uint32_t prev_run_time[16] = {0};
    static uint32_t prev_total_time = 0;
    static UBaseType_t prev_task_count = 0;
    static UBaseType_t prev_task_numbers[16] = {0};

    TaskStatus_t arr[16];
    uint32_t total;
    UBaseType_t count = uxTaskGetSystemState(arr, 16, &total);

    /* 按 xTaskNumber 排序 */
    for (UBaseType_t i = 0; i + 1 < count; i++)
    {
        for (UBaseType_t j = i + 1; j < count; j++)
        {
            if (arr[i].xTaskNumber > arr[j].xTaskNumber)
            {
                TaskStatus_t t = arr[i];
                arr[i] = arr[j];
                arr[j] = t;
            }
        }
    }

    /* 首次调用仅保存快照，不计算 cpu_usage */
    uint32_t delta_total = (prev_task_count > 0U) ? (total - prev_total_time) : 0U;

    for (UBaseType_t i = 0; i < count; i++)
    {
        task_status_t *p = &g_task_status[i];

        strncpy(p->name, arr[i].pcTaskName, configMAX_TASK_NAME_LEN - 1);
        p->name[configMAX_TASK_NAME_LEN - 1] = '\0';

        p->state   = arr[i].eCurrentState;
        p->priority = arr[i].uxCurrentPriority;
        p->stack_watermark = arr[i].usStackHighWaterMark;

        /* 按 xTaskNumber 匹配上一轮数据，计算增量 */
        uint32_t prev_val = 0U;
        uint32_t task_num = arr[i].xTaskNumber;
        for (UBaseType_t k = 0; k < prev_task_count; k++)
        {
            if (prev_task_numbers[k] == task_num)
            {
                prev_val = prev_run_time[k];
                break;
            }
        }

        if (delta_total > 0U)
        {
            uint32_t delta_task = arr[i].ulRunTimeCounter - prev_val;
            p->cpu_usage = (float)delta_task * 100.0f / (float)delta_total;
        }
        else
        {
            p->cpu_usage = 0.0f;
        }
    }

    /* 保存本轮快照 */
    prev_total_time = total;
    prev_task_count = count;
    for (UBaseType_t i = 0; i < count; i++)
    {
        prev_run_time[i] = arr[i].ulRunTimeCounter;
        prev_task_numbers[i] = arr[i].xTaskNumber;
    }
}


/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/


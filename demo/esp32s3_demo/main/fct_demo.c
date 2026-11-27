/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "fct_demo.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "fct.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/
#define TAG                     "FCT_DEMO"

#define FCT_DEMO_TASK_STACK     4096        /* 模拟测试任务栈大小 */
#define FCT_DEMO_TASK_PRIO      5           /* 模拟测试任务优先级 */

#define FCT_DEMO_HB_PERIOD_MS   2000        /* 心跳周期，与协议约定一致 */
#define FCT_DEMO_ROUND_PERIOD_MS 10000      /* 两轮模拟测试之间的空闲时间 */
#define FCT_DEMO_LOG_GAP_MS     300         /* 模拟测试中两条日志之间的间隔 */
#define FCT_DEMO_NG_CODE        12U         /* 模拟 NG 时上报的错误码，含义由现场定义 */

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/
static void fct_demo_run_round(uint32_t round);
static void fct_demo_task(void *param);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  启动 FCT 演示
 *
 * @return 0: 成功, -1: fct_init 失败, -2: 任务创建失败
 */
int fct_demo_start(void)
{
    int rc;

    rc = fct_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "fct_init failed: %d", rc);
        return -1;
    }

    if (xTaskCreate(fct_demo_task, "fct_demo", FCT_DEMO_TASK_STACK, NULL,
                    FCT_DEMO_TASK_PRIO, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "create fct demo task failed");
        fct_deinit();
        return -2;
    }

    return 0;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  模拟一轮 FCT 测试，奇偶轮交替输出 OK 和 NG
 *
 * @param  round 轮次，从 1 开始
 */
static void fct_demo_run_round(uint32_t round)
{
    bool ng = ((round % 2U) == 0U);
    int  ret;

    ret = fct_start();
    if (ret != 0)
    {
        ESP_LOGE(TAG, "fct_start failed: %d", ret);
        return;
    }

    fct_logf(FCT_LOG_INFO, "round %lu: power on self test start",
             (unsigned long)round);
    vTaskDelay(pdMS_TO_TICKS(FCT_DEMO_LOG_GAP_MS));

    fct_logf(FCT_LOG_INFO, "measure 3.3V = 3.31");
    vTaskDelay(pdMS_TO_TICKS(FCT_DEMO_LOG_GAP_MS));

    fct_logf(FCT_LOG_DEBUG, "ambient temperature = 26.5 C");
    vTaskDelay(pdMS_TO_TICKS(FCT_DEMO_LOG_GAP_MS));

    if (ng)
    {
        fct_logf(FCT_LOG_WARN, "measure 5V = 4.90, near limit");
        vTaskDelay(pdMS_TO_TICKS(FCT_DEMO_LOG_GAP_MS));

        fct_logf(FCT_LOG_ERROR, "measure 5V = 4.62, out of range");
    }
    else
    {
        fct_logf(FCT_LOG_INFO, "measure 5V = 5.02");
    }
    vTaskDelay(pdMS_TO_TICKS(FCT_DEMO_LOG_GAP_MS));

    ret = fct_result(ng ? FCT_RESULT_NG : FCT_RESULT_OK,
                     ng ? FCT_DEMO_NG_CODE : FCT_ERROR_CODE_NONE);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "fct_result failed: %d", ret);
    }

    ret = fct_end();
    if (ret != 0)
    {
        ESP_LOGE(TAG, "fct_end failed: %d", ret);
    }

    ESP_LOGI(TAG, "round %lu done: %s", (unsigned long)round,
             ng ? "NG" : "OK");
}

/**
 * @brief  FCT 模拟测试任务
 *
 * 空闲时每 FCT_DEMO_HB_PERIOD_MS 发送一条 IDLE 心跳，每 FCT_DEMO_ROUND_PERIOD_MS
 * 执行一轮模拟测试，用于验证上位机的接收、CSV 记录和界面显示。
 *
 * @param  param 未使用
 */
static void fct_demo_task(void *param)
{
    uint32_t elapsed_ms = 0;
    uint32_t round = 0;

    (void)param;

    for (;;)
    {
        /* 心跳每 2 秒一条，测试期间由模块自动切换为 TESTING */
        fct_hb();
        vTaskDelay(pdMS_TO_TICKS(FCT_DEMO_HB_PERIOD_MS));
        elapsed_ms += FCT_DEMO_HB_PERIOD_MS;

        if (elapsed_ms < FCT_DEMO_ROUND_PERIOD_MS)
        {
            continue;
        }

        elapsed_ms = 0;
        round++;
        fct_demo_run_round(round);
    }
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

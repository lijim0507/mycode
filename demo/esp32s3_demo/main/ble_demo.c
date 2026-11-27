/****************************************************************************/
/*                              Includes                                    */
/****************************************************************************/
#include "ble_demo.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "ble.h"
#include "ble_log.h"

/****************************************************************************/
/*                              Macros                                      */
/****************************************************************************/
#define TAG "BLE_DEMO"

#define BLE_DEMO_TASK_STACK         4096
#define BLE_DEMO_TASK_PRIO          5
#define BLE_DEMO_QUEUE_LEN          8
#define BLE_DEMO_POLL_MS            200
#define BLE_DEMO_LOG_PERIOD_MS      2000
#define BLE_DEMO_IND_PERIOD_MS      5000

/* 回环服务：128 位自定义 UUID */
#define BLE_DEMO_ECHO_SVC_UUID \
    BLE_UUID128_BYTES(0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, \
                      0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF)
#define BLE_DEMO_ECHO_CHR_UUID \
    BLE_UUID128_BYTES(0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, \
                      0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF)
#define BLE_DEMO_IND_CHR_UUID \
    BLE_UUID128_BYTES(0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, \
                      0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF)

/* 设备信息服务：16 位标准 UUID，用于验证 16 位路径 */
#define BLE_DEMO_MODEL_SVC_UUID     BLE_UUID16_BYTES(0x180A)
#define BLE_DEMO_MODEL_CHR_UUID     BLE_UUID16_BYTES(0x2A24)

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/

typedef struct
{
    uint16_t len;
    uint8_t  data[BLE_CHR_WRITE_MAX];
} ble_demo_msg_t;

/****************************************************************************/
/*                      Prototypes Of Local Functions                       */
/****************************************************************************/
static int  ble_demo_read_model(uint16_t conn_handle, uint16_t attr_handle,
                                uint8_t *data, uint16_t max_len,
                                uint16_t *out_len);
static int  ble_demo_write_echo(uint16_t conn_handle, uint16_t attr_handle,
                                const uint8_t *data, uint16_t len);
static void ble_demo_task(void *param);
static uint32_t ble_demo_get_time_ms(void);

/****************************************************************************/
/*                          Global Variables                                */
/****************************************************************************/

static uint16_t g_echo_val_handle;
static uint16_t g_indicate_val_handle;
static uint16_t g_model_val_handle;

static QueueHandle_t g_echo_queue;

static ble_svc_chr_t g_echo_chrs[] = {
    {
        .uuid         = BLE_DEMO_ECHO_CHR_UUID,
        .flags        = BLE_SVC_CHR_FLAG_WRITE | BLE_SVC_CHR_FLAG_WRITE_NO_RSP |
                        BLE_SVC_CHR_FLAG_NOTIFY,
        .val_handle   = &g_echo_val_handle,
        .read_cb      = NULL,
        .write_cb     = ble_demo_write_echo,
        .subscribe_cb = NULL,
    },
    {
        .uuid         = BLE_DEMO_IND_CHR_UUID,
        .flags        = BLE_SVC_CHR_FLAG_INDICATE,
        .val_handle   = &g_indicate_val_handle,
        .read_cb      = NULL,
        .write_cb     = NULL,
        .subscribe_cb = NULL,
    },
};

static const ble_svc_def_t g_echo_svc = {
    .uuid    = BLE_DEMO_ECHO_SVC_UUID,
    .chrs    = g_echo_chrs,
    .chr_num = sizeof(g_echo_chrs) / sizeof(g_echo_chrs[0]),
};

static ble_svc_chr_t g_model_chrs[] = {
    {
        .uuid         = BLE_DEMO_MODEL_CHR_UUID,
        .flags        = BLE_SVC_CHR_FLAG_READ,
        .val_handle   = &g_model_val_handle,
        .read_cb      = ble_demo_read_model,
        .write_cb     = NULL,
        .subscribe_cb = NULL,
    },
};

static const ble_svc_def_t g_model_svc = {
    .uuid    = BLE_DEMO_MODEL_SVC_UUID,
    .chrs    = g_model_chrs,
    .chr_num = sizeof(g_model_chrs) / sizeof(g_model_chrs[0]),
};

/****************************************************************************/
/*                          Exported Functions                              */
/****************************************************************************/

/**
 * @brief  启动 BLE 演示
 *
 * @return 0: 成功, -1: 队列创建失败, -2: 日志模块初始化失败,
 *         -3: 服务注册失败, -4: BLE 初始化失败, -5: 宿主任务创建失败,
 *         -6: 演示任务创建失败
 */
int ble_demo_start(void)
{
    ble_log_config_t log_cfg;
    int rc;

    g_echo_queue = xQueueCreate(BLE_DEMO_QUEUE_LEN, sizeof(ble_demo_msg_t));
    if (!g_echo_queue)
    {
        ESP_LOGE(TAG, "create echo queue failed");
        return -1;
    }

    /* 日志服务也要在 ble_init() 之前注册 */
    log_cfg.get_time_ms = ble_demo_get_time_ms;
    log_cfg.level = BLE_LOG_LEVEL_INFO;
    rc = ble_log_init(&log_cfg);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_log_init failed: %d", rc);
        return -2;
    }

    rc = ble_svc_register(&g_echo_svc);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "register echo service failed: %d", rc);
        return -3;
    }

    rc = ble_svc_register(&g_model_svc);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "register model service failed: %d", rc);
        return -3;
    }

    rc = ble_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_init failed: %d", rc);
        return -4;
    }

    /* 优先级与 ESP-IDF 官方 NimBLE 集成保持一致 */
    if (xTaskCreate(ble_task, "ble_host", BLE_HOST_TASK_STACK, NULL,
                    configMAX_PRIORITIES - 4, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "create ble host task failed");
        ble_deinit();
        return -5;
    }

    if (xTaskCreate(ble_demo_task, "ble_demo", BLE_DEMO_TASK_STACK, NULL,
                    BLE_DEMO_TASK_PRIO, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "create ble demo task failed");
        return -6;
    }

    ESP_LOGI(TAG, "BLE demo started, name=%s", BLE_DEVICE_NAME);
    return 0;
}

/****************************************************************************/
/*                          Static Functions                                */
/****************************************************************************/

/**
 * @brief  读取型号字符串
 *
 * @param  conn_handle 连接句柄
 * @param  attr_handle 特征 value handle
 * @param  data        输出缓冲区
 * @param  max_len     缓冲区容量
 * @param  out_len     实际写入长度
 * @return 0: 成功, -1: 缓冲区不足
 */
static int ble_demo_read_model(uint16_t conn_handle, uint16_t attr_handle,
                               uint8_t *data, uint16_t max_len,
                               uint16_t *out_len)
{
    static const char model[] = "ESP32S3_DEMO";
    uint16_t          len = (uint16_t)(sizeof(model) - 1);

    (void)conn_handle;
    (void)attr_handle;

    if (!data || !out_len || max_len < len)
    {
        return -1;
    }

    memcpy(data, model, len);
    *out_len = len;
    return 0;
}

/**
 * @brief  把上位机写入的数据放进队列，由演示任务回发
 *
 * @param  conn_handle 连接句柄
 * @param  attr_handle 特征 value handle
 * @param  data        写入数据
 * @param  len         写入长度
 * @return 0: 成功, -1: 队列已满
 */
static int ble_demo_write_echo(uint16_t conn_handle, uint16_t attr_handle,
                               const uint8_t *data, uint16_t len)
{
    ble_demo_msg_t msg;

    (void)conn_handle;
    (void)attr_handle;

    if (!data || len == 0 || len > sizeof(msg.data))
    {
        return -1;
    }

    msg.len = len;
    memcpy(msg.data, data, len);

    if (xQueueSend(g_echo_queue, &msg, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "echo queue full, drop %d bytes", len);
        return -1;
    }

    return 0;
}

/**
 * @brief  BLE 演示任务：回环通知、周期日志、周期指示
 *
 * @param  param 未使用
 */
static void ble_demo_task(void *param)
{
    ble_demo_msg_t msg;
    uint8_t        ind_data[4];
    uint32_t       now;
    uint32_t       last_log_ms = 0;
    uint32_t       last_ind_ms = 0;
    uint32_t       counter = 0;
    int            rc;

    (void)param;

    for (;;)
    {
        if (xQueueReceive(g_echo_queue, &msg, pdMS_TO_TICKS(BLE_DEMO_POLL_MS)) == pdTRUE)
        {
            rc = ble_notify_attr(g_echo_val_handle, msg.data, msg.len);
            if (rc != 0)
            {
                ESP_LOGD(TAG, "echo notify failed: %d", rc);
            }
        }

        now = ble_demo_get_time_ms();

        if ((now - last_log_ms) >= BLE_DEMO_LOG_PERIOD_MS)
        {
            last_log_ms = now;
            BLE_LOG_I(TAG, "echo service alive, counter=%lu",
                      (unsigned long)counter);
        }

        if ((now - last_ind_ms) >= BLE_DEMO_IND_PERIOD_MS)
        {
            last_ind_ms = now;
            counter++;

            ind_data[0] = (uint8_t)(counter & 0xFF);
            ind_data[1] = (uint8_t)((counter >> 8) & 0xFF);
            ind_data[2] = (uint8_t)((counter >> 16) & 0xFF);
            ind_data[3] = (uint8_t)((counter >> 24) & 0xFF);

            rc = ble_indicate_attr(g_indicate_val_handle, ind_data,
                                   (uint16_t)sizeof(ind_data));
            if (rc != 0 && rc != -2)
            {
                ESP_LOGD(TAG, "indicate failed: %d", rc);
            }
        }
    }
}

/**
 * @brief  获取毫秒时间戳，供 ble_log 使用
 *
 * @return 毫秒计数，约 49 天回绕一次
 */
static uint32_t ble_demo_get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/****************************************************************************/
/*                              EOF                                         */
/****************************************************************************/

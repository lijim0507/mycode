/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "common.h"
#include "drv_gap.h"
#include "drv_gatt_svc.h"

#include "app_protocol.h"
#include "app_test.h"
#include "app_rtos.h"
#include "esp_log.h"

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

static void on_stack_reset(int reason);
static void on_stack_sync(void);

static void ble_host_config_init(void) ;

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/**
 * @brief 复位回调
 * @param reason 复位原因
 */
static void on_stack_reset(int reason) 
{
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

/**
 * @brief 同步回调
 * @param void
 */
static void on_stack_sync(void) 
{
    /* On stack sync, do advertising initialization */
    gap_adv_init();
}

/**
 * @brief ble控制器初始化
 * 
 * @return uint8_t 
 */
static uint8_t ble_port_init(void)
{
    nimble_port_init();
}


/**
 * @brief 初始化 NimBLE 主机配置
 * 
 */
static void ble_host_config_init(void) 
{
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = NULL;

    /* Store host configuration */
    ble_store_config_init();
}



/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

/**
 * @brief 初始化 BLE 应用
 * 
 */
void app_ble_init(void)
{
    int rc = 0;
    esp_err_t ret = 0;
    /* Initialize BLE host and controller */
    ESP_LOGI(TAG, "Initializing BLE host and controller...");


    /* NimBLE stack initialization */
    ret = ble_port_init();
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0) 
    {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* GATT server initialization */
    rc = gatt_svc_init();
    if (rc != 0) 
    {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return;
    }

    /* NimBLE host configuration initialization */
    ble_host_config_init();

}


void app_ble_task(void *param) 
{
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}





/****************************************************************************/
/*								EOF											*/
/****************************************************************************/

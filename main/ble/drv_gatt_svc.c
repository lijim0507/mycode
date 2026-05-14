/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "drv_gatt_svc.h"
#include "common.h"

#include "app_protocol.h"
#include "app_rtos.h"
#include "app_stm32.h"
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
static int gatt_svr_access_callback(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) ;

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/* ============================== 服务 1 =================================== */
#define SVC_UUID(...)     BLE_UUID128_INIT(__VA_ARGS__)
#define CHR_UUID(...)     BLE_UUID128_INIT(__VA_ARGS__)


static const ble_uuid128_t service1_uuid =
    SVC_UUID(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10);

/* ---------- 服务 1: 特征值 1 ---------- */

static const ble_uuid128_t service1_chr1_uuid = 
    CHR_UUID(0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
              0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20);

static uint16_t service1_chr1_val_handle;

static uint8_t service1_chr1_tx_buf[64] = {0};
static uint8_t service1_chr1_rx_buf[64] = {0};
static uint16_t service1_chr1_conn_handle = 0;
static bool service1_chr1_conn_inited = false;
static bool service1_chr1_notify_enabled = false;

/* ---------- 服务 1: 特征值 2 ---------- */
static const ble_uuid128_t service1_chr2_uuid = 
    CHR_UUID(0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
              0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30);

static uint16_t service1_chr2_val_handle;

/* ============================== 服务 2 =================================== */
/**
 * 示例: 添加新服务
 * 
 * static const ble_uuid128_t service2_uuid =
 *     SVC_UUID(0x02, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
 *               0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10);
 */

/* ============================================================================
 * GATT 服务定义表
 * ============================================================================
 * 定义所有 GATT 服务和特征值
 * 末尾必须以 {0} 结束表示列表结束
 * ============================================================================
 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = 
{
    /* ============================ 服务 1 ============================ */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service1_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        {   
            /* --------------------- 特征值 1 --------------------- */
            {
                .uuid = &service1_chr1_uuid.u,
                .access_cb = gatt_svr_access_callback,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &service1_chr1_val_handle
            },
            /* --------------------- 特征值 2 --------------------- */
            {
                .uuid = &service1_chr2_uuid.u,
                .access_cb = gatt_svr_access_callback,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                .val_handle = &service1_chr2_val_handle
            },
            /* ------------------- 服务结束 --------------------- */
            {
                0
            }
        },
    },
    /* ============================ 服务 2 ============================ */
    /**
     * 示例: 添加新服务
     * {
     *     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     *     .uuid = &service2_uuid.u,
     *     .characteristics = (struct ble_gatt_chr_def[])
     *     {
     *         {
     *             .uuid = &service2_chr1_uuid.u,
     *             .access_cb = gatt_svr_access_callback,
     *             .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
     *             .val_handle = &service2_chr1_val_handle
     *         },
     *         { 0 }
     *     },
     * },
     */
    /* --------------------- 服务列表结束 --------------------- */
    {
        0,
    },
};


/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int gatt_svc_init(void) 
{
    int rc = 0;

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) 
    {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) 
    {
        return rc;
    }

    return 0;
}



/**
 * @brief 发送服务1特征值1的通知
 * @param data 数据指针
 * @param data_len 数据长度
 * @return 0成功，负值失败
 */
int send_svc1_chr1_notify(uint8_t *data, uint16_t data_len) 
{
    static struct os_mbuf *om = NULL;
    int rc = 0;

    if (data == NULL || data_len == 0 || data_len > sizeof(service1_chr1_tx_buf)) 
    {
        ESP_LOGE(TAG, "svc1 chr1 notify data invalid, len: %d", data_len);
        return -1;
    }

    if (!service1_chr1_notify_enabled || !service1_chr1_conn_inited) 
    {
        ESP_LOGE(TAG, "svc1 chr1 notify not subscribed or conn not inited");
        return -2;
    }

    memset(service1_chr1_tx_buf, 0, sizeof(service1_chr1_tx_buf));
    memcpy(service1_chr1_tx_buf, data, data_len);

    om = ble_hs_mbuf_from_flat(service1_chr1_tx_buf, data_len);   
    if (om == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate mbuf");
        return -1;
    }
    
    rc = ble_gatts_notify_custom(service1_chr1_conn_handle, service1_chr1_val_handle, om);
    if (rc != 0) 
    {
        ESP_LOGE(TAG, "svc1 chr1 notify send fail, rc: %d", rc);
        return -3;
    }

    ESP_LOGI(TAG, "svc1 chr1 notify sent, len: %d", data_len);
    return 0;
}

/**
 * @brief 发送服务1特征值2的通知
 */
int send_svc1_chr2_notify(uint8_t *data, uint16_t data_len) 
{
    static struct os_mbuf *om = NULL;
    int rc = 0;
    static uint16_t chr2_conn_handle = 0;
    static bool chr2_notify_enabled = false;

    if (data == NULL || data_len == 0) 
    {
        return -1;
    }

    if (!chr2_notify_enabled) 
    {
        return -2;
    }

    om = ble_hs_mbuf_from_flat(data, data_len);   
    if (om == NULL)
    {
        return -1;
    }
    
    rc = ble_gatts_notify_custom(chr2_conn_handle, service1_chr2_val_handle, om);
    return rc;
}


void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) 
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) 
    {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(TAG, "registered service %s with handle=%d",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(TAG, "registering characteristic %s with def_handle=%d val_handle=%d",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
            break;

        default:
            assert(0);
            break;
    }
}

void gatt_svr_subscribe_cb(struct ble_gap_event *event) 
{
    uint16_t conn_handle = event->subscribe.conn_handle;

    ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d notify=%d",
             conn_handle, event->subscribe.attr_handle, event->subscribe.cur_notify);

    /* --------------------- 服务1特征值1 --------------------- */
    if (event->subscribe.attr_handle == service1_chr1_val_handle) 
    {
        service1_chr1_conn_handle = conn_handle;
        service1_chr1_conn_inited = (conn_handle != BLE_HS_CONN_HANDLE_NONE);
        service1_chr1_notify_enabled = event->subscribe.cur_notify; 
        ESP_LOGI(TAG, "svc1 chr1 notify %s", service1_chr1_notify_enabled ? "enabled" : "disabled");
    }

    /* --------------------- 服务1特征值2 --------------------- */
    if (event->subscribe.attr_handle == service1_chr2_val_handle) 
    {
        ESP_LOGI(TAG, "svc1 chr2 subscribed");
    }
}


static int gatt_svr_access_callback(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) 
{
    int rc = 0;
    uint16_t data_len = 0;
    uint8_t recv_buf[64] = {0};

    switch (ctxt->op) 
    {
        case BLE_GATT_ACCESS_OP_READ_CHR:
        {
            /* --------------------- 服务1特征值1 --------------------- */
            if (attr_handle == service1_chr1_val_handle) 
            {
                ESP_LOGI(TAG, "svc1 chr1 read request");
                rc = os_mbuf_appendfromcopy(ctxt->om, 0, service1_chr1_rx_buf, sizeof(service1_chr1_rx_buf));
            }
            /* --------------------- 服务1特征值2 --------------------- */
            else if (attr_handle == service1_chr2_val_handle) 
            {
                ESP_LOGI(TAG, "svc1 chr2 read request");
            }
            break;
        }

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
        {
            data_len = os_mbuf_len(ctxt->om);
            if (data_len == 0 || data_len > sizeof(recv_buf)) 
            {   
                ESP_LOGE(TAG, "write data len invalid: %d", data_len);
                goto error;
            }

            rc = os_mbuf_copydata(ctxt->om, 0, data_len, recv_buf);
            if (rc != 0) 
            {
                ESP_LOGE(TAG, "os_mbuf_copydata fail, rc=%d", rc);
                goto error;
            }

            /* --------------------- 服务1特征值1 --------------------- */
            if (attr_handle == service1_chr1_val_handle) 
            {
                ESP_LOGI(TAG, "svc1 chr1 write, len: %d", data_len);
                memcpy(service1_chr1_rx_buf, recv_buf, data_len);
                uart_write_bytes(APP_STM32_UART_NUM, recv_buf, data_len);
                app_protocol_parse(service1_parse_handle, recv_buf, data_len);
            }
            /* --------------------- 服务1特征值2 --------------------- */
            else if (attr_handle == service1_chr2_val_handle) 
            {
                ESP_LOGI(TAG, "svc1 chr2 write, len: %d", data_len);
            }
            break;
        }

        case BLE_GATT_ACCESS_OP_READ_DSC:
        case BLE_GATT_ACCESS_OP_WRITE_DSC:
        {
            break;
        }

        default:
        {
            goto error;
        }
    }
    return 0;

error:
    ESP_LOGE(TAG, "unexpected access operation, attr_handle=%d opcode=%d", attr_handle, ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
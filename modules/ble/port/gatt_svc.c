/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gatt_svc.h"
#include "common.h"
#include "heart_rate.h"
#include "app_protocol.h"
#include "app_rtos.h"
#include "my_uart.h"
/* Private function declarations */
static int heart_rate_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int protocol_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) ;
/* Private variables */

/* 心跳 服务 *///---------------------------------------------------
static const ble_uuid16_t heart_rate_svc_uuid = BLE_UUID16_INIT(0x180D);

/* 心跳 服务 特征值：心跳*/
static const ble_uuid16_t heart_rate_chr_uuid = BLE_UUID16_INIT(0x2A37);
static uint16_t heart_rate_chr_val_handle;

static uint8_t heart_rate_chr_val[2] = {0};
static uint16_t heart_rate_chr_conn_handle = 0; 
static bool heart_rate_chr_conn_handle_inited = false;
static bool heart_rate_ind_status = false;
//----------------------------------------------------------------------

/*protocol 服务 *///---------------------------------------------------
static const ble_uuid128_t protocol_svc_uuid =
    BLE_UUID128_INIT(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10);

/*protocol 服务 特征值：协议*/
static const ble_uuid128_t protocol_chr_uuid = 
    BLE_UUID128_INIT(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10);
static uint16_t protocol_chr_val_handle;


static uint8_t protocol_rsp_buf[64] = {0};  // 回传数据缓冲区（和上位机下发缓冲区一致）
static uint16_t protocol_chr_conn_handle = 0; // protocol特征值的连接句柄
static bool protocol_chr_conn_inited = false;  // 连接句柄初始化标志
static bool protocol_notify_status = false;    // protocol通知使能状态（上位机是否订阅）

//----------------------------------------------------------------------

/* GATT 服务初始化 table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = 
{
    /* Heart rate service */
    {   .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &heart_rate_svc_uuid.u,
        .characteristics =(struct ble_gatt_chr_def[])
        {
            {/* Heart rate characteristic */
             .uuid = &heart_rate_chr_uuid.u,
             .access_cb = heart_rate_chr_access,
             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
             .val_handle = &heart_rate_chr_val_handle
            },

            {
                0, /* No more characteristics in this service. */
            }
        }
    },

    /* protocol service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &protocol_svc_uuid.u,
        .characteristics =(struct ble_gatt_chr_def[])
        {/* LED characteristic */
            {
                .uuid = &protocol_chr_uuid.u,
                .access_cb = protocol_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &protocol_chr_val_handle
            },
            {
                0
            }
        },
    },

    {
        0, /* No more services. */
    },
};

app_protocol_handle_t protocol_parse_handle = NULL;


/* Private functions */
static int heart_rate_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) 
{
    /* Local variables */
    int rc = 0;

    /* Handle access events */
    /* Note: Heart rate characteristic is read only */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        /* Verify attribute handle */
        if (attr_handle == heart_rate_chr_val_handle) {
            /* Update access buffer value */
            heart_rate_chr_val[1] = get_heart_rate();
            rc = os_mbuf_append(ctxt->om, &heart_rate_chr_val,
                                sizeof(heart_rate_chr_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to heart rate characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}



static int protocol_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) 
{
    /* Local variables */
    int rc = 0;
    uint16_t data_len = 0;
    // 定义缓冲区（根据实际需求调整大小，示例设64字节）
    uint8_t recv_buf[64] = {0};

    /* Handle access events */
    switch (ctxt->op) 
    {
        // 仅处理上位机的写入事件
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
        {
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE) 
            {
                ESP_LOGI(TAG, "protocol characteristic write; conn_handle=%d attr_handle=%d",
                         conn_handle, attr_handle);
            } else 
            {
                ESP_LOGI(TAG, "protocol characteristic write by nimble stack; attr_handle=%d",
                         attr_handle);
            }

            /*get write data length*/
            data_len = os_mbuf_len(ctxt->om);
            /*verify write data length*/
            if (data_len == 0 || data_len > sizeof(recv_buf)) 
            {   
                /*error happened*/
                ESP_LOGE(TAG, "protocol write data len invalid: %d", data_len);
                goto error;
            }

            rc = os_mbuf_copydata(ctxt->om, 0, data_len, recv_buf);
            if (rc != 0) 
            {
                ESP_LOGE(TAG, "os_mbuf_copydata fail, rc=%d", rc);
                goto error;
            }
                


            /*verify attribute handle*/
            if (attr_handle == protocol_chr_val_handle) 
            {

                ESP_LOGI(TAG, "protocol write data len: %d", data_len);

                /*push data to RTOS queue for further processing*/
                uart_write_bytes(MY_UART_NUM, recv_buf, data_len);
                if (app_protocol_parse(protocol_parse_handle, recv_buf, data_len) != 0)
                {


                    ESP_LOGI(TAG, "Protocol data pushed to RTOS queue successfully");
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to push protocol data to RTOS queue");
                    goto error;
                }


            }
            else
            {
                goto error;
            }

            break;
        }
        case BLE_GATT_ACCESS_OP_READ_CHR:
        {

            break;
        }
        // 未知操作，返回错误
        default:
            goto error;
            break;
    }
    return 0;

error:
    ESP_LOGE(TAG, "unexpected access operation to protocol characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}


/* Public functions */
void send_heart_rate_indication(void) 
{
    if (heart_rate_ind_status && heart_rate_chr_conn_handle_inited) 
    {
        ble_gatts_indicate(heart_rate_chr_conn_handle,heart_rate_chr_val_handle);

        ESP_LOGI(TAG, "heart rate indication sent!");
    }
}


int send_protocol_notify(uint8_t *data, uint16_t data_len) 
{
    static struct os_mbuf *om = NULL;
    int rc = 0;
    // 1. 入参合法性检查
    if (data == NULL || data_len == 0 || data_len > sizeof(protocol_rsp_buf)) 
    {
        ESP_LOGE(TAG, "protocol notify data invalid, len: %d", data_len);
        return -1;
    }

    // 2. 检查通知使能状态和连接句柄
    if (!protocol_notify_status || !protocol_chr_conn_inited) 
    {
        ESP_LOGE(TAG, "protocol notify not subscribed or conn not inited");
        return -2;
    }

    // 3. 拷贝数据到发送缓冲区
    memset(protocol_rsp_buf, 0, sizeof(protocol_rsp_buf));
    memcpy(protocol_rsp_buf, data, data_len);

    om = ble_hs_mbuf_from_flat(protocol_rsp_buf, data_len);   

    if (om == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate mbuf");
        return -1;
    }
    

    rc = ble_gatts_notify_custom(protocol_chr_conn_handle, protocol_chr_val_handle,om);
    if (rc != 0) 
    {
        ESP_LOGE(TAG, "protocol notify send fail, rc: %d", rc);
        return -3;
    }
    ESP_LOGI(TAG, "protocol notify sent, len: %d, data: %s", data_len, data);
    return 0;
}
/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) 
{
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) 
    {

        /* Service register event */
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(TAG, "registered service %s with handle=%d",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
            break;

        /* Characteristic register event */
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(TAG,
                    "registering characteristic %s with "
                    "def_handle=%d val_handle=%d",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;

        /* Descriptor register event */
        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
            break;

        /* Unknown event */
        default:
            assert(0);
            break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update heart rate subscription status
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event) 
{
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) 
    {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } 
    else 
    {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    /* Check attribute handle */
    if (event->subscribe.attr_handle == heart_rate_chr_val_handle) 
    {
        /* Update heart rate subscription status */
        heart_rate_chr_conn_handle = event->subscribe.conn_handle;
        heart_rate_chr_conn_handle_inited = true;
        heart_rate_ind_status = event->subscribe.cur_indicate;
    }

    
    if (event->subscribe.attr_handle == protocol_chr_val_handle) 
    {
        protocol_chr_conn_handle = event->subscribe.conn_handle;
        protocol_chr_conn_inited = true;
        protocol_notify_status = event->subscribe.cur_notify; 
        ESP_LOGI(TAG, "protocol notify status: %d", protocol_notify_status);
    }
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void) 
{
    /* Local variables */
    int rc = 0;

    /* 1. GATT service initialization */
    ble_svc_gatt_init();

    /* 2. Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);

    if (rc != 0) 
    {
        return rc;
    }

    /* 3. Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) 
    {
        return rc;
    }
    protocol_parse_handle = app_protocol_channel_create(app_rtos_protocol_message_put);
    if (protocol_parse_handle != NULL)
    {
        ESP_LOGI(TAG, "app protocol initialized, handle: %p", protocol_parse_handle);
    }
    else
    {
        ESP_LOGE(TAG, "app protocol initialization failed");
    }
    

    return 0;
}

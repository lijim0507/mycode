# 模块清单：ble_log
# 日志服务依赖 ble 的服务表与发送接口。

set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/ble_log/core/ble_log.c"
)
set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/ble_log/include"
)
set(DEMO_MODULE_DEPENDS ble)

# 模块清单：ble
# 根据 DEMO_PLATFORM 选择对应端口。

set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/ble/core/ble.c"
)
if(DEMO_PLATFORM STREQUAL "esp32")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/ble/port/esp32_ble_port.c"
    )
else()
    message(FATAL_ERROR "ble: 未适配平台 ${DEMO_PLATFORM}")
endif()
set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/ble/include"
    "${DEMO_MODULE_ROOT}/ble/port"
)
set(DEMO_MODULE_DEPENDS "")

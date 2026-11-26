# 模块清单：inmp441
# 根据 DEMO_PLATFORM 选择对应端口。

set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/inmp441/core/inmp441.c"
)
if(DEMO_PLATFORM STREQUAL "esp32")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/inmp441/port/esp32_inmp441_port.c"
    )
else()
    message(FATAL_ERROR "inmp441: 未适配平台 ${DEMO_PLATFORM}")
endif()
set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/inmp441/include"
    "${DEMO_MODULE_ROOT}/inmp441/port"
)
set(DEMO_MODULE_DEPENDS "")
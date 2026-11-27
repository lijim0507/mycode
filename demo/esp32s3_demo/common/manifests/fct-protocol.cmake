# 模块清单：fct-protocol
# 根据 DEMO_PLATFORM 选择对应端口。

set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/fct-protocol/core/fct.c"
)
if(DEMO_PLATFORM STREQUAL "esp32")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/fct-protocol/port/esp32_fct_port.c"
    )
else()
    message(FATAL_ERROR "fct-protocol: 未适配平台 ${DEMO_PLATFORM}")
endif()
set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/fct-protocol/include"
    "${DEMO_MODULE_ROOT}/fct-protocol/port"
)
set(DEMO_MODULE_DEPENDS "")

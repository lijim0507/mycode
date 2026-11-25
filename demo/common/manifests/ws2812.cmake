# 模块清单：ws2812
# 根据 DEMO_PLATFORM 选择对应端口。

set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/ws2812/core/ws2812.c"
    "${DEMO_MODULE_ROOT}/ws2812/core/ws2812_color.c"
    "${DEMO_MODULE_ROOT}/ws2812/core/ws2812_effect.c"
)

if(DEMO_PLATFORM STREQUAL "esp32")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/ws2812/port/esp32_rmt.c"
    )
elseif(DEMO_PLATFORM STREQUAL "stm32f1")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/ws2812/port/stm32_gpio.c"
    )
else()
    message(FATAL_ERROR "ws2812: 未适配平台 ${DEMO_PLATFORM}")
endif()

set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/ws2812/include"
    "${DEMO_MODULE_ROOT}/ws2812/port"
)
set(DEMO_MODULE_DEPENDS "")


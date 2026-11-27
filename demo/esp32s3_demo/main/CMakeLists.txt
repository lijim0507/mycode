set(DEMO_PLATFORM "esp32")

# 本 demo 使用的模块；只有列在这里的模块参与编译。
# 按需取消注释，例如：
#   utility
#   ws2812
set(DEMO_MODULES
    inmp441
    fct-protocol
    ble
    ble_log
)

include("${CMAKE_CURRENT_LIST_DIR}/../common/demo_modules.cmake")

idf_component_register(
    SRCS
        "main.c"
        "ble_demo.c"
        "fct_demo.c"
        ${DEMO_SRCS}
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        driver
        esp_timer
        freertos
        bt
        nvs_flash
)

target_include_directories(${COMPONENT_LIB} PRIVATE ${DEMO_INC_DIRS})


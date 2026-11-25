# ============================================================================
# 模块清单模板
# 复制为 demo/esp32s3_demo/common/manifests/<name>.cmake 后修改。
#
# 规则：
# 1. 只列 core/ 和当前平台的 port/；模块根目录的集成示例（例如 bms.c）不要列。
# 2. 只依赖头文件的模块放进 DEMO_MODULE_INCLUDES，不要列它的 .c。
# 3. DEMO_MODULE_DEPENDS 写模块名，由 demo_modules.cmake 自动补齐。
# ============================================================================

set(DEMO_MODULE_FILES
    "${DEMO_MODULE_ROOT}/sample/core/sample.c"
)
if(DEMO_PLATFORM STREQUAL "esp32")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/sample/port/esp32_sample_port.c"
    )
elseif(DEMO_PLATFORM STREQUAL "stm32f1")
    list(APPEND DEMO_MODULE_FILES
        "${DEMO_MODULE_ROOT}/sample/port/stm32_sample_port.c"
    )
else()
    message(FATAL_ERROR "sample: 未适配平台 ${DEMO_PLATFORM}")
endif()
set(DEMO_MODULE_INCLUDES
    "${DEMO_MODULE_ROOT}/sample/include"
    "${DEMO_MODULE_ROOT}/sample/port"
)
set(DEMO_MODULE_DEPENDS "")


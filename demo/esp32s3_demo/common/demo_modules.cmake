# ============================================================================
# ESP32 demo 模块选择
#
# 在本 demo 的 main/CMakeLists.txt 中：
#   set(DEMO_PLATFORM "esp32")
#   set(DEMO_MODULES utility ws2812)
#   include("${CMAKE_CURRENT_LIST_DIR}/../common/demo_modules.cmake")
#
# 展开后得到：
#   DEMO_SRCS     该 demo 需要编译的模块源文件（绝对路径）
#   DEMO_INC_DIRS 对应头文件目录
#
# 只有列在 DEMO_MODULES 里的模块参与编译；依赖由清单里的
# DEMO_MODULE_DEPENDS 自动补齐。
# ============================================================================

get_filename_component(DEMO_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(DEMO_REPO_ROOT "${DEMO_COMMON_DIR}/../../.." ABSOLUTE)
set(DEMO_MODULE_ROOT "${DEMO_REPO_ROOT}/modules")

if(NOT DEMO_PLATFORM)
    message(FATAL_ERROR "请先设置 DEMO_PLATFORM（ESP32 demo 固定为 esp32）")
endif()

set(DEMO_MODULE_SEEN "")
set(DEMO_MODULE_QUEUE ${DEMO_MODULES})
set(DEMO_SRCS "")
set(DEMO_INC_DIRS "")

while(DEMO_MODULE_QUEUE)
    list(GET DEMO_MODULE_QUEUE 0 _demo_module)
    list(REMOVE_AT DEMO_MODULE_QUEUE 0)

    if(NOT "${_demo_module}" IN_LIST DEMO_MODULE_SEEN)
        set(_demo_manifest "${DEMO_COMMON_DIR}/manifests/${_demo_module}.cmake")
        if(NOT EXISTS "${_demo_manifest}")
            message(FATAL_ERROR "未找到模块清单: ${_demo_manifest}")
        endif()

        set(DEMO_MODULE_FILES "")
        set(DEMO_MODULE_INCLUDES "")
        set(DEMO_MODULE_DEPENDS "")
        include("${_demo_manifest}")

        list(APPEND DEMO_SRCS ${DEMO_MODULE_FILES})
        list(APPEND DEMO_INC_DIRS ${DEMO_MODULE_INCLUDES})
        list(APPEND DEMO_MODULE_SEEN ${_demo_module})
        list(APPEND DEMO_MODULE_QUEUE ${DEMO_MODULE_DEPENDS})
    endif()
endwhile()

list(REMOVE_DUPLICATES DEMO_SRCS)
list(REMOVE_DUPLICATES DEMO_INC_DIRS)

message(STATUS "demo: 平台=${DEMO_PLATFORM} 模块=${DEMO_MODULE_SEEN}")


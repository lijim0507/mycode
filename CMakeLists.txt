cmake_minimum_required(VERSION 3.16)

set(DEMO_DIR "${CMAKE_CURRENT_LIST_DIR}/demo/esp32s3_demo")

set(IDF_TARGET "esp32s3")

set(EXTRA_COMPONENT_DIRS
    "${DEMO_DIR}/main"
)

set(SDKCONFIG_DEFAULTS
    "${DEMO_DIR}/sdkconfig.defaults"
)

set(SDKCONFIG
    "${DEMO_DIR}/sdkconfig"
)

include("$ENV{IDF_PATH}/tools/cmake/project.cmake")
project(esp32s3_demo)
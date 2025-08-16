cmake_minimum_required(VERSION 3.13)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(blink_uartcss C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(blink_uartcss
        blink_uartcss.c
)

include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(blink_uartcss PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CSS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CHANNELS=1
        PCH_NUM_SCHIBS=8
)

target_compile_options(blink_uartcss PRIVATE -Wall)

target_link_libraries(blink_uartcss PRIVATE
        picochan_css
	hardware_gpio
	hardware_timer
	hardware_uart
)

pico_add_extra_outputs(blink_uartcss)

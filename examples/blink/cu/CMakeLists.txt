cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(blinktestcu C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(blinktestcu
        blinktestcu.c
        blink_cu.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(blinktestcu PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=1
)

target_compile_options(blinktestcu PRIVATE -Wall)

target_link_libraries(blinktestcu PRIVATE
        picochan_cu
	hardware_gpio
	hardware_timer
	hardware_uart
)

pico_add_extra_outputs(blinktestcu)

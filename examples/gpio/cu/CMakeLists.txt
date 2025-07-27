cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(gdtest1cu C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(gdtest1cu
        gdtest1cu.c
        gd_cu.c
        gd_pins.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

target_compile_definitions(gdtest1cu PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=1
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=8
        PCH_CONFIG_ENABLE_MEMCHAN=0
#        GD_ENABLE_DEBUG_PRINTF=1
#        GD_ENABLE_GPIO_VERBOSE=1
#        GD_ENABLE_GPIO_WRITES=0
)

target_compile_options(gdtest1cu PRIVATE -Wall)

pico_enable_stdio_uart(gdtest1cu 0)
pico_enable_stdio_usb(gdtest1cu 1)

target_link_libraries(gdtest1cu PRIVATE
        picochan_cu
	hardware_gpio
	hardware_timer
	hardware_uart
        pico_stdio
)

#pico_set_binary_type(gdtest1cu copy_to_ram)
pico_add_extra_outputs(gdtest1cu)

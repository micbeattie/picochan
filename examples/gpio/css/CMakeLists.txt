cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(gdtest1css C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(gdtest1css
        gdtest1css.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

target_compile_definitions(gdtest1css PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CSS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=1
        PCH_CONFIG_ENABLE_MEMCHAN=0
        PCH_NUM_CSS_CUS=1
        PCH_NUM_SCHIBS=8
        GD_ENABLE_DEBUG_PRINTF=1
)

target_compile_options(gdtest1css PRIVATE -Wall)

pico_enable_stdio_uart(gdtest1css 0)
pico_enable_stdio_usb(gdtest1css 1)

target_link_libraries(gdtest1css PRIVATE
        picochan_css
	hardware_gpio
	hardware_timer
	hardware_uart
        pico_stdio
)

#pico_set_binary_type(gdtest1css copy_to_ram)
pico_add_extra_outputs(gdtest1css)

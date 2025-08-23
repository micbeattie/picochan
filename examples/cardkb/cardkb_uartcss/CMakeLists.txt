cmake_minimum_required(VERSION 3.13)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(cardkb_uartcss C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(cardkb_uartcss
        cardkb_uartcss.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_MEMCHAN 0 CACHE STRING "Enable/disable memchan (1 or 0)")
set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(cardkb_uartcss PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CSS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_CONFIG_ENABLE_MEMCHAN=${PCH_CONFIG_ENABLE_MEMCHAN}
        PCH_NUM_CHANNELS=1
        PCH_NUM_SCHIBS=8
)

target_compile_options(cardkb_uartcss PRIVATE -Wall)

pico_enable_stdio_usb(cardkb_uartcss 1)
pico_enable_stdio_uart(cardkb_uartcss 0)

target_link_libraries(cardkb_uartcss PRIVATE
        picochan_css
	hardware_gpio
	hardware_timer
	hardware_uart
        pico_stdio_usb
)

#pico_set_binary_type(cardkb_uartcss copy_to_ram)
pico_add_extra_outputs(cardkb_uartcss)

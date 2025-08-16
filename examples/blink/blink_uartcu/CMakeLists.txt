#
# blink_uartcu runs the CU side of the blink Picochan example and is
# configured to run on core 0 and serve up its "blink" device via
# a uart channel connected to UART 1 via GPIO pins 4-7.
# A physical connection is needed to a separate Pico that is running
# a CSS configured to use a UART channel for that connection, such as
# the blink_uartcss example program.
# Although it would be possible to run that CSS side on core 1 of
# "this" Pico (the blink_uartcu one) and have physical connections
# between appropriate pins mapped to the Pico's two UARTs, a more
# practical configuration would use a memory channel (memchan)
# between the cores with no need for the UARTs or any physical
# channel-to-CU connections - see the blink_memchan example for that.
#

cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(blink_uartcu C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(blink_uartcu
        blink_uartcu.c
        ../cu/blink_cu.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(blink_uartcu PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=1
)

target_compile_options(blink_uartcu PRIVATE -Wall)

target_link_libraries(blink_uartcu PRIVATE
        picochan_cu
	hardware_gpio
	hardware_timer
	hardware_uart
)

pico_add_extra_outputs(blink_uartcu)

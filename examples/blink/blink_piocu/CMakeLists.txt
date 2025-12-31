#
# blink_piocu runs the CU side of the blink Picochan example and is
# configured to run on core 0 and serve up its "blink" device via
# a PIO channel connected to GPIO pins 0-3 in "CU-side order", i.e.
# respectively TX_CLOCK_IN, TX_DATA_OUT, RX_CLOCK_OUT, RX_DATA_IN.
# A physical connection is needed to a separate Pico that is running
# a CSS configured to use a PIO channel for that connection with the
# appropriate pin connections in "CSS-side order", i.e. with
# TX_CLOCK_IN<->RX_CLOCK_OUT, TX_DATA_OUT<->RX_DATA_IN, such as the
# blink_uartcss example program.
#

cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(blink_piocu C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(blink_piocu
        blink_piocu.c
        ../cu/blink_cu.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(blink_piocu PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=1
)

target_compile_options(blink_piocu PRIVATE -Wall)

target_link_libraries(blink_piocu PRIVATE
        picochan_cu
	hardware_gpio
	hardware_timer
	hardware_pio
)

pico_add_extra_outputs(blink_piocu)

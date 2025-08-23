#
# cardkb_memchan runs the complete blink Picochan example on a
# single Pico. The CSS is run on core 0 and the CU on core 1.
# Instead of needing physical channel connections between CSS
# and CU, this configuration uses a memory channel (memchan)
# so that CSS-to-CU communication happens directly via
# memory-to-memory DMA for data transfers and 4-byte
# writes/reads from memory for command transfers.
#

cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(cardkb_memchan C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(cardkb_memchan
        cardkb_memchan.c
        ../cu/cardkb_cu.c
        ../cu/cardkb_i2c.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(cardkb_memchan PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CUS=1
        PCH_NUM_CHANNELS=1
        PCH_NUM_SCHIBS=8
        PICO_MAX_SHARED_IRQ_HANDLERS=8
)

target_compile_options(cardkb_memchan PRIVATE -Wall)

pico_enable_stdio_usb(cardkb_memchan 1)
pico_enable_stdio_uart(cardkb_memchan 0)

target_link_libraries(cardkb_memchan PRIVATE
        picochan_css
        picochan_cu
	hardware_gpio
        hardware_i2c
	hardware_timer
        pico_multicore
        pico_stdio_usb
)

pico_add_extra_outputs(cardkb_memchan)

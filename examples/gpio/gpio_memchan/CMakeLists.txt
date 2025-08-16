#
# gpio_memchan runs the complete blink Picochan example on a
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

project(gpio_memchan C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(gpio_memchan
        gpio_memchan.c
        ../cu/gd_cu.c
        ../cu/gd_pins.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

set(GD_IGNORE_GPIO_WRITE_MASK 0 CACHE STRING "Force CU to ignore attempted writes to GPIO pin numbers in this mask")

target_compile_definitions(gpio_memchan PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=1
        PCH_NUM_CHANNELS=1
        PCH_NUM_SCHIBS=8
        GD_IGNORE_GPIO_WRITE_MASK=${GD_IGNORE_GPIO_WRITE_MASK}
)

target_compile_options(gpio_memchan PRIVATE -Wall)

target_link_libraries(gpio_memchan PRIVATE
        picochan_css
        picochan_cu
	hardware_gpio
	hardware_timer
        pico_multicore
)

pico_add_extra_outputs(gpio_memchan)

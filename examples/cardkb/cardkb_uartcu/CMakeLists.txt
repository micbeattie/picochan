cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(cardkb_uartcu C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(cardkb_uartcu
        cardkb_uartcu.c
        ../cu/cardkb_cu.c
        ../cu/cardkb_i2c.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_MEMCHAN 0 CACHE STRING "Enable/disable memchan (1 or 0)")
set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

set(GD_IGNORE_GPIO_WRITE_MASK 0 CACHE STRING "Force CU to ignore attempted writes to GPIO pin numbers in this mask")

target_compile_definitions(cardkb_uartcu PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=8
        PCH_CONFIG_ENABLE_MEMCHAN=${PCH_CONFIG_ENABLE_MEMCHAN}
        GD_IGNORE_GPIO_WRITE_MASK=${GD_IGNORE_GPIO_WRITE_MASK}
)

target_compile_options(cardkb_uartcu PRIVATE -Wall)

target_link_libraries(cardkb_uartcu PRIVATE
        picochan_cu
	hardware_gpio
	hardware_i2c
	hardware_timer
	hardware_uart
)

#pico_set_binary_type(cardkb_uartcu copy_to_ram)
pico_add_extra_outputs(cardkb_uartcu)

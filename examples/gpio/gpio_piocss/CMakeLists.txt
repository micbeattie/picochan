cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(gpio_piocss C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(gpio_piocss
        gpio_piocss.c
)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(gpio_piocss PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CSS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_CONFIG_ENABLE_MEMCHAN=${PCH_CONFIG_ENABLE_MEMCHAN}
        PCH_NUM_CHANNELS=1
        PCH_NUM_SCHIBS=8
)

target_compile_options(gpio_piocss PRIVATE -Wall)

target_link_libraries(gpio_piocss PRIVATE
        picochan_css
	hardware_gpio
	hardware_timer
	hardware_pio
        pico_status_led
)

#pico_set_binary_type(gpio_piocss copy_to_ram)
pico_add_extra_outputs(gpio_piocss)

#
# mqtt_full_piocu runs the CU side of the mqtt_full Picochan example
# and is configured to run on core 0 and serve up its MQTT devices via
# a PIO channel connected to GPIO pins 0-3 in "CU-side order", i.e.
# respectively TX_CLOCK_IN, TX_DATA_OUT, RX_CLOCK_OUT, RX_DATA_IN.
# A physical connection is needed to a separate Pico that is running
# a CSS configured to use a PIO channel for that connection with the
# appropriate pin connections in "CSS-side order", i.e. with
# TX_CLOCK_IN<->RX_CLOCK_OUT, TX_DATA_OUT<->RX_DATA_IN, such as the
# mqtt_full_piocss example program.
#

cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)
set(PICO_BOARD pico2_w)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(mqtt_full_piocu C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(WIFI_SSID "$ENV{WIFI_SSID}")
set(WIFI_PASSWORD "$ENV{WIFI_PASSWORD}")

if ("${WIFI_SSID}" STREQUAL "")
        message(FATAL_ERROR "WIFI_SSID is not defined")
endif()

if ("${WIFI_PASSWORD}" STREQUAL "")
        message(FATAL_ERROR "WIFI_PASSWORD is not defined")
endif()

add_executable(mqtt_full_piocu
        mqtt_full_piocu.c
        ../cu/mqtt_cu.c
	../cu/ccw_buffers.c
	../cu/ccw.c
	../cu/ccw_global.c
	../cu/ccw_ring.c
	../cu/incoming.c
	../cu/md_tmbuf.c
	../cu/tasks.c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(mqtt_full_piocu 1)
pico_enable_stdio_uart(mqtt_full_piocu 0)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(mqtt_full_piocu PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_TRC_BUFFER_SIZE=16384
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=8
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        PICO_MAX_SHARED_IRQ_HANDLERS=8
)

target_compile_options(mqtt_full_piocu PRIVATE -Wall)

target_include_directories(mqtt_full_piocu PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(mqtt_full_piocu PRIVATE
        picochan_cu
        picochan_hldev
        pico_cyw43_arch_lwip_poll
        pico_stdlib
        pico_status_led
        pico_lwip_mqtt
        hardware_gpio
        hardware_pio
)

pico_add_extra_outputs(mqtt_full_piocu)

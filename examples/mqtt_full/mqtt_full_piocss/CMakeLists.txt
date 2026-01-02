#
# mqtt_full_piocss runs the CSS side of the full MQTT Picochan example
# and is configured to run on core 0 and connect to an mqtt_full CU
# instance via a PIO channel connected to PIO0 via GPIO pins 0-3.
# A physical connection is needed to a separate Pico that is hosting
# a PIO CU via that connection with a mqtt_full devices on unit
# addresses 0 and 1, such as the mqtt_full_piocu example program.
#

cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(mqtt_full_piocss C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(MQTT_USERNAME "$ENV{MQTT_USERNAME}")
set(MQTT_PASSWORD "$ENV{MQTT_PASSWORD}")
set(MQTT_SERVER_HOST "$ENV{MQTT_SERVER_HOST}")
set(MQTT_SERVER_PORT "$ENV{MQTT_SERVER_PORT}")

if ("${MQTT_SERVER_HOST}" STREQUAL "")
        message(FATAL_ERROR "MQTT_SERVER_HOST is not defined")
endif()

if ("${MQTT_SERVER_PORT}" STREQUAL "")
        message("MQTT_SERVER_PORT defaulting to 1883")
        set(MQTT_SERVER_PORT "1883")
endif()

add_executable(mqtt_full_piocss
        mqtt_full_piocss.c
        ../css_example.c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(mqtt_full_piocss 1)
pico_enable_stdio_uart(mqtt_full_piocss 0)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(mqtt_full_piocss PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CSS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_TRC_BUFFER_SIZE=16384
        PCH_NUM_CHANNELS=1
        PCH_NUM_SCHIBS=8
        MQTT_USERNAME=\"${MQTT_USERNAME}\"
        MQTT_PASSWORD=\"${MQTT_PASSWORD}\"
        MQTT_SERVER_HOST=\"${MQTT_SERVER_HOST}\"
        MQTT_SERVER_PORT=${MQTT_SERVER_PORT}
        PICO_MAX_SHARED_IRQ_HANDLERS=8
)

target_compile_options(mqtt_full_piocss PRIVATE -Wall)

target_include_directories(mqtt_full_piocss PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(mqtt_full_piocss PRIVATE
        picochan_css
        pico_stdlib
        hardware_gpio
        hardware_timer
        hardware_pio
        pico_status_led
)

pico_add_extra_outputs(mqtt_full_piocss)

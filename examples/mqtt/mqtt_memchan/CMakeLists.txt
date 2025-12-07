#
# mqtt_memchan runs the complete mqtt Picochan example on a
# single Pico. The CSS is run on core 0 and the CU on core 1.
# Instead of needing physical channel connections between CSS
# and CU, this configuration uses a memory channel (memchan)
# so that CSS-to-CU communication happens directly via
# memory-to-memory DMA for data transfers and 4-byte
# writes/reads from memory for command transfers.
#

cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico)
set(PICO_BOARD pico2_w)

#include(pico_sdk_import.cmake)
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

project(mqtt_memchan C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(WIFI_SSID "$ENV{WIFI_SSID}")
set(WIFI_PASSWORD "$ENV{WIFI_PASSWORD}")
set(MQTT_USERNAME "$ENV{MQTT_USERNAME}")
set(MQTT_PASSWORD "$ENV{MQTT_PASSWORD}")
set(MQTT_SERVER_HOST "$ENV{MQTT_SERVER_HOST}")
set(MQTT_SERVER_PORT "$ENV{MQTT_SERVER_PORT}")

if ("${WIFI_SSID}" STREQUAL "")
        message(FATAL_ERROR "WIFI_SSID is not defined")
endif()

if ("${WIFI_PASSWORD}" STREQUAL "")
        message(FATAL_ERROR "WIFI_PASSWORD is not defined")
endif()

if ("${MQTT_SERVER_HOST}" STREQUAL "")
        message(FATAL_ERROR "MQTT_SERVER_HOST is not defined")
endif()

if ("${MQTT_SERVER_PORT}" STREQUAL "")
        message("MQTT_SERVER_PORT defaulting to 1883")
        set(MQTT_SERVER_PORT "1883")
endif()

add_executable(mqtt_memchan
        mqtt_memchan.c
        ../cu/mqtt_cu.c
        ../cu/mqtt_util.c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(mqtt_memchan 1)
pico_enable_stdio_uart(mqtt_memchan 0)

#set(PCH_COMPILE_ENABLE_STRICT_WARNINGS 1)
include($ENV{PICOCHAN_PATH}/CMakeLists.txt)

set(PCH_CONFIG_ENABLE_TRACE 1 CACHE STRING "Enable/disable tracing (1 or 0)")

target_compile_definitions(mqtt_memchan PRIVATE
        PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
        PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
        PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
        PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
        PCH_CONFIG_ENABLE_TRACE=${PCH_CONFIG_ENABLE_TRACE}
        PCH_NUM_CUS=1
        PCH_MAX_DEVIBS_PER_CU=1
        PCH_NUM_CHANNELS=1
        PCH_NUM_SCHIBS=8
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        MQTT_USERNAME=\"${MQTT_USERNAME}\"
        MQTT_PASSWORD=\"${MQTT_PASSWORD}\"
        MQTT_SERVER_HOST=\"${MQTT_SERVER_HOST}\"
        MQTT_SERVER_PORT=${MQTT_SERVER_PORT}
        PICO_MAX_SHARED_IRQ_HANDLERS=8
)

target_compile_options(mqtt_memchan PRIVATE -Wall)

target_include_directories(mqtt_memchan PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(mqtt_memchan PRIVATE
        picochan_css
        picochan_hldev
        pico_cyw43_arch_lwip_poll
        pico_stdlib
        pico_status_led
        pico_lwip_mqtt
        pico_multicore
)

pico_add_extra_outputs(mqtt_memchan)

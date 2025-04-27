add_library(picochan_base INTERFACE)

target_include_directories(picochan_base INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}
)

target_sources(picochan_base INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/bsize/bsize.c
        ${CMAKE_CURRENT_LIST_DIR}/dmachan/mem_peer.c
        ${CMAKE_CURRENT_LIST_DIR}/dmachan/rx_channel.c
        ${CMAKE_CURRENT_LIST_DIR}/dmachan/tx_channel.c
        ${CMAKE_CURRENT_LIST_DIR}/dmachan/uart.c
        ${CMAKE_CURRENT_LIST_DIR}/proto/payload.c
        ${CMAKE_CURRENT_LIST_DIR}/trc/trace.c
        ${CMAKE_CURRENT_LIST_DIR}/txsm/txsm.c
)

if (PCH_COMPILE_ENABLE_STRICT_WARNINGS)
        message("Enabling strict compiler warnings for picochan_base: warnings from SDK source files are to be expected")
        target_compile_options(picochan_base INTERFACE
                $<$<COMPILE_LANGUAGE:C>:
                -Wall
                -Wextra
                -Wdouble-promotion
                -Wstrict-prototypes
                -Wstrict-overflow
                -Werror-implicit-function-declaration
                -Wfloat-equal
                -Wshadow
                -Wwrite-strings
                -Wsign-compare
                -Wmissing-format-attribute
                -Wunreachable-code
                -Wcast-align
                -Wcast-function-type
                -Wcast-qual
                -Wnull-dereference
                -Wuninitialized
                -Wunused
                -Wunused-function
                -Wreturn-type
                -Wredundant-decls>
        )
endif()

target_link_libraries(picochan_base INTERFACE
        hardware_base
        hardware_dma
        hardware_irq
        hardware_regs
        hardware_structs
        pico_runtime
        pico_runtime_init
)

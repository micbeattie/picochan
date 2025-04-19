add_library(picochan_cu INTERFACE)

target_include_directories(picochan_cu INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(picochan_cu INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/callback.c
        ${CMAKE_CURRENT_LIST_DIR}/cu.c
        ${CMAKE_CURRENT_LIST_DIR}/dev_api.c
        ${CMAKE_CURRENT_LIST_DIR}/irq.c
        ${CMAKE_CURRENT_LIST_DIR}/rx_handle.c
        ${CMAKE_CURRENT_LIST_DIR}/tx_commands.c
        ${CMAKE_CURRENT_LIST_DIR}/tx_handle.c
        ${CMAKE_CURRENT_LIST_DIR}/uart_cu.c
) 

if (PCH_COMPILE_ENABLE_STRICT_WARNINGS)
        message("Enabling strict compiler warnings for picochan_cu: warnings from SDK source files are to be expected")
        target_compile_options(picochan_cu INTERFACE
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

target_link_libraries(picochan_cu INTERFACE picochan_base)

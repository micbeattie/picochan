add_library(picochan_css INTERFACE)

target_include_directories(picochan_css INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(picochan_css INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/api.c
        ${CMAKE_CURRENT_LIST_DIR}/api_extra.c
        ${CMAKE_CURRENT_LIST_DIR}/ccw_fetch.c
        ${CMAKE_CURRENT_LIST_DIR}/css.c
        ${CMAKE_CURRENT_LIST_DIR}/css_cu.c
        ${CMAKE_CURRENT_LIST_DIR}/irq.c
        ${CMAKE_CURRENT_LIST_DIR}/isc.c
        ${CMAKE_CURRENT_LIST_DIR}/notify.c
        ${CMAKE_CURRENT_LIST_DIR}/rx_handle.c
        ${CMAKE_CURRENT_LIST_DIR}/schib_dlist.c
        ${CMAKE_CURRENT_LIST_DIR}/schib_func.c
        ${CMAKE_CURRENT_LIST_DIR}/schib_response.c
        ${CMAKE_CURRENT_LIST_DIR}/tx_handle.c
        ${CMAKE_CURRENT_LIST_DIR}/ua_dlist.c
        ${CMAKE_CURRENT_LIST_DIR}/ua_slist.c
)

if (PCH_COMPILE_ENABLE_STRICT_WARNINGS)
        message("Enabling strict compiler warnings for picochan_css: warnings from SDK source files are to be expected")
        target_compile_options(picochan_css INTERFACE
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

target_link_libraries(picochan_css INTERFACE picochan_base)

add_library(picochan_hldev INTERFACE)

target_include_directories(picochan_hldev INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(picochan_hldev INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/hldev.c
) 

if (PCH_COMPILE_ENABLE_STRICT_WARNINGS)
        message("Enabling strict compiler warnings for picochan_hldev: warnings from SDK source files are to be expected")
        target_compile_options(picochan_hldev INTERFACE
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

target_link_libraries(picochan_hldev INTERFACE picochan_cu)

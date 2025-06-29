cmake_minimum_required(VERSION 3.13)

project(PicochanDocs)

find_package(Doxygen QUIET)

if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR
  PARENT_PATH PICOCHAN_SOURCE_DIR)

message("CMAKE_CURRENT_SOURCE_DIR is ${CMAKE_CURRENT_SOURCE_DIR}")
message("CMAKE_CURRENT_BINARY_DIR is ${CMAKE_CURRENT_BINARY_DIR}")
message("PROJECT_SOURCE_DIR is ${PROJECT_SOURCE_DIR}")
message("PICOCHAN_SOURCE_DIR is ${PICOCHAN_SOURCE_DIR}")

set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

set(DOXY_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
string(REPLACE ";" " " DOXY_INPUT_DIRS "${PICO_DOXYGEN_PATHS}")

configure_file(${doxyfile_in} ${doxyfile} @ONLY)

add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)

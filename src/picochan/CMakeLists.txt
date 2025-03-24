if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/base/include/picochan/ccw.h)
  message("Picochan available at ${CMAKE_CURRENT_LIST_DIR}")
else()
  message(FATAL_ERROR "cannot find expected picochan source files under this directory: ${CMAKE_CURRENT_LIST_DIR}")
endif()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/base picochan_base)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/cu picochan_cu)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/css picochan_css)

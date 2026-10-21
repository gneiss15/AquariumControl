# CMakeLists.txt, Version: 1.01
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${PRJS_IDF_DEPENDENCIES}/arduino" "${CMAKE_CURRENT_LIST_DIR}/Sw/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include("${PRJS_VERSION_FILE}")
project(TmrSwA8T)

# IDF's generic flash target places this Main image at the factory/Rescue address.
# Installation is performed only with the existing signed Rescue upload path.
add_custom_target(a8t_serial_flash_block
  COMMAND ${CMAKE_COMMAND} -E echo "A8T Main must be uploaded as a signed app0 artifact through Rescue. Generic IDF flash is disabled."
  COMMAND ${CMAKE_COMMAND} -E false)
foreach(NAME flash app-flash bootloader-flash partition-table-flash)
  if(TARGET ${NAME})
    add_dependencies(${NAME} a8t_serial_flash_block)
  endif()
endforeach()

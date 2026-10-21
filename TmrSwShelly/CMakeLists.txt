# CMakeLists.txt, Version: 1.05
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${PRJS_IDF_DEPENDENCIES}/arduino" "${CMAKE_CURRENT_LIST_DIR}/Sw/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include("${PRJS_VERSION_FILE}")
project(TmrSwShelly)

# The generic IDF flash address selects Factory/Rescue, not this Main slot.
add_custom_target(shelly_serial_flash_block
  COMMAND ${CMAKE_COMMAND} -E echo "Use the signed Main artifact through Rescue; generic IDF flash is disabled."
  COMMAND ${CMAKE_COMMAND} -E false)
foreach(NAME flash app-flash bootloader-flash partition-table-flash)
  if(TARGET ${NAME})
    add_dependencies(${NAME} shelly_serial_flash_block)
  endif()
endforeach()

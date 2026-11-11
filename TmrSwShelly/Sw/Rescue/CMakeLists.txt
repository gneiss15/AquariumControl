# CMakeLists.txt, Version: 1.06
cmake_minimum_required(VERSION 3.16)
set(MINIMAL_BUILD ON)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(RescueVersionsFile "${CMAKE_CURRENT_LIST_DIR}/../../Versions.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RescueVersionsFile}")
file(STRINGS "${RescueVersionsFile}" RescueVersionLine REGEX "^Rescue=")
string(REGEX REPLACE "^Rescue=" "" PROJECT_VER "${RescueVersionLine}")
project(ShellyRescue)

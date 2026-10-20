# CMakeLists.txt, Version: 1.05
cmake_minimum_required(VERSION 3.16)
set(MINIMAL_BUILD ON)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../Versions.txt" RescueVersionLine REGEX "^Rescue=")
string(REGEX REPLACE "^Rescue=" "" PROJECT_VER "${RescueVersionLine}")
project(ShellyRescue)

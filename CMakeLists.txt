cmake_minimum_required(VERSION 3.20)

option(TB_BUILD_SAMPLES "" OFF)

if(NOT TB_SOURCE_PATH)
  message(FATAL_ERROR "Please specify TB_SOURCE_PATH as a source path to the toybox engine")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(FINAL "Compile with the intention to redistribute" OFF)
set(TB_BUILD_SAMPLES OFF)
set(TB_BUILD_VIEWER OFF)
option(PROFILE_TRACY "Compile with support for the tracy profiler" ON)
option(COOK_ASSETS "Process assets for runtime loading" ON)

get_filename_component(TB_ABS_PATH ${TB_SOURCE_PATH} ABSOLUTE)
message("Including: ${TB_ABS_PATH}/CMakeLists.txt")
include(${TB_ABS_PATH}/CMakeLists.txt)

# Determine version from the vcpkg json manifest
file(READ vcpkg.json VCPKG_MANIFEST)
string(JSON VCPKG_VERSION GET ${VCPKG_MANIFEST} "version")

project(upland
        VERSION ${VCPKG_VERSION}
        DESCRIPTION "A single stick shooter built on the toybox engine"
        LANGUAGES C CXX)

set(GAME_NAME "upland")
set(GAME_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(GAME_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(GAME_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})

file(GLOB_RECURSE source "source/*.c" "source/*.cpp" )

# Must pass source as a string or else it won't properly be interpreted as a list
tb_add_app(upland "${source}")

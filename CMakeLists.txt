# CMakeLists.txt
cmake_minimum_required (VERSION 3.8.2)
project (gpmeta-export)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

file(GLOB SOURCES GPMF_parser.c "demo/*.cpp" "demo/GPMF_mp4reader.c")
# file(GLOB SOURCES GPMF_parser.c "demo/*.cpp" "demo/*.c")

add_executable(gpmeta-export ${SOURCES})

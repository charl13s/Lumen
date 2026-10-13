cmake_minimum_required(VERSION 3.16)
project(LUMEN LANGUAGES CXX)

# LUMEN, Checkpoint 1. Plain C++17 with no libraries: every graphics algorithm
# is our own code, and the output is saved as images.
#
# Every .cpp file in src/core/ is built, so adding a file needs no change here.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

file(GLOB LUMEN_CORE CONFIGURE_DEPENDS src/core/*.cpp)
if (NOT LUMEN_CORE)
    message(STATUS "No source files in src/core yet.")
    return()
endif()

add_library(lumen_core STATIC ${LUMEN_CORE})
target_include_directories(lumen_core PUBLIC src)

if (EXISTS ${CMAKE_SOURCE_DIR}/src/main.cpp)
    add_executable(lumen_cp1 src/main.cpp)
    target_link_libraries(lumen_cp1 PRIVATE lumen_core)
    set_target_properties(lumen_cp1 PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT lumen_cp1)
endif()

if (EXISTS ${CMAKE_SOURCE_DIR}/tests/test_core.cpp)
    enable_testing()
    add_executable(lumen_tests tests/test_core.cpp)
    target_link_libraries(lumen_tests PRIVATE lumen_core)
    target_compile_definitions(lumen_tests PRIVATE LUMEN_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/")
    add_test(NAME core COMMAND lumen_tests)
endif()

cmake_minimum_required(VERSION 3.21)

# A build folder made for Checkpoint 2 has no vcpkg in it, and vcpkg cannot be
# added to an existing build folder. Say so plainly instead of failing later.
if (DEFINED CACHE{CMAKE_PROJECT_NAME} AND NOT VCPKG_MANIFEST_MODE)
    message(FATAL_ERROR
        "This build folder was made before vcpkg was added (Checkpoint 2).\n"
        "Delete the build folder, then run:  cmake --preset default")
endif()

project(LUMEN LANGUAGES CXX)

# LUMEN, Checkpoint 3: the gallery in OpenGL.
#
# The libraries come from vcpkg (see vcpkg.json and README.md). Every .cpp in
# src/core/ and src/rooms/ is built, so adding a file needs no change here.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT lumen)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(glfw3         CONFIG REQUIRED)
find_package(glad          CONFIG REQUIRED)
find_package(glm           CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")

# --- the core: our own algorithms, the plan, and the OpenGL wrappers ---------
file(GLOB LUMEN_CORE CONFIGURE_DEPENDS src/core/*.cpp)
add_library(lumen_core STATIC ${LUMEN_CORE})
target_include_directories(lumen_core PUBLIC src ${STB_INCLUDE_DIRS})
# glad must be the only OpenGL header, so GLFW is told not to include its own.
target_compile_definitions(lumen_core PUBLIC GLFW_INCLUDE_NONE)
target_link_libraries(lumen_core PUBLIC
    glfw glad::glad glm::glm nlohmann_json::nlohmann_json)

# --- the five galleries, one file each, and the list of them -----------------
file(GLOB LUMEN_ROOMS CONFIGURE_DEPENDS src/rooms/*.cpp)
if (LUMEN_ROOMS)
    add_library(lumen_rooms STATIC ${LUMEN_ROOMS})
    target_link_libraries(lumen_rooms PUBLIC lumen_core)
endif()

# --- the gallery -------------------------------------------------------------
if (EXISTS ${CMAKE_SOURCE_DIR}/src/main.cpp)
    add_executable(lumen src/main.cpp)
    target_link_libraries(lumen PRIVATE lumen_core)
    if (LUMEN_ROOMS)
        target_link_libraries(lumen PRIVATE lumen_rooms)
    endif()
    set_target_properties(lumen PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

# --- the Checkpoint 1 and 2 pages and animations, saved as images and GIFs ----
if (EXISTS ${CMAKE_SOURCE_DIR}/src/pages_main.cpp)
    add_executable(lumen_pages src/pages_main.cpp)
    target_link_libraries(lumen_pages PRIVATE lumen_core)
    set_target_properties(lumen_pages PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

# --- tests: no window or GPU needed ------------------------------------------
if (EXISTS ${CMAKE_SOURCE_DIR}/tests/test_core.cpp)
    enable_testing()
    add_executable(lumen_tests tests/test_core.cpp)
    target_link_libraries(lumen_tests PRIVATE lumen_core)
    target_compile_definitions(lumen_tests PRIVATE LUMEN_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/")
    add_test(NAME core COMMAND lumen_tests)
endif()

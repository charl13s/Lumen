cmake_minimum_required(VERSION 3.21)
project(LUMEN LANGUAGES CXX)

# ---------------------------------------------------------------------------
# LUMEN - a virtual gallery.  ICS 3204 Computer Graphics semester project.
#
# Dependencies come from vcpkg in manifest mode (see vcpkg.json), so nobody has
# to hunt for libraries by hand. Build instructions are in README.md.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    # Visual Studio defaults to building ALL_BUILD; make F5 run the gallery.
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT lumen)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(glfw3  CONFIG REQUIRED)
find_package(glad   CONFIG REQUIRED)
find_package(glm    CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")

# --- the shared core -------------------------------------------------------
# Everything five people depend on. Changes here go through a pull request.
add_library(lumen_core STATIC
    src/core/Plan.cpp
    src/core/Raster.cpp
    src/core/Collision.cpp
    src/core/Lighting.cpp
    src/core/Camera.cpp
    src/core/Gfx.cpp
    src/core/Builders.cpp
    src/core/Minimap.cpp
    src/core/Registry.cpp
)
target_include_directories(lumen_core PUBLIC src ${STB_INCLUDE_DIRS})
target_link_libraries(lumen_core PUBLIC
    glfw glad::glad glm::glm nlohmann_json::nlohmann_json)

# --- the rooms -------------------------------------------------------------
# One file per member. Add nothing else here; your room is self-contained.
add_library(lumen_rooms STATIC
    src/rooms/RoomOne.cpp      # Shawn Kuria
    src/rooms/RoomTwo.cpp      # Charles Murage
    src/rooms/RoomThree.cpp    # Timothy Kimani
    src/rooms/RoomFour.cpp     # Erykah Diang'a
    src/rooms/RoomFive.cpp     # Priscillah Njoroge
)
target_link_libraries(lumen_rooms PUBLIC lumen_core)

# --- the application -------------------------------------------------------
add_executable(lumen src/main.cpp)
target_link_libraries(lumen PRIVATE lumen_rooms lumen_core)

# Run from the repo root so assets/ and shaders/ resolve, whatever the IDE does.
set_target_properties(lumen PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# --- tests -----------------------------------------------------------------
# These need no GPU and run in about a second. Run them before you push.
enable_testing()
add_executable(lumen_tests tests/test_core.cpp)
target_link_libraries(lumen_tests PRIVATE lumen_core)
add_test(NAME core COMMAND lumen_tests)

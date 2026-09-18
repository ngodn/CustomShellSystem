cmake_minimum_required(VERSION 3.25)
project(CompassMinimap LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Repo root is two levels up (extensions/compass-n-mmap -> repo).
get_filename_component(CSS_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_library(compass_minimap SHARED src/main.cpp)
target_include_directories(compass_minimap PRIVATE
    "${CSS_REPO}/native/include"   # <cssx/api.h>, <cssx/client.hpp>, <cssx/hud.h>
    "${CSS_REPO}/native/vendor")   # nlohmann/json.hpp
# The output DLL name must match extension.json "entry".
set_target_properties(compass_minimap PROPERTIES PREFIX "")

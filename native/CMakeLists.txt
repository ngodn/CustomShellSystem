cmake_minimum_required(VERSION 3.25)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" CSS_VERSION LIMIT_COUNT 1)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CSS_NUMERIC_VERSION "${CSS_VERSION}")
project(CustomShellSystem VERSION ${CSS_NUMERIC_VERSION} LANGUAGES C CXX)
configure_file(src/version.hpp.in generated/css_version.hpp @ONLY)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CSS_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../reference/ue4ss-sdk-d7e7826d" CACHE PATH "Retained d7e7826d header SDK; runtime imports are pinned separately")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/ue4ss-runtime.json" CSS_UE4SS_RUNTIME_JSON)
string(JSON CSS_UE4SS_RUNTIME_REVISION GET "${CSS_UE4SS_RUNTIME_JSON}" revision)
string(JSON CSS_UE4SS_IMPORT_RELATIVE GET "${CSS_UE4SS_RUNTIME_JSON}" import_library)
set(CSS_UE4SS_IMPORT_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../${CSS_UE4SS_IMPORT_RELATIVE}" CACHE FILEPATH "Pinned runtime import library; headers remain at CSS_SDK")
add_library(css_data STATIC src/data.cpp src/animations.cpp src/packages.cpp src/controls.cpp src/body_geometry.cpp src/extension_data.cpp)
set_target_properties(css_data PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(css_data PUBLIC src vendor)
if(WIN32)
  target_link_libraries(css_data PUBLIC bcrypt)
else()
  find_package(OpenSSL REQUIRED)
  target_link_libraries(css_data PUBLIC OpenSSL::Crypto)
endif()
# The in-tree CSSX host (cssx_core, cssx_lua, cssx_validate) was removed: CSSX is now the
# standalone build under extensions/core. The CSS core keeps only its own reflected access.
add_library(css_player_recovery STATIC shared/player_recovery.cpp)
target_include_directories(css_player_recovery PUBLIC shared include vendor)
set_target_properties(css_player_recovery PROPERTIES POSITION_INDEPENDENT_CODE ON)
# The Cheat Menu is a CSSX extension built by extensions/core/CMakeLists.txt from
# extensions/cheat-menu. The CSS build keeps only its own targets.
option(CSS_DATA_TESTS_ONLY "Build portable catalog/state tests without the UE4SS runtime" OFF)
if(NOT WIN32 OR CSS_DATA_TESTS_ONLY)
  enable_testing()
  add_executable(css_extension_tests ../tests/extension_tests.cpp)
  target_link_libraries(css_extension_tests PRIVATE css_data)
  add_test(NAME css_extensions COMMAND css_extension_tests)
  add_executable(css_tests ../tests/data_tests.cpp)
  target_link_libraries(css_tests PRIVATE css_data)
  add_test(NAME css_data COMMAND css_tests)
  add_executable(css_animation_tests ../tests/animation_tests.cpp)
  target_link_libraries(css_animation_tests PRIVATE css_data)
  add_test(NAME css_animations COMMAND css_animation_tests)
  add_executable(css_animation_runtime_tests ../tests/animation_runtime_tests.cpp)
  target_include_directories(css_animation_runtime_tests PRIVATE src)
  add_test(NAME css_animation_runtime COMMAND css_animation_runtime_tests)
  add_executable(css_control_tests ../tests/control_tests.cpp)
  target_link_libraries(css_control_tests PRIVATE css_data)
  add_test(NAME css_controls COMMAND css_control_tests)
  add_executable(css_body_geometry_tests ../tests/body_geometry_tests.cpp)
  target_link_libraries(css_body_geometry_tests PRIVATE css_data)
  add_executable(css_package_tests ../tests/package_tests.cpp)
  target_link_libraries(css_package_tests PRIVATE css_data)
  add_executable(css_discovery_tests ../tests/discovery_tests.cpp)
  target_link_libraries(css_discovery_tests PRIVATE css_data)
  add_executable(css_startup_tests ../tests/startup_tests.cpp)
  target_link_libraries(css_startup_tests PRIVATE css_data)
  add_test(NAME css_startup COMMAND css_startup_tests)
  add_executable(css_recovery_tests ../tests/recovery_tests.cpp)
  target_include_directories(css_recovery_tests PRIVATE src)
  add_test(NAME css_recovery COMMAND css_recovery_tests)
  add_executable(css_skeleton_tests ../tests/skeleton_compatibility_tests.cpp)
  target_include_directories(css_skeleton_tests PRIVATE src)
  add_test(NAME css_skeleton_compatibility COMMAND css_skeleton_tests)
  add_executable(css_inventory_motion_tests ../tests/inventory_motion_tests.cpp)
  target_include_directories(css_inventory_motion_tests PRIVATE src)
  add_test(NAME css_inventory_motion COMMAND css_inventory_motion_tests)
  add_executable(css_inventory_light_tests ../tests/inventory_light_tests.cpp)
  target_include_directories(css_inventory_light_tests PRIVATE src)
  add_test(NAME css_inventory_light COMMAND css_inventory_light_tests)
  add_executable(css_inventory_backdrop_tests ../tests/inventory_backdrop_tests.cpp)
  target_include_directories(css_inventory_backdrop_tests PRIVATE src)
  add_test(NAME css_inventory_backdrop COMMAND css_inventory_backdrop_tests)
  add_executable(css_frame_profile_tests ../tests/frame_profile_tests.cpp)
  target_include_directories(css_frame_profile_tests PRIVATE src)
  add_test(NAME css_frame_profile COMMAND css_frame_profile_tests)
  return()
endif()
add_library(css_sdk INTERFACE)
file(GLOB first_includes "${CSS_SDK}/deps/first/*/include")
target_include_directories(css_sdk SYSTEM INTERFACE
  "${CSS_SDK}/UE4SS/include" "${CSS_SDK}/UE4SS/generated_include"
  ${first_includes} "${CSS_SDK}/deps/first/Unreal/generated_include"
  "${CSS_SDK}/deps/first/Unreal/include/Unreal"
  "${CSS_SDK}/deps/first/Unreal/include/Unreal/Core"
  "${CSS_SDK}/sdk/third-party/fmt/include"
  "${CSS_SDK}/sdk/third-party/imgui"
  "${CSS_SDK}/sdk/third-party/imguitextedit"
  "${CSS_SDK}/sdk/third-party/zydis/include"
  "${CSS_SDK}/sdk/third-party/zycore/include"
  "${CSS_SDK}/sdk/third-party/polyhook"
  "${CSS_SDK}/sdk/third-party/asmjit/src"
  "${CSS_SDK}/sdk/third-party/asmtk/src"
  "${CSS_SDK}/sdk/third-party/glaze/include")
target_compile_definitions(css_sdk INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN
  RC_UNREAL_DISABLE_CLASS_DEPRECATION_WARNINGS FMT_HEADER_ONLY
  UNICODE _UNICODE UE_GAME UE_BUILD_SHIPPING=1 UBT_COMPILED_PLATFORM=Win64
  OVERRIDE_PLATFORM_HEADER_NAME=Windows PLATFORM_WINDOWS=1 PLATFORM_MICROSOFT=1
  PLATFORM_64BITS=1 WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(css_sdk INTERFACE "${CSS_UE4SS_IMPORT_LIBRARY}")
add_library(css_imgui STATIC
  "${CSS_SDK}/sdk/third-party/imgui/imgui.cpp"
  "${CSS_SDK}/sdk/third-party/imgui/imgui_draw.cpp"
  "${CSS_SDK}/sdk/third-party/imgui/imgui_tables.cpp"
  "${CSS_SDK}/sdk/third-party/imgui/imgui_widgets.cpp")
target_link_libraries(css_imgui PRIVATE css_sdk)
add_library(main SHARED src/loader.cpp src/hook_host.cpp)
target_link_libraries(main PRIVATE css_sdk css_data user32)
target_include_directories(main PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(css_core SHARED src/core.cpp src/engine.cpp "${CSS_SDK}/UE4SS/src/USMapGenerator/Generator.cpp")
option(CSS_TRANSITION_TESTS "Enable developer-only cosmetic reset probe" OFF)
option(CSS_INVENTORY_DEV "Enable local inventory inspection and screenshot commands" OFF)
if(CSS_INVENTORY_DEV)
  target_compile_definitions(css_core PRIVATE CSS_INVENTORY_DEV)
endif()
if(CSS_TRANSITION_TESTS)
  target_compile_definitions(css_core PRIVATE CSS_TRANSITION_TESTS)
endif()
target_compile_definitions(css_core PRIVATE UE4SS_LIB_BUILD_GITSHA="${CSS_UE4SS_RUNTIME_REVISION}")
target_include_directories(css_core PRIVATE include)
target_link_libraries(css_core PRIVATE css_sdk css_data css_imgui css_player_recovery user32)
foreach(target main css_core css_data)
  target_compile_options(${target} PRIVATE /W4 /EHsc /permissive-)
endforeach()
set_target_properties(main css_core PROPERTIES PREFIX "")

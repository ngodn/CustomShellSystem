cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CCS_VERSION LIMIT_COUNT 1)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CCS_NUMERIC_VERSION "${CCS_VERSION}")

project(CCS VERSION ${CCS_NUMERIC_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

get_filename_component(CCS_REPO "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CCS_VENDOR "${CCS_REPO}/native/vendor" CACHE PATH "nlohmann/json sources")
set(CCS_SDK "${CCS_REPO}/reference/ue4ss-sdk-d7e7826d" CACHE PATH "Retained UE4SS header SDK")

file(READ "${CCS_REPO}/native/ue4ss-runtime.json" CCS_UE4SS_RUNTIME_JSON)
string(JSON CCS_UE4SS_REVISION GET "${CCS_UE4SS_RUNTIME_JSON}" revision)
string(JSON CCS_UE4SS_DLL_SHA GET "${CCS_UE4SS_RUNTIME_JSON}" dll_sha256)
string(JSON CCS_UE4SS_IMPORT_RELATIVE GET "${CCS_UE4SS_RUNTIME_JSON}" import_library)
set(CCS_UE4SS_IMPORT_LIBRARY "${CCS_REPO}/${CCS_UE4SS_IMPORT_RELATIVE}" CACHE FILEPATH "Pinned UE4SS runtime import library")

configure_file(src/shared/version.hpp.in generated/ccs_version.hpp @ONLY)

add_library(ccs_runtime STATIC
  src/runtime/common.cpp src/runtime/writer.cpp
  src/runtime/settings.cpp src/runtime/storage.cpp src/runtime/catalog.cpp src/runtime/tab_order.cpp src/runtime/persistence.cpp src/runtime/frame_profile.cpp src/runtime/content_monitor.cpp)
target_include_directories(ccs_runtime PUBLIC src/runtime src/shared "${CCS_VENDOR}")
if(WIN32)
  target_compile_definitions(ccs_runtime PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
find_package(Threads REQUIRED)
target_link_libraries(ccs_runtime PUBLIC Threads::Threads)
if(NOT WIN32)
  enable_testing()
  add_executable(ccs_writer_tests work/takeover/tests/writer_tests.cpp)
  target_link_libraries(ccs_writer_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_writer COMMAND ccs_writer_tests "${CMAKE_CURRENT_BINARY_DIR}/writer-test.log")
  set_tests_properties(ccs_writer PROPERTIES TIMEOUT 5)
  add_executable(ccs_runtime_tests work/takeover/tests/runtime_tests.cpp)
  target_link_libraries(ccs_runtime_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_runtime COMMAND ccs_runtime_tests "${CMAKE_CURRENT_BINARY_DIR}/runtime-test" "${CMAKE_CURRENT_SOURCE_DIR}/data/catalog.json")
  add_executable(ccs_probe_schedule_tests work/takeover/tests/probe_schedule_tests.cpp)
  target_link_libraries(ccs_probe_schedule_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_probe_schedule COMMAND ccs_probe_schedule_tests)
  add_executable(ccs_tab_order_tests work/takeover/tests/tab_order_tests.cpp)
  target_link_libraries(ccs_tab_order_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_tab_order COMMAND ccs_tab_order_tests)
  add_executable(ccs_persistence_tests work/takeover/tests/persistence_tests.cpp)
  target_link_libraries(ccs_persistence_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_persistence COMMAND ccs_persistence_tests "${CMAKE_CURRENT_BINARY_DIR}/persistence-test")
  set_tests_properties(ccs_persistence PROPERTIES TIMEOUT 15)
  add_executable(ccs_input_tests work/takeover/tests/input_tests.cpp)
  target_link_libraries(ccs_input_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_input COMMAND ccs_input_tests)
  add_executable(ccs_window_tests work/takeover/tests/window_tests.cpp)
  target_link_libraries(ccs_window_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_window COMMAND ccs_window_tests)
  add_executable(ccs_menu_layout_tests work/takeover/tests/menu_layout_tests.cpp)
  target_link_libraries(ccs_menu_layout_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_menu_layout COMMAND ccs_menu_layout_tests)
  add_executable(ccs_content_tests work/takeover/tests/content_tests.cpp)
  target_link_libraries(ccs_content_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_content COMMAND ccs_content_tests "${CMAKE_CURRENT_BINARY_DIR}/content-test")
  set_tests_properties(ccs_content PROPERTIES TIMEOUT 15)
  add_executable(ccs_profile_tests work/takeover/tests/profile_tests.cpp)
  target_link_libraries(ccs_profile_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_profile COMMAND ccs_profile_tests "${CMAKE_CURRENT_BINARY_DIR}/profile-test")
  add_executable(ccs_hook_target_tests work/takeover/tests/hook_target_tests.cpp)
  target_link_libraries(ccs_hook_target_tests PRIVATE ccs_runtime)
  add_test(NAME ccs_hook_target COMMAND ccs_hook_target_tests)
  set_tests_properties(ccs_profile PROPERTIES TIMEOUT 15)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME ccs_tools COMMAND ${Python3_EXECUTABLE} -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/work/takeover/tests" -p "test_*.py")
  return()
endif()

option(CCS_EXPERIMENTAL_MENU "Build the unverified CCS menu prototype" OFF)
option(CCS_FRAME_PROFILE "Build F7-triggered diagnostic frame profiling" OFF)
option(CCS_DISCOVERY_PROBE "Build the read-only loaded combat discovery probe" OFF)
option(CCS_ATTACK_PROBE "Build the read-only native montage-task call probe" OFF)
option(CCS_REGISTRY_CONTROLS "Add reflected registry and ability-tag controls to the discovery probe" OFF)
if(CCS_REGISTRY_CONTROLS AND NOT CCS_DISCOVERY_PROBE)
  message(FATAL_ERROR "Registry controls require the discovery probe")
endif()
if(CCS_EXPERIMENTAL_MENU AND CCS_DISCOVERY_PROBE)
  message(FATAL_ERROR "Discovery probe and experimental menu must be built separately")
endif()
if(CCS_ATTACK_PROBE AND (CCS_EXPERIMENTAL_MENU OR CCS_DISCOVERY_PROBE))
  message(FATAL_ERROR "Attack call probe requires a separate build")
endif()

# ---- UE4SS SDK Interface ----
add_library(ccs_sdk INTERFACE)
file(GLOB first_includes "${CCS_SDK}/deps/first/*/include")
target_include_directories(ccs_sdk SYSTEM INTERFACE
  "${CCS_SDK}/UE4SS/include"
  "${CCS_SDK}/UE4SS/generated_include"
  ${first_includes}
  "${CCS_SDK}/deps/first/Unreal/generated_include"
  "${CCS_SDK}/deps/first/Unreal/include/Unreal"
  "${CCS_SDK}/deps/first/Unreal/include/Unreal/Core"
  "${CCS_SDK}/sdk/third-party/fmt/include"
  "${CCS_SDK}/sdk/third-party/imgui"
  "${CCS_SDK}/sdk/third-party/imguitextedit"
  "${CCS_SDK}/sdk/third-party/zydis/include"
  "${CCS_SDK}/sdk/third-party/zycore/include"
  "${CCS_SDK}/sdk/third-party/polyhook"
  "${CCS_SDK}/sdk/third-party/asmjit/src"
  "${CCS_SDK}/sdk/third-party/asmtk/src"
  "${CCS_SDK}/sdk/third-party/glaze/include"
  "${CCS_VENDOR}"
)

target_compile_definitions(ccs_sdk INTERFACE
  NOMINMAX WIN32_LEAN_AND_MEAN
  RC_UNREAL_DISABLE_CLASS_DEPRECATION_WARNINGS
  FMT_HEADER_ONLY
  UNICODE _UNICODE UE_GAME UE_BUILD_SHIPPING=1
  UBT_COMPILED_PLATFORM=Win64
  OVERRIDE_PLATFORM_HEADER_NAME=Windows
  PLATFORM_WINDOWS=1 PLATFORM_MICROSOFT=1
  PLATFORM_64BITS=1 WINVER=0x0A00 _WIN32_WINNT=0x0A00
)

target_link_libraries(ccs_sdk INTERFACE "${CCS_UE4SS_IMPORT_LIBRARY}")

# ---- Loader Target (dlls/main.dll) ----
add_library(main SHARED
  src/loader/loader.cpp
  src/loader/hook_service.cpp
)
target_include_directories(main PRIVATE
  include
  src/shared
  src/loader
  src/runtime
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(main PRIVATE ccs_sdk ccs_runtime user32)
set_target_properties(main PROPERTIES PREFIX "")

# ---- Core Target (core/ccs_core.dll) ----
add_library(ccs_core SHARED
  src/core/engine.cpp
  src/core/menu_input.cpp
  src/core/widget_pool.cpp
  src/core/moveset_manager.cpp
  src/core/menu.cpp
  src/core/menu_page.cpp
  src/core/core.cpp
)

target_include_directories(ccs_core PRIVATE
  include
  src/shared
  src/runtime
  src/core
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(ccs_core PRIVATE ccs_sdk ccs_runtime user32 psapi)
if(CCS_FRAME_PROFILE)
  target_compile_definitions(main PRIVATE CCS_FRAME_PROFILE)
  target_compile_definitions(ccs_core PRIVATE CCS_FRAME_PROFILE)
endif()
if(CCS_EXPERIMENTAL_MENU)
  target_sources(ccs_core PRIVATE src/core/loaded_moves.cpp)
  target_compile_definitions(ccs_core PRIVATE CCS_EXPERIMENTAL_MENU)
endif()
if(CCS_DISCOVERY_PROBE)
  target_sources(ccs_core PRIVATE src/core/discovery_probe.cpp)
  target_compile_definitions(ccs_core PRIVATE CCS_DISCOVERY_PROBE)
endif()
if(CCS_REGISTRY_CONTROLS)
  target_compile_definitions(ccs_core PRIVATE CCS_REGISTRY_CONTROLS)
endif()
if(CCS_ATTACK_PROBE)
  target_sources(ccs_core PRIVATE src/core/attack_probe.cpp)
  target_compile_definitions(ccs_core PRIVATE CCS_ATTACK_PROBE)
endif()
set_target_properties(ccs_core PROPERTIES PREFIX "")

foreach(target main ccs_core ccs_runtime)
  target_compile_options(${target} PRIVATE /W4 /EHsc /permissive-)
endforeach()

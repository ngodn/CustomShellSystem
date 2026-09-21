cmake_minimum_required(VERSION 3.25)
# Standalone CSSX: permanent loader (dlls/main.dll), replaceable runtime
# (core/cssx_core-<version>.dll), the Cheat Menu extension and portable tests.
#
# Host (Linux) configure builds the portable libraries and tests only:
#   cmake -S extensions/core -B extensions/core/build/host && cmake --build extensions/core/build/host && ctest --test-dir extensions/core/build/host
# Windows cross build (clang-cl + xwin, MSVC ABI, pinned UE4SS runtime):
#   cmake -S extensions/core -B extensions/core/build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_TOOLCHAIN_FILE=$PWD/native/toolchain-clang-cl.cmake
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CSSX_VERSION LIMIT_COUNT 1)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CSSX_NUMERIC_VERSION "${CSSX_VERSION}")
project(CSSX VERSION ${CSSX_NUMERIC_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

# Verified build inputs shared with the repository. None of them is a runtime
# dependency on CSS; they are headers, the pinned SDK and the import library.
get_filename_component(CSSX_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CSSX_VENDOR "${CSSX_REPO}/native/vendor" CACHE PATH "nlohmann/json and Lua 5.4 sources")
set(CSSX_SDK "${CSSX_REPO}/reference/ue4ss-sdk-d7e7826d" CACHE PATH "Retained d7e7826d UE4SS header SDK")
file(READ "${CSSX_REPO}/native/ue4ss-runtime.json" CSSX_UE4SS_RUNTIME_JSON)
string(JSON CSSX_UE4SS_REVISION GET "${CSSX_UE4SS_RUNTIME_JSON}" revision)
string(JSON CSSX_UE4SS_DLL_SHA GET "${CSSX_UE4SS_RUNTIME_JSON}" dll_sha256)
string(JSON CSSX_UE4SS_IMPORT_RELATIVE GET "${CSSX_UE4SS_RUNTIME_JSON}" import_library)
set(CSSX_UE4SS_IMPORT_LIBRARY "${CSSX_REPO}/${CSSX_UE4SS_IMPORT_RELATIVE}" CACHE FILEPATH "Pinned UE4SS runtime import library")
configure_file(src/shared/version.hpp.in generated/cssx_version.hpp @ONLY)

option(CSSX_DEV "Developer build: extra probes in the core" OFF)

# ---- portable runtime pieces (compile on Linux for tests and on Windows) ----
add_library(cssx_runtime STATIC
  src/runtime/common.cpp src/runtime/manifest.cpp src/runtime/storage.cpp
  src/runtime/settings.cpp src/runtime/extensions.cpp src/runtime/writer.cpp)
target_include_directories(cssx_runtime PUBLIC include src/runtime src/shared "${CSSX_VENDOR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_target_properties(cssx_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB cssx_lua_sources CONFIGURE_DEPENDS "${CSSX_VENDOR}/lua/*.c")
list(FILTER cssx_lua_sources EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(cssx_lua STATIC ${cssx_lua_sources})
# Lua compiled as C++ so allocation failures unwind host values safely.
set_source_files_properties(${cssx_lua_sources} PROPERTIES LANGUAGE CXX)
target_include_directories(cssx_lua PUBLIC "${CSSX_VENDOR}/lua")
set_target_properties(cssx_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)
target_link_libraries(cssx_runtime PUBLIC cssx_lua ${CMAKE_DL_LIBS} Threads::Threads)

# ---- Cheat Menu gameplay logic (portable, tested against a fake host) ----
file(GLOB cssx_cheat_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cheat-menu/src/*.cpp")
list(FILTER cssx_cheat_sources EXCLUDE REGEX "/main\\.cpp$")
add_library(cssx_cheat_logic STATIC ${cssx_cheat_sources})
target_include_directories(cssx_cheat_logic PUBLIC include cheat-menu/src "${CSSX_VENDOR}")
set_target_properties(cssx_cheat_logic PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(cheat_menu SHARED cheat-menu/src/main.cpp)
target_link_libraries(cheat_menu PRIVATE cssx_cheat_logic)
set_target_properties(cheat_menu PROPERTIES PREFIX "")

# ---- tests (host) ----
if(NOT WIN32)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(cssx_runtime_tests tests/runtime_tests.cpp)
  target_link_libraries(cssx_runtime_tests PRIVATE cssx_runtime)
  add_test(NAME cssx_runtime COMMAND cssx_runtime_tests)
  add_executable(cssx_cheat_tests tests/cheat_menu_tests.cpp)
  target_link_libraries(cssx_cheat_tests PRIVATE cssx_cheat_logic cssx_runtime)
  add_test(NAME cssx_cheat_menu COMMAND cssx_cheat_tests ${CMAKE_CURRENT_SOURCE_DIR}/cheat-menu/menu.json)
  foreach(mode RANGE 1 4)
    add_library(cssx_fixture_${mode} SHARED tests/extension_fixture.cpp)
    target_include_directories(cssx_fixture_${mode} PRIVATE include)
    target_compile_definitions(cssx_fixture_${mode} PRIVATE CSSX_TEST_MODE=${mode})
  endforeach()
  add_library(cssx_runtime_shared SHARED tests/runtime_shared.cpp)
  target_link_libraries(cssx_runtime_shared PRIVATE cssx_runtime)
  add_test(NAME cssx_extension_lifecycle COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/lifecycle_check.py
    $<TARGET_FILE:cssx_runtime_shared> $<TARGET_FILE:cssx_fixture_1> $<TARGET_FILE:cssx_fixture_2> $<TARGET_FILE:cssx_fixture_3> $<TARGET_FILE:cssx_fixture_4>
    ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  add_test(NAME cssx_tools COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p "test_*.py")
  return()
endif()

# ---- Windows: UE4SS-facing DLLs ----
add_library(cssx_sdk INTERFACE)
file(GLOB first_includes "${CSSX_SDK}/deps/first/*/include")
target_include_directories(cssx_sdk SYSTEM INTERFACE
  "${CSSX_SDK}/UE4SS/include" "${CSSX_SDK}/UE4SS/generated_include"
  ${first_includes} "${CSSX_SDK}/deps/first/Unreal/generated_include"
  "${CSSX_SDK}/deps/first/Unreal/include/Unreal"
  "${CSSX_SDK}/deps/first/Unreal/include/Unreal/Core"
  "${CSSX_SDK}/sdk/third-party/fmt/include"
  "${CSSX_SDK}/sdk/third-party/imgui"
  "${CSSX_SDK}/sdk/third-party/imguitextedit"
  "${CSSX_SDK}/sdk/third-party/zydis/include"
  "${CSSX_SDK}/sdk/third-party/zycore/include"
  "${CSSX_SDK}/sdk/third-party/polyhook"
  "${CSSX_SDK}/sdk/third-party/asmjit/src"
  "${CSSX_SDK}/sdk/third-party/asmtk/src"
  "${CSSX_SDK}/sdk/third-party/glaze/include")
target_compile_definitions(cssx_sdk INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN
  RC_UNREAL_DISABLE_CLASS_DEPRECATION_WARNINGS FMT_HEADER_ONLY
  UNICODE _UNICODE UE_GAME UE_BUILD_SHIPPING=1 UBT_COMPILED_PLATFORM=Win64
  OVERRIDE_PLATFORM_HEADER_NAME=Windows PLATFORM_WINDOWS=1 PLATFORM_MICROSOFT=1
  PLATFORM_64BITS=1 WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(cssx_sdk INTERFACE "${CSSX_UE4SS_IMPORT_LIBRARY}")

add_library(main SHARED src/loader/loader.cpp src/loader/hook_service.cpp)
target_include_directories(main PRIVATE src/shared src/runtime "${CSSX_VENDOR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(main PRIVATE cssx_sdk cssx_runtime user32)

add_library(cssx_core SHARED
  src/core/core.cpp src/core/engine.cpp src/core/bridge.cpp src/core/hud.cpp src/core/menu.cpp src/core/devchannel.cpp)
target_include_directories(cssx_core PRIVATE src/core src/shared "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(cssx_core PRIVATE cssx_sdk cssx_runtime user32 psapi)
if(CSSX_DEV)
  target_compile_definitions(cssx_core PRIVATE CSSX_DEV)
endif()
foreach(target main cssx_core cssx_runtime cheat_menu)
  target_compile_options(${target} PRIVATE /W4 /EHsc /permissive-)
endforeach()
set_target_properties(main cssx_core PROPERTIES PREFIX "")

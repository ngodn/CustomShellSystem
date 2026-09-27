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
)
target_include_directories(main PRIVATE
  include
  src/shared
  src/loader
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(main PRIVATE ccs_sdk user32)
set_target_properties(main PROPERTIES PREFIX "")

# ---- Core Target (core/ccs_core.dll) ----
add_library(ccs_core SHARED
  src/runtime/common.cpp
  src/runtime/writer.cpp
  src/runtime/settings.cpp
  src/runtime/storage.cpp
  src/core/engine.cpp
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

target_link_libraries(ccs_core PRIVATE ccs_sdk user32 psapi)
set_target_properties(ccs_core PROPERTIES PREFIX "")

foreach(target main ccs_core)
  target_compile_options(${target} PRIVATE /W4 /EHsc /permissive-)
endforeach()

cmake_minimum_required(VERSION 3.25)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" CSS_VERSION LIMIT_COUNT 1)
project(CustomShellSystem VERSION ${CSS_VERSION} LANGUAGES CXX)
configure_file(src/version.hpp.in generated/css_version.hpp @ONLY)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CSS_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../reference/ue4ss-sdk-d7e7826d" CACHE PATH "Exact UE4SS SDK")
add_library(css_data STATIC src/data.cpp src/packages.cpp src/colors.cpp)
target_include_directories(css_data PUBLIC src vendor)
if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(css_data PUBLIC OpenSSL::Crypto)
  enable_testing()
  add_executable(css_tests ../tests/data_tests.cpp)
  target_link_libraries(css_tests PRIVATE css_data)
  add_test(NAME css_data COMMAND css_tests)
  add_executable(css_color_tests ../tests/color_tests.cpp)
  target_link_libraries(css_color_tests PRIVATE css_data)
  add_test(NAME css_colors COMMAND css_color_tests)
  add_executable(css_package_tests ../tests/package_tests.cpp)
  target_link_libraries(css_package_tests PRIVATE css_data)
  add_executable(css_startup_tests ../tests/startup_tests.cpp)
  target_link_libraries(css_startup_tests PRIVATE css_data)
  add_test(NAME css_startup COMMAND css_startup_tests)
  add_executable(css_recovery_tests ../tests/recovery_tests.cpp)
  target_include_directories(css_recovery_tests PRIVATE src)
  add_test(NAME css_recovery COMMAND css_recovery_tests)
  return()
endif()
target_link_libraries(css_data PUBLIC bcrypt)
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
target_link_libraries(css_sdk INTERFACE "${CSS_SDK}/sdk/lib/UE4SS.lib")
add_library(css_imgui STATIC
  "${CSS_SDK}/sdk/third-party/imgui/imgui.cpp"
  "${CSS_SDK}/sdk/third-party/imgui/imgui_draw.cpp"
  "${CSS_SDK}/sdk/third-party/imgui/imgui_tables.cpp"
  "${CSS_SDK}/sdk/third-party/imgui/imgui_widgets.cpp")
target_link_libraries(css_imgui PRIVATE css_sdk)
add_library(main SHARED src/loader.cpp)
target_link_libraries(main PRIVATE css_sdk css_data user32)
target_include_directories(main PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(css_core SHARED src/core.cpp src/engine.cpp "${CSS_SDK}/UE4SS/src/USMapGenerator/Generator.cpp")
option(CSS_TRANSITION_TESTS "Enable developer-only cosmetic reset probe" OFF)
if(CSS_TRANSITION_TESTS)
  target_compile_definitions(css_core PRIVATE CSS_TRANSITION_TESTS)
endif()
target_compile_definitions(css_core PRIVATE UE4SS_LIB_BUILD_GITSHA="d7e7826d")
target_link_libraries(css_core PRIVATE css_sdk css_data css_imgui user32 xinput9_1_0)
foreach(target main css_core css_data)
  target_compile_options(${target} PRIVATE /W4 /EHsc /permissive-)
endforeach()
set_target_properties(main css_core PROPERTIES PREFIX "")

cmake_minimum_required(VERSION 3.25)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" WCMG_VERSION LIMIT_COUNT 1)
project(WCMG VERSION ${WCMG_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Windows build: configure with the CSS cross toolchain,
#   cmake -S WCMG -B WCMG/build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_TOOLCHAIN_FILE=../native/toolchain-clang-cl.cmake
# Linux build (tests + CLI only): cmake -S WCMG -B WCMG/build/linux -G Ninja

configure_file(src/core/version.hpp.in generated/wcmg_version.hpp @ONLY)
# The default knowledge file travels inside the exe as a raw string literal.
file(READ src/core/knowledge.json WCMG_KNOWLEDGE_JSON)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/wcmg_knowledge_embedded.hpp"
     "#pragma once\nstatic const char* const kEmbeddedKnowledge = R\"wcmgjson(${WCMG_KNOWLEDGE_JSON})wcmgjson\";\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/core/knowledge.json)

# Portable core: every scanner, the rules, the report, the zip bundle.
add_library(wcmg_core STATIC
    src/core/util.cpp
    src/core/knowledge.cpp
    src/core/crash_context.cpp
    src/core/minidump.cpp
    src/core/ue4ss.cpp
    src/core/lua_scan.cpp
    src/core/our_mods.cpp
    src/core/paks.cpp
    src/core/manifest.cpp
    src/core/system.cpp
    src/core/discovery.cpp
    src/core/rules.cpp
    src/core/report.cpp
    src/core/collect.cpp
    src/core/diagnose.cpp
    vendor/miniz/miniz.c)
target_include_directories(wcmg_core PUBLIC src/core vendor "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(wcmg_core PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
if(WIN32)
    # Static CRT: one exe, nothing to install.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_definitions(wcmg_core PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_options(wcmg_core PUBLIC /EHsc /utf-8 /W3)
    target_link_libraries(wcmg_core PUBLIC advapi32 shell32)
else()
    target_compile_options(wcmg_core PUBLIC -Wall -Wextra)
endif()

# Console runner, both platforms: wcmg_cli <game dir> [--json] [--zip out.zip]
add_executable(wcmg_cli src/cli/main.cpp)
target_link_libraries(wcmg_cli PRIVATE wcmg_core)
if(WIN32)
    # Registry and Epic discovery for the console build too.
    target_sources(wcmg_cli PRIVATE src/win/discovery_win.cpp)
    target_include_directories(wcmg_cli PRIVATE src/win)
    target_link_libraries(wcmg_cli PRIVATE user32 ole32 shell32 shlwapi)
endif()

if(WIN32)
    add_executable(wcmg WIN32
        src/win/main.cpp
        src/win/gui.cpp
        src/win/discovery_win.cpp
        src/win/wcmg.rc)
    target_include_directories(wcmg PRIVATE src/win)
    target_link_libraries(wcmg PRIVATE wcmg_core user32 gdi32 comctl32 comdlg32 ole32 shlwapi uxtheme)
    set_target_properties(wcmg PROPERTIES OUTPUT_NAME "WCMG")
    # Common Controls 6 + per-monitor DPI come from the manifest in wcmg.rc.
    target_link_options(wcmg PRIVATE /MANIFEST:NO)
endif()

if(NOT WIN32)
    enable_testing()
    foreach(t crash_context minidump ue4ss paks manifest rules discovery)
        add_executable(wcmg_${t}_tests tests/${t}_tests.cpp)
        target_link_libraries(wcmg_${t}_tests PRIVATE wcmg_core)
        add_test(NAME ${t} COMMAND wcmg_${t}_tests)
    endforeach()
endif()

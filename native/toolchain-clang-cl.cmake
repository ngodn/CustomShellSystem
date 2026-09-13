# Cross-compile Linux -> Windows x64 with the MSVC ABI.
#
# Why the MSVC ABI and not mingw: this DLL calls the game's own C++ member
# functions and reads its structs. Mortal Shell II and its UE4SS runtime are
# both MSVC builds. mingw agrees with MSVC on the x64 calling convention, so the
# old pure-C input bridge worked, but it does not agree on C++ details, and this
# mod goes further than passing key codes.
#
# The Windows headers and import libraries come from xwin:
#     cargo install xwin
#     xwin --accept-license --arch x86_64 splat --output ~/.xwin
#
# Override with -DXWIN_DIR=/path if yours lives elsewhere.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT DEFINED XWIN_DIR)
    set(XWIN_DIR "$ENV{HOME}/.xwin")
endif()

if(NOT EXISTS "${XWIN_DIR}/crt/include")
    message(FATAL_ERROR
        "no Windows SDK at ${XWIN_DIR}. Run:\n"
        "  cargo install xwin && xwin --accept-license --arch x86_64 splat --output ${XWIN_DIR}")
endif()

set(CMAKE_C_COMPILER   clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER       lld-link)
set(CMAKE_RC_COMPILER  llvm-rc)
set(CMAKE_MT           llvm-mt)

# clang-cl needs an explicit target when it is not running on Windows.
set(_target "--target=x86_64-pc-windows-msvc")

# /imsvc marks these as system includes, which suppresses the warnings the
# Windows SDK produces under clang's stricter defaults.
set(_includes
    "/imsvc${XWIN_DIR}/crt/include"
    "/imsvc${XWIN_DIR}/sdk/include/ucrt"
    "/imsvc${XWIN_DIR}/sdk/include/um"
    "/imsvc${XWIN_DIR}/sdk/include/shared")
list(JOIN _includes " " _includes_flat)

set(CMAKE_C_FLAGS_INIT   "${_target} ${_includes_flat}")
set(CMAKE_CXX_FLAGS_INIT "${_target} ${_includes_flat}")

# xwin ships only the release CRT, so the debug runtime does not exist here and
# CMake's default /MDd makes even the compiler check fail with a missing
# msvcrtd.lib. Use the release CRT for toolchain checks; the CSS targets use /MD.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)

set(_libpaths
    "/libpath:${XWIN_DIR}/crt/lib/x86_64"
    "/libpath:${XWIN_DIR}/sdk/lib/ucrt/x86_64"
    "/libpath:${XWIN_DIR}/sdk/lib/um/x86_64")
list(JOIN _libpaths " " _libpaths_flat)

set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_libpaths_flat}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_libpaths_flat}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_libpaths_flat}")

# Only look on the host for programs; never for headers or libraries.
set(CMAKE_FIND_ROOT_PATH "${XWIN_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

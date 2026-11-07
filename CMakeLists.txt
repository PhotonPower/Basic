cmake_minimum_required(VERSION 3.16)

project(BasicCompiler
        VERSION 0.0.1
        DESCRIPTION "GFA-BASIC 3.5 compiler for MC68000 (Amiga), Z80 (Amstrad CPC) and C++ (PC)"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BASIC_BUILD_TESTS "Build the unit tests" ON)
option(BASIC_WITH_EMULATORS "Fetch the sister projects (Z80, MC68000) for assembling and emulator tests" ON)
option(BASIC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BASIC_SANITIZE "Build with AddressSanitizer and UBSan (GCC/Clang)" OFF)

function(basic_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(BASIC_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
    if(BASIC_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(BASIC_SANITIZE)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

# ---- sister projects (assemblers, emulators) -----------------------------------------
# Pinned to a commit so that a change there never breaks this build unnoticed. To work on a
# local checkout instead, configure with
#   -DFETCHCONTENT_SOURCE_DIR_Z80TOOLS=../Z80 -DFETCHCONTENT_SOURCE_DIR_M68KTOOLS=../MC68000
# See docs/toolchain.md.
if(BASIC_WITH_EMULATORS)
  include(FetchContent)
  set(Z80_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(M68K_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(z80tools
    GIT_REPOSITORY https://github.com/PhotonPower/Z80.git
    GIT_TAG        e74cf002e300e55dd55511b22e3f545fae5821a6)
  FetchContent_Declare(m68ktools
    GIT_REPOSITORY https://github.com/PhotonPower/MC68000.git
    GIT_TAG        0074b8b4da269398d44253853cec5501d5a9ea60)
  FetchContent_MakeAvailable(z80tools m68ktools)
endif()

# ---- C reference runtime ----------------------------------------------------------------
add_subdirectory(runtime/ref)

# ---- core library ---------------------------------------------------------------------
# Frontend, IR, backends and the runtime linker are added here as they are written
# (see docs/architecture.md and docs/roadmap.md).
add_library(basiccore STATIC
  src/target.cpp
  src/frontend/lexer.cpp
  src/frontend/parser.cpp
  src/frontend/ast_dump.cpp
  src/frontend/builtins.cpp)
target_include_directories(basiccore PUBLIC include src)
target_compile_features(basiccore PUBLIC cxx_std_20)
basic_set_warnings(basiccore)

# ---- command line tool ----------------------------------------------------------------
add_executable(basicc app/main.cpp)
target_link_libraries(basicc PRIVATE basiccore)
basic_set_warnings(basicc)

install(TARGETS basicc RUNTIME DESTINATION bin)

# ---- tests ----------------------------------------------------------------------------
if(BASIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

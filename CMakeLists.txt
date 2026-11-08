cmake_minimum_required(VERSION 3.16)

project(BasicCompiler
        VERSION 0.6.0
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
    GIT_TAG        f41f822c08051b7ed33da783dda36094cdaa043a)
  FetchContent_MakeAvailable(z80tools m68ktools)
endif()

# ---- C reference runtime ----------------------------------------------------------------
add_subdirectory(runtime/ref)

# ---- C++ runtime layer of the pc target (also used by the reference interpreter) -------------
add_library(basic_rt_pc STATIC platform/pc/rt_pc.cpp platform/pc/rt_gfx.cpp platform/pc/rt_gui.cpp)
target_include_directories(basic_rt_pc PUBLIC platform/pc)
target_link_libraries(basic_rt_pc PUBLIC basic_rt_ref)
target_compile_features(basic_rt_pc PUBLIC cxx_std_17)
basic_set_warnings(basic_rt_pc)

# ---- runtime modules built into the compiler ---------------------------------------------
set(BASIC_MODULE_DIRS runtime/m68k platform/m68k-test platform/amiga runtime/z80 platform/z80-test platform/cpc)
string(REPLACE ";" "," BASIC_MODULE_DIRS_ARG "${BASIC_MODULE_DIRS}")
set(BASIC_MODULE_FILES "")
foreach(dir ${BASIC_MODULE_DIRS})
  file(GLOB files CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/${dir}/*.s)
  list(APPEND BASIC_MODULE_FILES ${files})
endforeach()
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_modules.cpp
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -DDIRS=${BASIC_MODULE_DIRS_ARG}
          -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/embedded_modules.cpp -P ${PROJECT_SOURCE_DIR}/cmake/embed_modules.cmake
  DEPENDS ${BASIC_MODULE_FILES} ${PROJECT_SOURCE_DIR}/cmake/embed_modules.cmake
  COMMENT "Embedding runtime modules")

# ---- core library ---------------------------------------------------------------------
# Frontend, IR, backends and the runtime linker are added here as they are written
# (see docs/architecture.md and docs/roadmap.md).
add_library(basiccore STATIC
  src/target.cpp
  src/frontend/lexer.cpp
  src/frontend/parser.cpp
  src/frontend/ast_dump.cpp
  src/frontend/builtins.cpp
  src/frontend/sema.cpp
  src/ir/hir.cpp
  src/ir/interp.cpp
  src/backend/cpp/cpp_backend.cpp
  src/link/linker.cpp
  src/ir/lir.cpp
  src/ir/lower.cpp
  src/backend/m68k/m68k_backend.cpp
  src/backend/z80/z80_backend.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/embedded_modules.cpp)
target_include_directories(basiccore PUBLIC include src)
target_link_libraries(basiccore PUBLIC basic_rt_pc)
target_compile_features(basiccore PUBLIC cxx_std_20)
basic_set_warnings(basiccore)

# ---- command line tool ----------------------------------------------------------------
add_executable(basicc app/main.cpp)
target_link_libraries(basicc PRIVATE basiccore)
if(BASIC_WITH_EMULATORS)  # amiga, cpc: assemble with the sister projects' assemblers
  # cpc snapshots: the real firmware booted in the emulator of PhotonPower/Z80 (needs a ROM image)
  add_library(basic_cpcsna STATIC src/backend/z80/cpc_snapshot.cpp)
  target_include_directories(basic_cpcsna PUBLIC src)
  target_link_libraries(basic_cpcsna PUBLIC z80core)
  basic_set_warnings(basic_cpcsna)
  target_link_libraries(basicc PRIVATE basic_cpcsna)
  target_link_libraries(basicc PRIVATE m68kcore z80core)
  target_compile_definitions(basicc PRIVATE BASIC_HAVE_M68K_ASSEMBLER BASIC_HAVE_Z80_ASSEMBLER)
endif()
basic_set_warnings(basicc)

install(TARGETS basicc RUNTIME DESTINATION bin)

# ---- tests ----------------------------------------------------------------------------
if(BASIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

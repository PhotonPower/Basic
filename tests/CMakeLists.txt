# Unit tests (no external framework, see test_util.hpp).
foreach(name targets frontend programs)
  add_executable(test_${name} test_${name}.cpp)
  target_link_libraries(test_${name} PRIVATE basiccore)
  basic_set_warnings(test_${name})
  add_test(NAME ${name} COMMAND test_${name})
endforeach()
target_compile_definitions(test_frontend PRIVATE BASIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(test_programs PRIVATE BASIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# C reference runtime (runtime/ref) against the host's IEEE arithmetic and the original's results.
add_executable(test_runtime_ref test_runtime_ref.cpp)
target_link_libraries(test_runtime_ref PRIVATE basic_rt_ref)
basic_set_warnings(test_runtime_ref)
add_test(NAME runtime_ref COMMAND test_runtime_ref)

# Graphics of platform/pc (D25) against the rasters measured on the original (probe22-24).
add_executable(test_gfx test_gfx.cpp)
target_link_libraries(test_gfx PRIVATE basic_rt_pc)
target_compile_definitions(test_gfx PRIVATE BASIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
basic_set_warnings(test_gfx)
add_test(NAME gfx COMMAND test_gfx)

# Differential test of the pc target: every tests/programs/<name>.lst is compiled to C++ with
# basicc, built, run, and its output compared with <name>.out (made on the original GFA-BASIC).
file(GLOB BASIC_TEST_PROGRAMS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/programs/*.lst)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pc)
foreach(program ${BASIC_TEST_PROGRAMS})
  get_filename_component(name ${program} NAME_WE)
  set(generated ${CMAKE_CURRENT_BINARY_DIR}/pc/${name}.cpp)
  add_custom_command(OUTPUT ${generated}
                     COMMAND basicc --target pc ${program} -o ${generated}
                     DEPENDS basicc ${program}
                     COMMENT "basicc --target pc ${name}.lst")
  add_executable(pc_${name} ${generated})
  target_link_libraries(pc_${name} PRIVATE basic_rt_pc)
  if(BASIC_SANITIZE AND NOT MSVC)
    target_compile_options(pc_${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(pc_${name} PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME pc_${name}
           COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:pc_${name}>
                   -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.out
                   -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.in
                   -DERRFILE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.err
                   -DGFXFILE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.gfx
                   -DGFXOUT=${CMAKE_CURRENT_BINARY_DIR}/pc/${name}.screen
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/compare_output.cmake)
endforeach()

# Emulator harness: runs generated code in the emulators of the sister projects (docs/testing.md).
if(BASIC_WITH_EMULATORS)
  add_library(basic_harness STATIC
    harness/z80_runner.cpp
    harness/m68k_runner.cpp
    harness/m68k_routines.cpp)
  target_include_directories(basic_harness PUBLIC harness)
  target_link_libraries(basic_harness PUBLIC z80core m68kcore)
  basic_set_warnings(basic_harness)

  foreach(name harness_z80 harness_m68k)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE basic_harness)
    basic_set_warnings(test_${name})
    add_test(NAME ${name} COMMAND test_${name})
  endforeach()

  # MC68000 runtime against runtime/ref (docs/testing.md#konformanz-der-runtime)
  add_executable(test_conformance_m68k test_conformance_m68k.cpp)
  target_link_libraries(test_conformance_m68k PRIVATE basic_harness basiccore basic_rt_ref)
  target_compile_definitions(test_conformance_m68k PRIVATE BASIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  basic_set_warnings(test_conformance_m68k)
  add_test(NAME conformance_m68k COMMAND test_conformance_m68k)

  # Programs compiled for m68k-test against the outputs of the original
  add_executable(test_m68k_programs test_m68k_programs.cpp)
  target_link_libraries(test_m68k_programs PRIVATE basic_harness basiccore)
  target_compile_definitions(test_m68k_programs PRIVATE BASIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  basic_set_warnings(test_m68k_programs)
  add_test(NAME m68k_programs COMMAND test_m68k_programs)
endif()

# Tools written in Python (standard library only); skipped without Python 3.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME tools_gfa_run
           COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_*.py")
endif()

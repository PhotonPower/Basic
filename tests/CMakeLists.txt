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
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/compare_output.cmake)
endforeach()

# Emulator harness: runs generated code in the emulators of the sister projects (docs/testing.md).
if(BASIC_WITH_EMULATORS)
  add_library(basic_harness STATIC
    harness/z80_runner.cpp
    harness/m68k_runner.cpp)
  target_include_directories(basic_harness PUBLIC harness)
  target_link_libraries(basic_harness PUBLIC z80core m68kcore)
  basic_set_warnings(basic_harness)

  foreach(name harness_z80 harness_m68k)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE basic_harness)
    basic_set_warnings(test_${name})
    add_test(NAME ${name} COMMAND test_${name})
  endforeach()
endif()

# Tools written in Python (standard library only); skipped without Python 3.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME tools_gfa_run
           COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_*.py")
endif()

# Unit tests (no external framework, see test_util.hpp).
foreach(name targets)
  add_executable(test_${name} test_${name}.cpp)
  target_link_libraries(test_${name} PRIVATE basiccore)
  basic_set_warnings(test_${name})
  add_test(NAME ${name} COMMAND test_${name})
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

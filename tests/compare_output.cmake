# Runs EXE and compares its standard output with the file EXPECTED (differential test of the pc
# target, docs/testing.md).
execute_process(COMMAND ${EXE} OUTPUT_VARIABLE actual ERROR_VARIABLE errors RESULT_VARIABLE status)
file(READ ${EXPECTED} expected)
if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "output differs from ${EXPECTED}\n--- expected\n${expected}--- actual\n${actual}${errors}")
endif()
if(NOT status EQUAL 0)
  message(FATAL_ERROR "exit status ${status}\n${errors}")
endif()

if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE OR NOT DEFINED EXPECTED)
  message(FATAL_ERROR "ROCKETC, SOURCE, and EXPECTED are required")
endif()

execute_process(
  COMMAND "${ROCKETC}" run "${SOURCE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)

set(combined "${output}${error}")
if(result EQUAL 0)
  message(FATAL_ERROR "program unexpectedly succeeded:\n${combined}")
endif()
string(FIND "${combined}" "${EXPECTED}" position)
if(position EQUAL -1)
  message(FATAL_ERROR "expected runtime diagnostic '${EXPECTED}', got:\n${combined}")
endif()

message(STATUS "observed expected runtime failure: ${EXPECTED}")

execute_process(
  COMMAND "${ROCKETC}" check "${SOURCE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)

if(result EQUAL 0)
  message(FATAL_ERROR "expected compilation to fail, but it succeeded")
endif()

set(combined "${output}${error}")
string(FIND "${combined}" "${EXPECTED}" found)
if(found EQUAL -1)
  message(FATAL_ERROR "expected compiler diagnostic '${EXPECTED}', got:\n${combined}")
endif()

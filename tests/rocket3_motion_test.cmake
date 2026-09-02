if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP13 motion test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/artifacts")
file(WRITE "${WORK}/motion-format.rocket"
  "import  rocket.motion\nfn main()->Int:\n    let value=motion.linear(progress:0.5)\n    return 0\n")
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/motion-format.rocket"
  RESULT_VARIABLE format_write_result OUTPUT_VARIABLE format_write_output ERROR_VARIABLE format_write_error)
if(NOT format_write_result EQUAL 0)
  message(FATAL_ERROR "WP13 motion formatter write failed:\n${format_write_output}${format_write_error}")
endif()
file(READ "${WORK}/motion-format.rocket" formatted)
set(expected_format
  "import rocket.motion\nfn main() -> Int:\n    let value = motion.linear(progress: 0.5)\n    return 0\n")
if(NOT formatted STREQUAL expected_format)
  message(FATAL_ERROR "WP13 motion formatter output was not canonical:\n${formatted}")
endif()
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/motion-format.rocket" --check
  RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP13 motion formatter failed:\n${format_output}${format_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_motion_named.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE named_result OUTPUT_VARIABLE named_output ERROR_VARIABLE named_error)
if(NOT named_result EQUAL 0)
  message(FATAL_ERROR "WP13 motion named surface failed:\n${named_output}${named_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" build "${SOURCE_DIR}/tests/fixtures/rocket3_motion.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "WP13 motion build failed:\n${build_output}${build_error}")
endif()
set(executable "${WORK}/artifacts/rocket3_motion/.rocketc/targets/windows-x64/rocket3_motion.exe")
execute_process(COMMAND "${executable}" WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
string(REPLACE "\r\n" "\n" normalized_output "${run_output}")
set(expected_output "")
foreach(index RANGE 1 20)
  string(APPEND expected_output "1\n")
endforeach()
if(NOT run_result EQUAL 0 OR NOT normalized_output STREQUAL expected_output)
  message(FATAL_ERROR "WP13 motion output failed:\n${run_output}${run_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_motion_wrong_type_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE failure_result OUTPUT_VARIABLE failure_output ERROR_VARIABLE failure_error)
if(failure_result EQUAL 0 OR NOT "${failure_output}${failure_error}" MATCHES "error\\[R4001\\]")
  message(FATAL_ERROR "WP13 motion type diagnostic failed:\n${failure_output}${failure_error}")
endif()
message(STATUS "WP13 motion matrix passed")

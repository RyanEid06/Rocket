if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP10 named-argument test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/artifacts" "${WORK}/format")

function(run_success input expected)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
      "${ROCKETC}" build "${input}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
  if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "named-argument build failed for ${input}:\n${build_output}${build_error}")
  endif()
  if(IS_DIRECTORY "${input}")
    set(executable
      "${WORK}/artifacts/rocket3_named_arguments/.rocketc/targets/windows-x64/main.exe")
  else()
    set(executable
      "${WORK}/artifacts/rocket3_named_arguments/.rocketc/targets/windows-x64/rocket3_named_arguments.exe")
  endif()
  execute_process(
    COMMAND "${executable}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "named-argument run failed for ${input}:\n${output}${error}")
  endif()
  string(REPLACE "\r\n" "\n" normalized "${output}")
  if(NOT normalized STREQUAL "${expected}")
    message(FATAL_ERROR "unexpected named-argument output for ${input}:\n${output}${error}")
  endif()
endfunction()

run_success("${SOURCE_DIR}/tests/fixtures/rocket3_named_arguments.rocket"
  "2\n1\n4\n3\n34\n56\n31\n")
run_success("${SOURCE_DIR}/tests/fixtures/rocket3_named_arguments_package"
  "12\n")

set(failures
  "rocket3_named_positional_after_named_failure.rocket|R2001|positional argument cannot follow a named argument"
  "rocket3_named_unknown_failure.rocket|R4002|unknown named argument 'rght'\; did you mean 'right'?"
  "rocket3_named_duplicate_failure.rocket|R4005|duplicate named argument 'left'"
  "rocket3_named_missing_failure.rocket|R4005|missing required argument 'right'"
  "rocket3_named_conflict_failure.rocket|R4005|argument 'left' is already supplied positionally"
  "rocket3_named_wrong_type_failure.rocket|R4001|argument 'left' has type String, expected Int")

foreach(case IN LISTS failures)
  string(REPLACE "|" ";" fields "${case}")
  list(GET fields 0 fixture)
  list(GET fields 1 code)
  list(GET fields 2 message)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
      "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/${fixture}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(result EQUAL 0)
    message(FATAL_ERROR "expected ${fixture} to fail")
  endif()
  set(combined "${output}${error}")
  string(FIND "${combined}" "error[${code}]: ${message}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "unexpected diagnostic for ${fixture}:\n${combined}")
  endif()
endforeach()

file(WRITE "${WORK}/format/named.rocket"
  "fn combine(left:Int,right:Int)->Int:\n    return left+right\nfn main()->Int:\n    return combine( right :2,left: 1 )\n")
execute_process(
  COMMAND "${ROCKETC}" fmt "${WORK}/format/named.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE format_result
  OUTPUT_VARIABLE format_output
  ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "named-argument formatter failed:\n${format_output}${format_error}")
endif()
file(READ "${WORK}/format/named.rocket" formatted)
set(expected_format
  "fn combine(left: Int, right: Int) -> Int:\n    return left + right\nfn main() -> Int:\n    return combine(right: 2, left: 1)\n")
if(NOT formatted STREQUAL expected_format)
  message(FATAL_ERROR "named-argument formatter output was not canonical:\n${formatted}")
endif()

execute_process(
  COMMAND "${ROCKETC}" doc
    "${SOURCE_DIR}/tests/fixtures/rocket3_named_arguments_package"
    --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output
  ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "named-argument documentation generation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
string(FIND "${documentation_index}" "\"parameters\": [\"left\", \"right\"]" parameters_found)
if(parameters_found EQUAL -1)
  message(FATAL_ERROR "documentation metadata omitted public parameter names:\n${documentation_index}")
endif()

message(STATUS "WP10 named arguments matrix passed")

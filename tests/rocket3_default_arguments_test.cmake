if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP11 default-argument test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/artifacts" "${WORK}/format")

function(run_success input expected executable_name)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
      "${ROCKETC}" build "${input}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
  if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "default-argument build failed for ${input}:\n${build_output}${build_error}")
  endif()
  set(executable
    "${WORK}/artifacts/rocket3_default_arguments/.rocketc/targets/windows-x64/${executable_name}.exe")
  execute_process(
    COMMAND "${executable}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "default-argument run failed for ${input}:\n${output}${error}")
  endif()
  string(REPLACE "\r\n" "\n" normalized "${output}")
  if(NOT normalized STREQUAL "${expected}")
    message(FATAL_ERROR "unexpected default-argument output for ${input}:\n${output}${error}")
  endif()
endfunction()

run_success("${SOURCE_DIR}/tests/fixtures/rocket3_default_arguments.rocket"
  "9\n1\n2\n129\n5\n6\n456\n789\n16\n18\n11\n12\n"
  "rocket3_default_arguments")
run_success("${SOURCE_DIR}/tests/fixtures/rocket3_default_arguments_package"
  "356\n469\n" "main")

set(failures
  "rocket3_default_required_after_default_failure.rocket|R4005|required parameter 'second' cannot follow a defaulted parameter"
  "rocket3_default_later_parameter_failure.rocket|R4002|undefined name 'second'"
  "rocket3_default_wrong_type_failure.rocket|R4001|default argument for 'value' has type String, expected Int"
  "rocket3_default_missing_required_failure.rocket|R4005|missing required argument 'left'"
  "rocket3_default_lambda_failure.rocket|R4005|default arguments are not supported for lambda parameters"
  "rocket3_default_callback_failure.rocket|R4005|default arguments are not supported for callback parameters"
  "rocket3_default_trait_failure.rocket|R4005|default arguments are not supported for trait-declaration parameters"
  "rocket3_default_enum_failure.rocket|R4005|default arguments are not supported for enum payloads"
  "rocket3_default_extern_failure.rocket|R4005|default arguments are not supported for extern parameters"
  "rocket3_default_struct_field_failure.rocket|R4005|default arguments are not supported for struct fields")

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

file(WRITE "${WORK}/format/defaults.rocket"
  "fn choose(first:Int,second:Int=first+1)->Int:\n    return second\n")
execute_process(
  COMMAND "${ROCKETC}" fmt "${WORK}/format/defaults.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE format_result
  OUTPUT_VARIABLE format_output
  ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "default-argument formatter failed:\n${format_output}${format_error}")
endif()
file(READ "${WORK}/format/defaults.rocket" formatted)
set(expected_format
  "fn choose(first: Int, second: Int = first + 1) -> Int:\n    return second\n")
if(NOT formatted STREQUAL expected_format)
  message(FATAL_ERROR "default-argument formatter output was not canonical:\n${formatted}")
endif()

execute_process(
  COMMAND "${ROCKETC}" doc
    "${SOURCE_DIR}/tests/fixtures/rocket3_default_arguments_package"
    --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output
  ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "default-argument documentation generation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
string(FIND "${documentation_index}"
  "pub fn score(base: Int, bonus: Int = offset(base), scale: Int = bonus + 1) -> Int:"
  declaration_found)
if(declaration_found EQUAL -1)
  message(FATAL_ERROR "documentation omitted public default expressions:\n${documentation_index}")
endif()
string(FIND "${documentation_index}"
  "\"defaults\": [null, \"offset(base)\", \"bonus + 1\"]"
  defaults_found)
if(defaults_found EQUAL -1)
  message(FATAL_ERROR "documentation metadata omitted public defaults:\n${documentation_index}")
endif()

message(STATUS "WP11 default arguments matrix passed")

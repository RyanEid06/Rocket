if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP11A named-callable-parity test is missing required arguments")
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
    message(FATAL_ERROR "named-callable-parity build failed for ${input}:\n${build_output}${build_error}")
  endif()
  set(executable
    "${WORK}/artifacts/rocket3_named_callable_parity/.rocketc/targets/windows-x64/${executable_name}.exe")
  execute_process(
    COMMAND "${executable}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "named-callable-parity run failed for ${input}:\n${output}${error}")
  endif()
  string(REPLACE "\r\n" "\n" normalized "${output}")
  if(NOT normalized STREQUAL "${expected}")
    message(FATAL_ERROR "unexpected named-callable-parity output for ${input}:\n${output}${error}")
  endif()
endfunction()

run_success("${SOURCE_DIR}/tests/fixtures/rocket3_named_callable_parity.rocket"
  "12\n34\n6\n5\n56\n78\n12\n34\n10\n9\n100\n122\nab\ncd\n14\n13\nef\ngh\n15\n15\np\n16\nq\n17\n19\n18\nr\n18\ns\n20\nlegacy\n21\n"
  "rocket3_named_callable_parity")
run_success("${SOURCE_DIR}/tests/fixtures/rocket3_named_callable_parity_package"
  "package\n12\n" "main")

set(failures
  "rocket3_named_closure_unknown_failure.rocket|R4002|unknown named argument 'rght'\; did you mean 'right'?"
  "rocket3_named_closure_duplicate_failure.rocket|R4005|duplicate named argument 'left'"
  "rocket3_named_closure_missing_failure.rocket|R4005|missing required argument 'right'"
  "rocket3_named_closure_conflict_failure.rocket|R4005|argument 'left' is already supplied positionally"
  "rocket3_named_closure_wrong_type_failure.rocket|R4001|argument 'left' has type String, expected Int"
  "rocket3_named_standard_unknown_failure.rocket|R4002|unknown named argument 'rght'\; did you mean 'right'?"
  "rocket3_named_standard_duplicate_failure.rocket|R4005|duplicate named argument 'left'"
  "rocket3_named_standard_missing_failure.rocket|R4005|missing required argument 'right'"
  "rocket3_named_standard_conflict_failure.rocket|R4005|argument 'left' is already supplied positionally"
  "rocket3_named_standard_wrong_type_failure.rocket|R4001|argument 'left' has type Int, expected String"
  "rocket3_named_builtin_unknown_failure.rocket|R4002|unknown named argument 'vlue'\; did you mean 'value'?"
  "rocket3_named_builtin_duplicate_failure.rocket|R4005|duplicate named argument 'value'"
  "rocket3_named_builtin_missing_failure.rocket|R4005|missing required argument 'value'"
  "rocket3_named_builtin_conflict_failure.rocket|R4005|argument 'value' is already supplied positionally"
  "rocket3_named_enum_unknown_failure.rocket|R4002|unknown named argument 'lbel'\; did you mean 'label'?"
  "rocket3_named_enum_duplicate_failure.rocket|R4005|duplicate named argument 'amount'"
  "rocket3_named_enum_missing_failure.rocket|R4005|missing required argument 'label'"
  "rocket3_named_enum_conflict_failure.rocket|R4005|argument 'amount' is already supplied positionally"
  "rocket3_named_enum_wrong_type_failure.rocket|R4001|argument 'amount' has type String, expected Int"
  "rocket3_named_enum_anonymous_failure.rocket|R4005|named arguments are not supported for anonymous enum payloads"
  "rocket3_named_enum_mixed_payload_failure.rocket|R4005|enum variant 'Value' cannot mix labeled and anonymous payload entries")

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

file(WRITE "${WORK}/format/callables.rocket"
  "enum Choice:\n    Value(amount:Int,label:String)\nfn main()->Int:\n    let pair=fn(left:Int,right:Int)->Int=>left+right\n    print(value:pair(right:2,left:1))\n")
execute_process(
  COMMAND "${ROCKETC}" fmt "${WORK}/format/callables.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE format_result
  OUTPUT_VARIABLE format_output
  ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "named-callable-parity formatter failed:\n${format_output}${format_error}")
endif()
file(READ "${WORK}/format/callables.rocket" formatted)
set(expected_format
  "enum Choice:\n    Value(amount: Int, label: String)\nfn main() -> Int:\n    let pair = fn(left: Int, right: Int) -> Int => left + right\n    print(value: pair(right: 2, left: 1))\n")
if(NOT formatted STREQUAL expected_format)
  message(FATAL_ERROR "named-callable-parity formatter output was not canonical:\n${formatted}")
endif()

execute_process(
  COMMAND "${ROCKETC}" doc
    "${SOURCE_DIR}/tests/fixtures/rocket3_named_callable_parity_package"
    --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output
  ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "named-callable-parity documentation generation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
string(FIND "${documentation_index}"
  "\"payloadLabels\": [\"amount\", \"label\"]"
  payload_labels_found)
if(payload_labels_found EQUAL -1)
  message(FATAL_ERROR "documentation metadata omitted public enum payload labels:\n${documentation_index}")
endif()
string(FIND "${documentation_index}"
  "\"name\": \"std.string.concat\", \"parameters\": [\"left\", \"right\"]"
  intrinsic_names_found)
if(intrinsic_names_found EQUAL -1)
  message(FATAL_ERROR "documentation metadata omitted standard intrinsic parameter names:\n${documentation_index}")
endif()
string(FIND "${documentation_index}"
  "\"name\": \"print\", \"parameters\": [\"value\"]"
  builtin_names_found)
if(builtin_names_found EQUAL -1)
  message(FATAL_ERROR "documentation metadata omitted built-in parameter names:\n${documentation_index}")
endif()

message(STATUS "WP11A named callable parity matrix passed")

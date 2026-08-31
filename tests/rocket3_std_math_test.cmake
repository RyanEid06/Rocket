if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP12 std.math test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/artifacts")
file(WRITE "${WORK}/math-format.rocket"
  "import  std.math\nfn main()->Int:\n    let value=math.clamp(maximum:1.0,value:0.5,minimum:0.0)\n    return 0\n")
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/math-format.rocket"
  RESULT_VARIABLE format_write_result
  OUTPUT_VARIABLE format_write_output ERROR_VARIABLE format_write_error)
if(NOT format_write_result EQUAL 0)
  message(FATAL_ERROR "WP12 std.math formatter write failed:\n${format_write_output}${format_write_error}")
endif()
file(READ "${WORK}/math-format.rocket" formatted)
set(expected_format
  "import std.math\nfn main() -> Int:\n    let value = math.clamp(maximum: 1.0, value: 0.5, minimum: 0.0)\n    return 0\n")
if(NOT formatted STREQUAL expected_format)
  message(FATAL_ERROR "WP12 std.math formatter output was not canonical:\n${formatted}")
endif()
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/math-format.rocket" --check
  RESULT_VARIABLE format_result
  OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP12 std.math formatter failed:\n${format_output}${format_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check
  "${SOURCE_DIR}/tests/fixtures/rocket3_std_math_named.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE named_result
  OUTPUT_VARIABLE named_output ERROR_VARIABLE named_error)
if(NOT named_result EQUAL 0)
  message(FATAL_ERROR "WP12 std.math named surface failed:\n${named_output}${named_error}")
endif()
execute_process(COMMAND "${ROCKETC}" doc
  "${SOURCE_DIR}/tests/fixtures/rocket3_std_math_package" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP12 std.math documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
set(expected_math_callables
  "\"name\": \"std.math.pi\", \"parameters\": []"
  "\"name\": \"std.math.tau\", \"parameters\": []"
  "\"name\": \"std.math.e\", \"parameters\": []"
  "\"name\": \"std.math.abs\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.abs_int\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.min\", \"parameters\": [\"left\", \"right\"]"
  "\"name\": \"std.math.max\", \"parameters\": [\"left\", \"right\"]"
  "\"name\": \"std.math.min_int\", \"parameters\": [\"left\", \"right\"]"
  "\"name\": \"std.math.max_int\", \"parameters\": [\"left\", \"right\"]"
  "\"name\": \"std.math.clamp\", \"parameters\": [\"value\", \"minimum\", \"maximum\"]"
  "\"name\": \"std.math.clamp_int\", \"parameters\": [\"value\", \"minimum\", \"maximum\"]"
  "\"name\": \"std.math.sign\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.sign_int\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.floor\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.ceil\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.round\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.trunc\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.fract\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.sqrt\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.pow\", \"parameters\": [\"base\", \"exponent\"]"
  "\"name\": \"std.math.exp\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.log\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.log10\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.sin\", \"parameters\": [\"radians\"]"
  "\"name\": \"std.math.cos\", \"parameters\": [\"radians\"]"
  "\"name\": \"std.math.tan\", \"parameters\": [\"radians\"]"
  "\"name\": \"std.math.asin\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.acos\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.atan\", \"parameters\": [\"value\"]"
  "\"name\": \"std.math.atan2\", \"parameters\": [\"y\", \"x\"]"
  "\"name\": \"std.math.radians\", \"parameters\": [\"degrees\"]"
  "\"name\": \"std.math.degrees\", \"parameters\": [\"radians\"]"
  "\"name\": \"std.math.lerp\", \"parameters\": [\"start\", \"end\", \"progress\"]"
  "\"name\": \"std.math.inverse_lerp\", \"parameters\": [\"start\", \"end\", \"value\"]"
  "\"name\": \"std.math.remap\", \"parameters\": [\"input_start\", \"input_end\", \"output_start\", \"output_end\", \"value\"]"
  "\"name\": \"std.math.smoothstep\", \"parameters\": [\"start\", \"end\", \"value\"]"
  "\"name\": \"std.math.smootherstep\", \"parameters\": [\"start\", \"end\", \"value\"]"
  "\"name\": \"std.math.approach\", \"parameters\": [\"current\", \"target\", \"maximum_delta\"]"
  "\"name\": \"std.math.move_towards\", \"parameters\": [\"current\", \"target\", \"maximum_delta\"]")
foreach(expected_callable IN LISTS expected_math_callables)
  string(FIND "${documentation_index}" "${expected_callable}" callable_found)
  if(callable_found EQUAL -1)
    message(FATAL_ERROR "WP12 std.math documentation metadata omitted ${expected_callable}:\n${documentation_index}")
  endif()
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
    "${ROCKETC}" build "${SOURCE_DIR}/tests/fixtures/rocket3_std_math.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "WP12 std.math build failed:\n${build_output}${build_error}")
endif()
set(executable "${WORK}/artifacts/rocket3_std_math/.rocketc/targets/windows-x64/rocket3_std_math.exe")
execute_process(COMMAND "${executable}" WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
string(REPLACE "\r\n" "\n" normalized_output "${run_output}")
set(expected_output "")
foreach(index RANGE 1 76)
  string(APPEND expected_output "1\n")
endforeach()
if(NOT run_result EQUAL 0 OR NOT normalized_output STREQUAL expected_output)
  message(FATAL_ERROR "WP12 std.math output failed:\n${run_output}${run_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_std_math_wrong_type_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE failure_result OUTPUT_VARIABLE failure_output ERROR_VARIABLE failure_error)
if(failure_result EQUAL 0 OR NOT "${failure_output}${failure_error}" MATCHES "error\\[R4001\\]")
  message(FATAL_ERROR "WP12 std.math type diagnostic failed:\n${failure_output}${failure_error}")
endif()
message(STATUS "WP12 std.math matrix passed")

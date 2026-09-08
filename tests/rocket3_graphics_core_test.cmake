if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP19 graphics core test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/artifacts")
file(WRITE "${WORK}/graphics-format.rocket"
  "import  rocket.graphics\nfn main()->Int:\n    let value=graphics.color_from_rgba(alpha:1.0,blue:0.0,green:0.0,red:1.0)\n    return 0\n")
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/graphics-format.rocket"
  RESULT_VARIABLE format_write_result
  OUTPUT_VARIABLE format_write_output ERROR_VARIABLE format_write_error)
if(NOT format_write_result EQUAL 0)
  message(FATAL_ERROR "WP19 graphics formatter write failed:\n${format_write_output}${format_write_error}")
endif()
file(READ "${WORK}/graphics-format.rocket" formatted)
set(expected_format
  "import rocket.graphics\nfn main() -> Int:\n    let value = graphics.color_from_rgba(alpha: 1.0, blue: 0.0, green: 0.0, red: 1.0)\n    return 0\n")
if(NOT formatted STREQUAL expected_format)
  message(FATAL_ERROR "WP19 graphics formatter output was not canonical:\n${formatted}")
endif()
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/graphics-format.rocket" --check
  RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP19 graphics formatter failed:\n${format_output}${format_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_graphics_core_named.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE named_result
  OUTPUT_VARIABLE named_output ERROR_VARIABLE named_error)
if(NOT named_result EQUAL 0)
  message(FATAL_ERROR "WP19 graphics named surface failed:\n${named_output}${named_error}")
endif()
execute_process(COMMAND "${ROCKETC}" doc
  "${SOURCE_DIR}/tests/fixtures/rocket3_graphics_core_package" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP19 graphics documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
string(FIND "${documentation_index}" "\"name\": \"sample_color\", \"kind\": \"fn\"" sample_found)
string(FIND "${documentation_index}" "\"parameters\": [\"red\", \"green\", \"blue\", \"alpha\"]" parameters_found)
string(FIND "${documentation_index}" "\"defaults\": [null, null, null, \"1.0\"]" defaults_found)
if(sample_found EQUAL -1 OR parameters_found EQUAL -1 OR defaults_found EQUAL -1)
  message(FATAL_ERROR "WP19 graphics package documentation omitted public parameter/default metadata:\n${documentation_index}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
    "${ROCKETC}" build "${SOURCE_DIR}/tests/fixtures/rocket3_graphics_core.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "WP19 graphics build failed:\n${build_output}${build_error}")
endif()
set(executable "${WORK}/artifacts/rocket3_graphics_core/.rocketc/targets/windows-x64/rocket3_graphics_core.exe")
execute_process(COMMAND "${executable}" WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
string(REPLACE "\r\n" "\n" normalized_output "${run_output}")
set(expected_output "")
foreach(index RANGE 1 23)
  string(APPEND expected_output "1\n")
endforeach()
if(NOT run_result EQUAL 0 OR NOT normalized_output STREQUAL expected_output)
  message(FATAL_ERROR "WP19 graphics output failed:\n${run_output}${run_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_graphics_core_wrong_type_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE failure_result
  OUTPUT_VARIABLE failure_output ERROR_VARIABLE failure_error)
if(failure_result EQUAL 0 OR NOT "${failure_output}${failure_error}" MATCHES "error\\[R4001\\]")
  message(FATAL_ERROR "WP19 graphics type diagnostic failed:\n${failure_output}${failure_error}")
endif()
message(STATUS "WP19 graphics core matrix passed")

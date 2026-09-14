if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP22 typography test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/examples/raylib_showcase")
set(public_package "${SOURCE_DIR}/tests/fixtures/rocket3_typography_public_package")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/artifacts")

execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP22 typography package check failed:\n${check_output}${check_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
    "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
    "${ROCKETC}" test "${package}" --filter typography
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE test_result OUTPUT_VARIABLE test_output ERROR_VARIABLE test_error)
if(NOT test_result EQUAL 0 OR
   NOT "${test_output}${test_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP22 typography Rocket test failed:\n${test_output}${test_error}")
endif()

execute_process(COMMAND "${ROCKETC}" check "${public_package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE public_check_result OUTPUT_VARIABLE public_check_output ERROR_VARIABLE public_check_error)
if(NOT public_check_result EQUAL 0)
  message(FATAL_ERROR "WP22 canonical typography package check failed:\n${public_check_output}${public_check_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
    "ROCKET_ARTIFACT_ROOT=${WORK}/public-artifacts"
    "${ROCKETC}" test "${public_package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE public_test_result OUTPUT_VARIABLE public_test_output ERROR_VARIABLE public_test_error)
if(NOT public_test_result EQUAL 0 OR
   NOT "${public_test_output}${public_test_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP22 canonical typography test failed:\n${public_test_output}${public_test_error}")
endif()

foreach(source IN ITEMS
    "${SOURCE_DIR}/stdlib/rocket/graphics.rocket"
    "${SOURCE_DIR}/stdlib/rocket/raylib/native.rocket"
    "${SOURCE_DIR}/stdlib/rocket/raylib/safe.rocket"
    "${package}/src/rocket_raylib.rocket"
    "${package}/src/rocket_raylib_adapter.rocket"
    "${package}/src/rocket_raylib_testing.rocket"
    "${package}/tests/typography_test.rocket"
    "${public_package}/src/main.rocket"
    "${public_package}/testing.rocket"
    "${public_package}/tests/public_api_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${source}" --check
    RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
  if(NOT format_result EQUAL 0)
    message(FATAL_ERROR "WP22 typography formatter failed for ${source}:\n${format_output}${format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" doc "${package}" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE doc_result OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP22 typography documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
string(FIND "${documentation_index}" "\"name\": \"measure_text\", \"kind\": \"fn\"" measure_found)
string(FIND "${documentation_index}" "\"parameters\": [\"font\", \"text\", \"style\", \"max_width\", \"max_height\", \"wrap\", \"overflow\"]" measure_parameters_found)
string(FIND "${documentation_index}" "\"defaults\": [null, null, null, \"0.0\", \"0.0\", \"false\", \"0\"]" measure_defaults_found)
string(FIND "${documentation_index}" "\"name\": \"draw_text_layout\", \"kind\": \"fn\"" draw_found)
if(measure_found EQUAL -1 OR measure_parameters_found EQUAL -1 OR
   measure_defaults_found EQUAL -1 OR draw_found EQUAL -1)
  message(FATAL_ERROR "WP22 typography docs/search omitted public metadata:\n${documentation_index}")
endif()

message(STATUS "WP22 typography stage/compiler surface passed")

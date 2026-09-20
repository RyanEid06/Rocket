if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP30 bounded-state test is missing required arguments")
endif()

set(ui_package "${SOURCE_DIR}/tests/fixtures/rocket3_bounded_state_package")
set(asset_package "${SOURCE_DIR}/examples/raylib_showcase")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/ui-docs" "${WORK}/asset-docs")

foreach(source IN ITEMS
    "stdlib/rocket/ui.rocket"
    "stdlib/rocket/raylib/safe.rocket"
    "examples/raylib_showcase/src/rocket_assets.rocket"
    "examples/raylib_showcase/src/rocket_raylib.rocket"
    "stdlib/rocket/raylib/native.rocket"
    "examples/raylib_showcase/tests/asset_store_test.rocket"
    "tests/fixtures/rocket3_bounded_state_surface.rocket"
    "tests/fixtures/rocket3_bounded_state_package/src/main.rocket"
    "tests/fixtures/rocket3_bounded_state_package/src/wp30_adapter_testing.rocket"
    "tests/fixtures/rocket3_bounded_state_package/tests/bounded_state_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${SOURCE_DIR}/${source}" --check
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE format_result
    OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
  if(NOT format_result EQUAL 0)
    message(FATAL_ERROR "WP30 formatter failed for ${source}:\n${format_output}${format_error}")
  endif()
endforeach()

foreach(package IN ITEMS "${ui_package}" "${asset_package}")
  execute_process(COMMAND "${ROCKETC}" check "${package}"
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
  if(NOT check_result EQUAL 0)
    message(FATAL_ERROR "WP30 package check failed for ${package}:\n${check_output}${check_error}")
  endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/ui-artifacts"
  "${ROCKETC}" test "${ui_package}"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE ui_test_result
  OUTPUT_VARIABLE ui_test_output ERROR_VARIABLE ui_test_error)
if(NOT ui_test_result EQUAL 0 OR
   NOT "${ui_test_output}${ui_test_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP30 100,000-ID bounded-state stress failed:\n${ui_test_output}${ui_test_error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/asset-artifacts"
  "${ROCKETC}" test "${asset_package}" --filter asset_store
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE asset_test_result
  OUTPUT_VARIABLE asset_test_output ERROR_VARIABLE asset_test_error)
if(NOT asset_test_result EQUAL 0 OR
   NOT "${asset_test_output}${asset_test_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP30 bounded asset-store test failed:\n${asset_test_output}${asset_test_error}")
endif()

execute_process(COMMAND "${ROCKETC}" doc "${ui_package}" --output "${WORK}/ui-docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE ui_doc_result
  OUTPUT_VARIABLE ui_doc_output ERROR_VARIABLE ui_doc_error)
execute_process(COMMAND "${ROCKETC}" doc "${asset_package}" --output "${WORK}/asset-docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE asset_doc_result
  OUTPUT_VARIABLE asset_doc_output ERROR_VARIABLE asset_doc_error)
if(NOT ui_doc_result EQUAL 0 OR NOT asset_doc_result EQUAL 0)
  message(FATAL_ERROR "WP30 documentation generation failed:\n${ui_doc_output}${ui_doc_error}${asset_doc_output}${asset_doc_error}")
endif()
file(READ "${WORK}/ui-docs/search.json" ui_index)
file(READ "${WORK}/asset-docs/search.json" asset_index)
foreach(symbol IN ITEMS calibrated_context retained_count)
  if(NOT ui_index MATCHES "\"name\": \"${symbol}\", \"kind\": \"fn\"")
    message(FATAL_ERROR "WP30 UI documentation omitted ${symbol}")
  endif()
endforeach()
string(FIND "${asset_index}" "\"name\": \"capacity\", \"kind\": \"fn\"" capacity_found)
string(FIND "${asset_index}" "\"parameters\": [\"window\", \"audio\", \"package_root\", \"capacity\"]" open_parameters_found)
string(FIND "${asset_index}" [=["defaults": [null, null, "\".\"", "256"]]=] open_defaults_found)
if(capacity_found EQUAL -1 OR open_parameters_found EQUAL -1 OR open_defaults_found EQUAL -1)
  message(FATAL_ERROR "WP30 asset documentation omitted bounded-capacity metadata")
endif()

foreach(document IN ITEMS docs/BOOK.md docs/SPEC.md docs/STDLIB.md docs/ROCKET_3_0_SYNTAX_DICTIONARY.md)
  file(READ "${SOURCE_DIR}/${document}" document_text)
  if(NOT document_text MATCHES "(2,048|2048)" OR NOT document_text MATCHES "256")
    message(FATAL_ERROR "WP30 calibrated bounds are missing from ${document}")
  endif()
endforeach()

set(calibration_record
  "${SOURCE_DIR}/docs/ROCKET_3_0_WAVE_C_CALIBRATION.json")
if(NOT EXISTS "${calibration_record}")
  message(FATAL_ERROR
    "WP30 calibration evidence is missing: ${calibration_record}")
endif()
file(READ "${calibration_record}" calibration_json)
string(JSON calibration_schema ERROR_VARIABLE calibration_error
  GET "${calibration_json}" schema)
if(calibration_error OR
   NOT calibration_schema STREQUAL "rocket3-wave-c-capacity-calibration-v1")
  message(FATAL_ERROR "WP30 calibration evidence has an invalid schema")
endif()
function(check_calibration_value field expected)
  string(JSON observed ERROR_VARIABLE calibration_error
    GET "${calibration_json}" frozen_values "${field}")
  if(calibration_error OR NOT observed EQUAL expected)
    message(FATAL_ERROR
      "WP30 calibration evidence has ${field}=${observed}; expected ${expected}")
  endif()
endfunction()
check_calibration_value(ui_context_capacity 2048)
check_calibration_value(unseen_retention_frames 8)
check_calibration_value(measurement_cache_capacity 256)
check_calibration_value(asset_store_default_capacity 256)
string(JSON sample_count ERROR_VARIABLE calibration_error
  GET "${calibration_json}" method samples_per_workload)
if(calibration_error OR sample_count LESS 7)
  message(FATAL_ERROR "WP30 calibration evidence requires at least 7 samples")
endif()
foreach(workload IN ITEMS ui_default_and_100k_churn native_cache_and_cleanup)
  string(JSON outcome ERROR_VARIABLE calibration_error
    GET "${calibration_json}" workloads "${workload}" outcome)
  string(JSON sample_length ERROR_VARIABLE sample_error
    LENGTH "${calibration_json}" workloads "${workload}" elapsed_ms)
  if(calibration_error OR sample_error OR
     NOT outcome STREQUAL "passed" OR sample_length LESS 7)
    message(FATAL_ERROR
      "WP30 calibration evidence is incomplete for ${workload}")
  endif()
endforeach()

message(STATUS "WP30 bounded UI/resource/measurement-cache matrix passed")

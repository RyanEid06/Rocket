if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP25 UI-theme test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

foreach(source IN ITEMS
    "${SOURCE_DIR}/stdlib/rocket/ui/theme.rocket"
    "${SOURCE_DIR}/stdlib/rocket/ui/styles.rocket"
    "${SOURCE_DIR}/tests/fixtures/rocket3_ui_theme_surface.rocket"
    "${SOURCE_DIR}/tests/fixtures/rocket3_ui_theme_package/src/main.rocket"
    "${SOURCE_DIR}/tests/fixtures/rocket3_ui_theme_package/tests/theme_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${source}" --check
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE source_format_result
    OUTPUT_VARIABLE source_format_output ERROR_VARIABLE source_format_error)
  if(NOT source_format_result EQUAL 0)
    message(FATAL_ERROR "WP25 source format check failed for ${source}:\n${source_format_output}${source_format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_ui_theme_surface.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE surface_result
  OUTPUT_VARIABLE surface_output ERROR_VARIABLE surface_error)
if(NOT surface_result EQUAL 0)
  message(FATAL_ERROR "WP25 public UI-theme surface failed:\n${surface_output}${surface_error}")
endif()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_ui_theme_wrong_type_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE wrong_type_result
  OUTPUT_VARIABLE wrong_type_output ERROR_VARIABLE wrong_type_error)
if(wrong_type_result EQUAL 0 OR NOT "${wrong_type_output}${wrong_type_error}" MATCHES "error\\[R4001\\].*Vec2, expected Float")
  message(FATAL_ERROR "WP25 typed UI-theme diagnostic failed:\n${wrong_type_output}${wrong_type_error}")
endif()

execute_process(COMMAND "${ROCKETC}" doc "${SOURCE_DIR}/tests/fixtures/rocket3_ui_theme_package" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP25 package documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
foreach(symbol IN ITEMS sample_theme sample_styles sample_resolve)
  string(FIND "${documentation_index}" "\"name\": \"${symbol}\", \"kind\": \"fn\"" symbol_found)
  if(symbol_found EQUAL -1)
    message(FATAL_ERROR "WP25 documentation omitted ${symbol}:\n${documentation_index}")
  endif()
endforeach()
string(FIND "${documentation_index}" "\"parameters\": [\"value\"]" styles_parameters_found)
string(FIND "${documentation_index}" "\"parameters\": [\"values\", \"state\"]" resolve_parameters_found)
if(styles_parameters_found EQUAL -1 OR resolve_parameters_found EQUAL -1)
  message(FATAL_ERROR "WP25 documentation omitted named-parameter metadata:\n${documentation_index}")
endif()

foreach(document IN ITEMS docs/BOOK.md docs/SPEC.md docs/STDLIB.md docs/ROCKET_3_0_SYNTAX_DICTIONARY.md)
  file(READ "${SOURCE_DIR}/${document}" document_text)
  if(NOT document_text MATCHES "rocket.ui.theme" OR NOT document_text MATCHES "rocket.ui.styles")
    message(FATAL_ERROR "WP25 documentation omitted public theme/style modules in ${document}")
  endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" test "${SOURCE_DIR}/tests/fixtures/rocket3_ui_theme_package"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0 OR NOT "${package_output}${package_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP25 deterministic package test failed:\n${package_output}${package_error}")
endif()

message(STATUS "WP25 public UI theme/style matrix passed")

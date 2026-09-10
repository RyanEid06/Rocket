if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP23 UI-context test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/docs")
file(WRITE "${WORK}/ui-format.rocket"
  "import  rocket.ui\nimport rocket.graphics\nfn main()->Int:\n    let result=ui.new_context(namespace:\"main\",capacity:32)\n    return 0\n")
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/ui-format.rocket"
  RESULT_VARIABLE format_write_result OUTPUT_VARIABLE format_write_output ERROR_VARIABLE format_write_error)
if(NOT format_write_result EQUAL 0)
  message(FATAL_ERROR "WP23 formatter write failed:\n${format_write_output}${format_write_error}")
endif()
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/ui-format.rocket" --check
  RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP23 formatter check failed:\n${format_output}${format_error}")
endif()

foreach(source IN ITEMS
    "stdlib/rocket/ui.rocket"
    "stdlib/rocket/graphics.rocket"
    "stdlib/rocket/graphics/input.rocket"
    "stdlib/rocket/raylib/safe.rocket"
    "tests/fixtures/rocket3_ui_context_surface.rocket"
    "tests/fixtures/rocket3_ui_context_wrong_type_failure.rocket"
    "tests/fixtures/rocket3_ui_context_package/src/main.rocket"
    "tests/fixtures/rocket3_ui_context_package/src/wp23_adapter_testing.rocket"
    "tests/fixtures/rocket3_ui_context_package/tests/ui_context_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${SOURCE_DIR}/${source}" --check
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE source_format_result
    OUTPUT_VARIABLE source_format_output ERROR_VARIABLE source_format_error)
  if(NOT source_format_result EQUAL 0)
    message(FATAL_ERROR "WP23 source format check failed for ${source}:\n${source_format_output}${source_format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_ui_context_surface.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE surface_result
  OUTPUT_VARIABLE surface_output ERROR_VARIABLE surface_error)
if(NOT surface_result EQUAL 0)
  message(FATAL_ERROR "WP23 public UI-context surface failed:\n${surface_output}${surface_error}")
endif()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_ui_context_wrong_type_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE wrong_type_result
  OUTPUT_VARIABLE wrong_type_output ERROR_VARIABLE wrong_type_error)
if(wrong_type_result EQUAL 0 OR NOT "${wrong_type_output}${wrong_type_error}" MATCHES "error\\[R4001\\].*Vec2, expected rocket.raylib.safe.Window")
  message(FATAL_ERROR "WP23 typed UI-context diagnostic failed:\n${wrong_type_output}${wrong_type_error}")
endif()

execute_process(COMMAND "${ROCKETC}" doc "${SOURCE_DIR}/tests/fixtures/rocket3_ui_context_package" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP23 package documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
foreach(symbol IN ITEMS sample_context sample_id sample_interact)
  string(FIND "${documentation_index}" "\"name\": \"${symbol}\", \"kind\": \"fn\"" symbol_found)
  if(symbol_found EQUAL -1)
    message(FATAL_ERROR "WP23 documentation omitted ${symbol}:\n${documentation_index}")
  endif()
endforeach()
string(FIND "${documentation_index}" "\"parameters\": [\"namespace\", \"capacity\"]" context_parameters_found)
string(FIND "${documentation_index}" "\"parameters\": [\"parent\", \"child\"]" id_parameters_found)
string(FIND "${documentation_index}" "\"parameters\": [\"frame\", \"id\", \"bounds\", \"disabled\"]" interact_parameters_found)
if(context_parameters_found EQUAL -1 OR id_parameters_found EQUAL -1 OR interact_parameters_found EQUAL -1)
  message(FATAL_ERROR "WP23 documentation omitted named-parameter metadata:\n${documentation_index}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" test "${SOURCE_DIR}/tests/fixtures/rocket3_ui_context_package"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0 OR NOT "${package_output}${package_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP23 deterministic package test failed:\n${package_output}${package_error}")
endif()

message(STATUS "WP23 public UI context/interaction matrix passed")

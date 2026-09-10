if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP24 UI-layout test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/docs")
file(WRITE "${WORK}/layout-format.rocket"
  "import  rocket.ui.layout\nimport rocket.graphics\nfn main()->Int:\n    let bounds=graphics.rect(0.0,0.0,100.0,100.0)\n    let item=layout.layout_item(layout.fill(),layout.fill(),graphics.size(0.0,0.0),layout.insets_all(0.0),layout.center())\n    let result=layout.anchor_rect(bounds,item,layout.insets_all(0.0),layout.no_safe_area())\n    return 0\n")
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/layout-format.rocket"
  RESULT_VARIABLE format_write_result OUTPUT_VARIABLE format_write_output ERROR_VARIABLE format_write_error)
if(NOT format_write_result EQUAL 0)
  message(FATAL_ERROR "WP24 formatter write failed:\n${format_write_output}${format_write_error}")
endif()
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/layout-format.rocket" --check
  RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP24 formatter check failed:\n${format_output}${format_error}")
endif()

foreach(source IN ITEMS
    "stdlib/rocket/ui/layout.rocket"
    "tests/fixtures/rocket3_ui_layout_surface.rocket"
    "tests/fixtures/rocket3_ui_layout_wrong_type_failure.rocket"
    "tests/fixtures/rocket3_ui_layout_package/src/main.rocket"
    "tests/fixtures/rocket3_ui_layout_package/tests/layout_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${SOURCE_DIR}/${source}" --check
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE source_format_result
    OUTPUT_VARIABLE source_format_output ERROR_VARIABLE source_format_error)
  if(NOT source_format_result EQUAL 0)
    message(FATAL_ERROR "WP24 source format check failed for ${source}:\n${source_format_output}${source_format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_ui_layout_surface.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE surface_result
  OUTPUT_VARIABLE surface_output ERROR_VARIABLE surface_error)
if(NOT surface_result EQUAL 0)
  message(FATAL_ERROR "WP24 public UI-layout surface failed:\n${surface_output}${surface_error}")
endif()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_ui_layout_wrong_type_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE wrong_type_result
  OUTPUT_VARIABLE wrong_type_output ERROR_VARIABLE wrong_type_error)
if(wrong_type_result EQUAL 0 OR NOT "${wrong_type_output}${wrong_type_error}" MATCHES "error\\[R4001\\].*Rect, expected rocket.ui.layout.Insets")
  message(FATAL_ERROR "WP24 typed UI-layout diagnostic failed:\n${wrong_type_output}${wrong_type_error}")
endif()

execute_process(COMMAND "${ROCKETC}" doc "${SOURCE_DIR}/tests/fixtures/rocket3_ui_layout_package" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP24 package documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
foreach(symbol IN ITEMS sample_row sample_grid sample_anchor)
  string(FIND "${documentation_index}" "\"name\": \"${symbol}\", \"kind\": \"fn\"" symbol_found)
  if(symbol_found EQUAL -1)
    message(FATAL_ERROR "WP24 documentation omitted ${symbol}:\n${documentation_index}")
  endif()
endforeach()
string(FIND "${documentation_index}" "\"parameters\": [\"bounds\", \"items\", \"gap\", \"padding\", \"safe_area\", \"horizontal\", \"vertical\"]" row_parameters_found)
string(FIND "${documentation_index}" "\"parameters\": [\"bounds\", \"columns\", \"rows\", \"items\", \"column_gap\", \"row_gap\", \"padding\", \"safe_area\", \"horizontal\", \"vertical\"]" grid_parameters_found)
string(FIND "${documentation_index}" "\"parameters\": [\"bounds\", \"item\", \"padding\", \"safe_area\"]" anchor_parameters_found)
if(row_parameters_found EQUAL -1 OR grid_parameters_found EQUAL -1 OR anchor_parameters_found EQUAL -1)
  message(FATAL_ERROR "WP24 documentation omitted named-parameter metadata:\n${documentation_index}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" test "${SOURCE_DIR}/tests/fixtures/rocket3_ui_layout_package"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0 OR NOT "${package_output}${package_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP24 deterministic package test failed:\n${package_output}${package_error}")
endif()

message(STATUS "WP24 public UI layout matrix passed")

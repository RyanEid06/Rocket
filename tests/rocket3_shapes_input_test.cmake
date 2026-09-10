if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP20 shapes/input test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/docs")
file(WRITE "${WORK}/shapes-format.rocket"
  "import  rocket.graphics\nimport rocket.graphics.shapes\nimport rocket.raylib.safe\nfn main()->Int:\n    let frame=safe.Frame(1)\n    let value=shapes.draw_rect_outline(frame:frame,bounds:graphics.rect(0.0,0.0,10.0,10.0),color:graphics.color_from_rgba(1.0,0.0,0.0),thickness:2.0)\n    return 0\n")
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/shapes-format.rocket"
  RESULT_VARIABLE format_write_result OUTPUT_VARIABLE format_write_output ERROR_VARIABLE format_write_error)
if(NOT format_write_result EQUAL 0)
  message(FATAL_ERROR "WP20 formatter write failed:\n${format_write_output}${format_write_error}")
endif()
execute_process(COMMAND "${ROCKETC}" fmt "${WORK}/shapes-format.rocket" --check
  RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP20 formatter check failed:\n${format_output}${format_error}")
endif()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_shapes_input_surface.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE surface_result
  OUTPUT_VARIABLE surface_output ERROR_VARIABLE surface_error)
if(NOT surface_result EQUAL 0)
  message(FATAL_ERROR "WP20 public shapes/input surface failed:\n${surface_output}${surface_error}")
endif()

execute_process(COMMAND "${ROCKETC}" check "${SOURCE_DIR}/tests/fixtures/rocket3_shapes_input_wrong_type_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE wrong_type_result
  OUTPUT_VARIABLE wrong_type_output ERROR_VARIABLE wrong_type_error)
if(wrong_type_result EQUAL 0 OR NOT "${wrong_type_output}${wrong_type_error}" MATCHES "error\\[R4001\\].*Vec2, expected rocket.graphics.Rect")
  message(FATAL_ERROR "WP20 typed-shape diagnostic failed:\n${wrong_type_output}${wrong_type_error}")
endif()

execute_process(COMMAND "${ROCKETC}" doc "${SOURCE_DIR}/tests/fixtures/rocket3_shapes_input_package" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE doc_result
  OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP20 package documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
string(FIND "${documentation_index}" "\"name\": \"sample_outline\", \"kind\": \"fn\"" outline_found)
string(FIND "${documentation_index}" "\"parameters\": [\"frame\", \"bounds\", \"color\", \"thickness\"]" outline_parameters_found)
string(FIND "${documentation_index}" "\"defaults\": [null, null, null, \"1.0\"]" outline_defaults_found)
string(FIND "${documentation_index}" "\"name\": \"sample_pointer\", \"kind\": \"fn\"" pointer_found)
string(FIND "${documentation_index}" "\"defaults\": [null, \"0\"]" pointer_defaults_found)
if(outline_found EQUAL -1 OR outline_parameters_found EQUAL -1 OR outline_defaults_found EQUAL -1 OR pointer_found EQUAL -1 OR pointer_defaults_found EQUAL -1)
  message(FATAL_ERROR "WP20 documentation omitted named/default metadata:\n${documentation_index}")
endif()

execute_process(COMMAND "${ROCKETC}" test "${SOURCE_DIR}/tests/fixtures/rocket3_shapes_input_package"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0 OR NOT "${package_output}${package_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP20 deterministic package test failed:\n${package_output}${package_error}")
endif()

message(STATUS "WP20 public shapes/input matrix passed")

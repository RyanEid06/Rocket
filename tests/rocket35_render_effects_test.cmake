if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP3 rendering effects test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/tests/fixtures/rocket35_render_effects_package")
file(MAKE_DIRECTORY "${WORK}")
foreach(source IN ITEMS
    "stdlib/rocket/raylib/safe.rocket"
    "stdlib/rocket/raylib/native.rocket"
    "tests/fixtures/rocket35_render_effects_package/src/main.rocket"
    "tests/fixtures/rocket35_render_effects_package/testing.rocket"
    "tests/fixtures/rocket35_render_effects_package/tests/effects_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${SOURCE_DIR}/${source}" --check
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE format_result
    OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
  if(NOT format_result EQUAL 0)
    message(FATAL_ERROR "WP3 format check failed for ${source}:\n${format_output}${format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE surface_result
  OUTPUT_VARIABLE surface_output ERROR_VARIABLE surface_error)
if(NOT surface_result EQUAL 0)
  message(FATAL_ERROR "WP3 canonical surface failed:\n${surface_output}${surface_error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" test "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0 OR NOT "${package_output}${package_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP3 package test failed:\n${package_output}${package_error}")
endif()

message(STATUS "WP3 production rendering effects package passed")

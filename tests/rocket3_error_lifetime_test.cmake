if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP29 error/lifetime test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

foreach(source IN ITEMS
    "stdlib/rocket/raylib/safe.rocket"
    "stdlib/rocket/graphics/input.rocket"
    "stdlib/rocket/graphics/canvas.rocket"
    "stdlib/rocket/ui.rocket"
    "tests/fixtures/rocket3_error_lifetime_surface.rocket"
    "tests/fixtures/rocket3_error_lifetime_wrong_resource_failure.rocket"
    "tests/fixtures/rocket3_error_lifetime_package/src/main.rocket"
    "tests/fixtures/rocket3_error_lifetime_package/src/wp29_adapter_testing.rocket"
    "tests/fixtures/rocket3_error_lifetime_package/tests/error_lifetime_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${SOURCE_DIR}/${source}" --check
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE format_result
    OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
  if(NOT format_result EQUAL 0)
    message(FATAL_ERROR
      "WP29 formatter failed for ${source}:\n${format_output}${format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check
  "${SOURCE_DIR}/tests/fixtures/rocket3_error_lifetime_package"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE check_result
  OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP29 package check failed:\n${check_output}${check_error}")
endif()

execute_process(COMMAND "${ROCKETC}" check
  "${SOURCE_DIR}/tests/fixtures/rocket3_error_lifetime_wrong_resource_failure.rocket"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE wrong_type_result
  OUTPUT_VARIABLE wrong_type_output ERROR_VARIABLE wrong_type_error)
if(wrong_type_result EQUAL 0 OR
   NOT "${wrong_type_output}${wrong_type_error}" MATCHES "error\\[R4001\\].*RenderTexture, expected rocket.raylib.safe.Window")
  message(FATAL_ERROR
    "WP29 wrong-resource compile-time contract failed:\n${wrong_type_output}${wrong_type_error}")
endif()

file(READ "${SOURCE_DIR}/docs/STDLIB.md" stdlib_documentation)
foreach(required_text IN ITEMS
    "mismatches are compile-time errors"
    "recoverable `Result[..., String]` failures"
    "color-channel clamping remains intentional value semantics"
    "silently treated as successful"
    "an outdated copied lease returns the stale-lifecycle error")
  string(FIND "${stdlib_documentation}" "${required_text}" required_text_position)
  if(required_text_position EQUAL -1)
    message(FATAL_ERROR "WP29 error/lifetime classification omitted: ${required_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
    "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
    "${ROCKETC}" test "${SOURCE_DIR}/tests/fixtures/rocket3_error_lifetime_package"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output
  ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0 OR NOT "${package_output}${package_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP29 stale-window Result contract failed:\n${package_output}${package_error}")
endif()

message(STATUS "WP29 error/lifetime matrix passed")

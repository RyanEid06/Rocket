if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP2 image-backed table test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/examples/rocket35_texture_table")
file(READ "${package}/src/main.rocket" source)
if("${source}" MATCHES "examples/raylib_showcase|rocket.raylib.native|src.rocket_raylib")
  message(FATAL_ERROR "WP2 table must use only canonical public graphics modules")
endif()

file(MAKE_DIRECTORY "${WORK}")
file(COPY "${package}/assets" DESTINATION "${WORK}")
execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE check_result
  OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP2 table source check failed:\n${check_output}${check_error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" run "${package}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "WP2 image-backed table failed to render:\n${run_output}${run_error}")
endif()

set(screenshot "${WORK}/casino-table.png")
if(NOT EXISTS "${screenshot}")
  message(FATAL_ERROR "WP2 image-backed table did not save its screenshot")
endif()
file(SIZE "${screenshot}" screenshot_size)
file(READ "${screenshot}" png_header LIMIT 24 HEX)
if(screenshot_size LESS 10000 OR NOT png_header MATCHES "^89504e470d0a1a0a0000000d4948445200000320000001c2$")
  message(FATAL_ERROR "WP2 table screenshot is not the expected nontrivial 800x450 PNG")
endif()

file(SHA256 "${screenshot}" first_capture)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" run "${package}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE repeat_result
  OUTPUT_VARIABLE repeat_output ERROR_VARIABLE repeat_error)
if(NOT repeat_result EQUAL 0)
  message(FATAL_ERROR "WP2 table repeat render failed:\n${repeat_output}${repeat_error}")
endif()
file(SHA256 "${screenshot}" second_capture)
if(NOT first_capture STREQUAL second_capture)
  message(FATAL_ERROR "WP2 table screenshots differ across identical runs")
endif()

message(STATUS "WP2 image-backed casino table rendered to ${screenshot}")

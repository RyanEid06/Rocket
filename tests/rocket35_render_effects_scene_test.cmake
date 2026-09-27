if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT OR NOT DEFINED NATIVE_TARGET OR
   NOT DEFINED EXECUTABLE_SUFFIX OR NOT DEFINED SOFTWARE_OPENGL_ROOT OR
   NOT DEFINED PYTHON)
  message(FATAL_ERROR "WP3 render effects scene test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/examples/rocket35_render_effects")
file(READ "${package}/src/main.rocket" source)
if("${source}" MATCHES "examples/raylib_showcase|rocket.raylib.native|src.rocket_raylib")
  message(FATAL_ERROR "WP3 scene must use documented stdlib APIs only")
endif()
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${package}/assets" DESTINATION "${WORK}")
execute_process(COMMAND "${ROCKETC}" fmt "${package}/src/main.rocket" --check
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE format_result
  OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP3 scene format failed:\n${format_output}${format_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE check_result
  OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP3 scene source check failed:\n${check_output}${check_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" build "${package}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "WP3 scene failed to build:\n${build_output}${build_error}")
endif()
set(executable_directory "${WORK}/artifacts/rocket35_render_effects/.rocketc/targets/${NATIVE_TARGET}")
set(executable "${executable_directory}/main${EXECUTABLE_SUFFIX}")
if(NATIVE_TARGET STREQUAL "windows-x64" AND
   EXISTS "${SOFTWARE_OPENGL_ROOT}/opengl32.dll")
  file(COPY "${SOFTWARE_OPENGL_ROOT}/opengl32.dll"
            "${SOFTWARE_OPENGL_ROOT}/libgallium_wgl.dll"
       DESTINATION "${executable_directory}")
endif()
execute_process(COMMAND "${executable}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "WP3 scene failed to render:\n${run_output}${run_error}")
endif()
foreach(name IN ITEMS "casino-effects-off.png" "casino-effects-before.png" "casino-effects-resized.png")
  set(screenshot "${WORK}/${name}")
  if(NOT EXISTS "${screenshot}")
    message(FATAL_ERROR "WP3 scene did not save ${name}")
  endif()
  file(SIZE "${screenshot}" screenshot_size)
  file(READ "${screenshot}" png_header LIMIT 8 HEX)
  if(screenshot_size LESS 10000 OR NOT png_header STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "WP3 scene output ${name} is not a nontrivial PNG")
  endif()
endforeach()
if(NOT "${run_output}" MATCHES "wp3-shader-unavailable")
  execute_process(COMMAND "${PYTHON}"
      "${SOURCE_DIR}/tests/rocket35_shader_effect_compare.py"
      "${WORK}/casino-effects-off.png"
      "${WORK}/casino-effects-before.png"
    RESULT_VARIABLE compare_result OUTPUT_VARIABLE compare_output ERROR_VARIABLE compare_error)
  if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "WP3 shader effect comparison failed:\n${compare_output}${compare_error}")
  endif()
  message(STATUS "${compare_output}")
else()
  message(STATUS "WP3 shader effect comparison skipped: shader capability unavailable")
endif()
file(SHA256 "${WORK}/casino-effects-before.png" before_capture)
file(SHA256 "${WORK}/casino-effects-resized.png" resized_capture)
if(before_capture STREQUAL resized_capture)
  if(NOT "${run_output}" MATCHES "wp3-resize-unavailable")
    message(FATAL_ERROR "WP3 output did not change despite a resized framebuffer")
  endif()
  message(STATUS "Headless display kept its original framebuffer; deterministic WP3 fixture covers resized canvas composition")
elseif(NOT "${run_output}" MATCHES "wp3-resize-applied")
  message(FATAL_ERROR "WP3 output changed without an observed framebuffer resize")
endif()
message(STATUS "WP3 shader, blend, texture and resized canvas scene rendered")

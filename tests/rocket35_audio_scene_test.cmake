if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT OR NOT DEFINED NATIVE_TARGET OR
   NOT DEFINED EXECUTABLE_SUFFIX OR NOT DEFINED SOFTWARE_OPENGL_ROOT)
  message(FATAL_ERROR "WP4 audio scene test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/examples/rocket35_audio")
file(READ "${package}/src/main.rocket" source)
if("${source}" MATCHES "examples/raylib_showcase|rocket.raylib.native|src.rocket_raylib")
  message(FATAL_ERROR "WP4 scene must use documented stdlib APIs only")
endif()
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${package}/assets" DESTINATION "${WORK}")
execute_process(COMMAND "${ROCKETC}" fmt "${package}/src/main.rocket" --check
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE format_result
  OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP4 scene format failed:\n${format_output}${format_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE check_result
  OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP4 scene source check failed:\n${check_output}${check_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" build "${package}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "WP4 scene failed to build:\n${build_output}${build_error}")
endif()
set(executable_directory "${WORK}/artifacts/rocket35_audio/.rocketc/targets/${NATIVE_TARGET}")
set(executable "${executable_directory}/main${EXECUTABLE_SUFFIX}")
file(REMOVE "${WORK}/casino-audio-before.png" "${WORK}/casino-audio-resized.png")
if(NATIVE_TARGET STREQUAL "windows-x64" AND
   EXISTS "${SOFTWARE_OPENGL_ROOT}/opengl32.dll")
  file(COPY "${SOFTWARE_OPENGL_ROOT}/opengl32.dll"
            "${SOFTWARE_OPENGL_ROOT}/libgallium_wgl.dll"
       DESTINATION "${executable_directory}")
endif()
execute_process(COMMAND "${executable}"
  WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
if(run_result EQUAL 20 AND "${run_output}${run_error}" MATCHES "raylib: device or backend unavailable")
  message("SKIP: system audio device unavailable; deterministic adapter and native null-backend tests cover lifecycle and stream execution, not audible playback")
  return()
endif()
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "WP4 scene failed to render:\n${run_output}${run_error}")
endif()
foreach(name IN ITEMS "casino-audio-before.png" "casino-audio-resized.png")
  set(screenshot "${WORK}/${name}")
  if(NOT EXISTS "${screenshot}")
    message(FATAL_ERROR "WP4 scene did not save ${name}")
  endif()
  file(SIZE "${screenshot}" screenshot_size)
  file(READ "${screenshot}" png_header LIMIT 8 HEX)
  if(screenshot_size LESS 10000 OR NOT png_header STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "WP4 scene output ${name} is not a nontrivial PNG")
  endif()
endforeach()
file(SHA256 "${WORK}/casino-audio-before.png" before_capture)
file(SHA256 "${WORK}/casino-audio-resized.png" resized_capture)
if(before_capture STREQUAL resized_capture)
  if(NOT "${run_output}" MATCHES "wp4-resize-unavailable")
    message(FATAL_ERROR "WP4 output did not change despite a resized framebuffer")
  endif()
  message(STATUS "Headless display kept its original framebuffer; deterministic WP4 fixture covers resized canvas composition")
elseif(NOT "${run_output}" MATCHES "wp4-resize-applied")
  message(FATAL_ERROR "WP4 output changed without an observed framebuffer resize")
endif()
if("$ENV{ROCKET_AUDIO_NULL_BACKEND}" STREQUAL "1")
  message(STATUS "WP4 null-backend scene ran native music, effects, texture and resized canvas; no audible output")
else()
  message(STATUS "WP4 system-audio-backend scene ran music, effects, texture and resized canvas; audible output was not measured")
endif()

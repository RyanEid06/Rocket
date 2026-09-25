if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT OR NOT DEFINED NATIVE_TARGET OR
   NOT DEFINED EXECUTABLE_SUFFIX OR NOT DEFINED PYTHON)
  message(FATAL_ERROR "WP7 slice test is missing required arguments")
endif()
if(DEFINED STAGE0)
  set(ENV{ROCKET_STAGE0} "${STAGE0}")
endif()

set(package "${SOURCE_DIR}/examples/scroll2roll_vertical_slice")
file(READ "${package}/src/main.rocket" source)
if("${source}" MATCHES "examples/raylib_showcase|rocket.raylib.native|src.rocket_raylib|unsafe:")
  message(FATAL_ERROR "WP7 slice must use the public Rocket 3.5 API only")
endif()
file(READ "${SOURCE_DIR}/examples/raylib_showcase/src/showcase.rocket" showcase_source)
if("${showcase_source}" MATCHES "import src\\.rocket_raylib")
  message(FATAL_ERROR "Showcase application still imports the duplicate wrapper")
endif()
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${ROCKETC}" fmt "${package}/src/main.rocket" --check
  RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "WP7 slice format failed:\n${format_output}${format_error}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${package}"
  RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP7 slice check failed:\n${check_output}${check_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
  "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
  "${ROCKETC}" build "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "WP7 slice build failed:\n${build_output}${build_error}")
endif()
set(executable_directory "${WORK}/artifacts/scroll2roll_vertical_slice/.rocketc/targets/${NATIVE_TARGET}")
set(executable "${executable_directory}/scroll2roll-vertical-slice${EXECUTABLE_SUFFIX}")
if(NATIVE_TARGET STREQUAL "windows-x64" AND DEFINED SOFTWARE_OPENGL_ROOT AND
   EXISTS "${SOFTWARE_OPENGL_ROOT}/opengl32.dll")
  file(COPY "${SOFTWARE_OPENGL_ROOT}/opengl32.dll"
            "${SOFTWARE_OPENGL_ROOT}/libgallium_wgl.dll"
       DESTINATION "${executable_directory}")
endif()

# Run from outside the package three times. This exercises explicit package
# roots, repeated native startup/shutdown, resize/fullscreen transitions, and
# all four nested rendering modes in the source scene.
foreach(cycle RANGE 1 3)
  file(REMOVE "${WORK}/scroll2roll-slice.png" "${WORK}/scroll2roll-presented.png")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_SLICE_BENCHMARK=1"
    "ROCKET_SLICE_PACKAGE_ROOT=${package}"
    "${executable}"
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
  if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "WP7 slice cycle ${cycle} failed:\n${run_output}${run_error}")
  endif()
  if(NOT "${run_output}" MATCHES "measured frames[\r\n]+240[\r\n]+average ms")
    message(FATAL_ERROR "WP7 slice cycle ${cycle} did not report 240 measured frames:\n${run_output}")
  endif()
  string(REGEX MATCH "average ms[\r\n]+([0-9]+(\\.[0-9]+)?)" average_match "${run_output}")
  set(average_ms "${CMAKE_MATCH_1}")
  string(REGEX MATCH "worst ms[\r\n]+([0-9]+(\\.[0-9]+)?)" worst_match "${run_output}")
  set(worst_ms "${CMAKE_MATCH_1}")
  if(average_ms STREQUAL "" OR worst_ms STREQUAL "")
    message(FATAL_ERROR "WP7 slice cycle ${cycle} omitted timing data:\n${run_output}")
  endif()
  foreach(name IN ITEMS scroll2roll-slice.png scroll2roll-presented.png)
    if(NOT EXISTS "${WORK}/${name}")
      message(FATAL_ERROR "WP7 slice cycle ${cycle} did not capture ${name}")
    endif()
    file(SIZE "${WORK}/${name}" png_size)
    file(READ "${WORK}/${name}" png_header LIMIT 8 HEX)
    if(png_size LESS 10000 OR NOT png_header STREQUAL "89504e470d0a1a0a")
      message(FATAL_ERROR "WP7 slice cycle ${cycle} captured an invalid ${name}")
    endif()
  endforeach()
  execute_process(COMMAND "${PYTHON}"
    "${SOURCE_DIR}/tests/rocket35_scroll2roll_visual_compare.py"
    "${SOURCE_DIR}/tests/visual/goldens/wp7/manifest.json"
    "${SOURCE_DIR}/tests/visual/goldens/wp7"
    "${WORK}/scroll2roll-slice.png"
    "${WORK}/scroll2roll-presented.png"
    RESULT_VARIABLE visual_result OUTPUT_VARIABLE visual_output ERROR_VARIABLE visual_error)
  if(NOT visual_result EQUAL 0)
    message(FATAL_ERROR "WP7 slice visual cycle ${cycle} failed:\n${visual_output}${visual_error}")
  endif()
  message(STATUS "cycle ${cycle}: average=${average_ms}ms worst=${worst_ms}ms ${visual_output}")
endforeach()
message(STATUS "WP7 slice: 3 native lifetimes, 240 measured frames each, golden passed")

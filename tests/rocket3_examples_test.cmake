if(NOT DEFINED ROCKETC OR NOT DEFINED EXAMPLE_ROOT OR
   NOT DEFINED NATIVE_LIBRARY_ROOT OR NOT DEFINED NATIVE_TARGET OR
   NOT DEFINED EXECUTABLE_SUFFIX OR NOT DEFINED SOFTWARE_OPENGL_ROOT OR
   NOT DEFINED VISUAL_TEST OR NOT DEFINED GOLDEN_ROOT OR
   NOT DEFINED ARTIFACT_ROOT)
  message(FATAL_ERROR "WP33 example acceptance is missing arguments")
endif()

set(example_files
  "examples/motion_and_color.rocket"
  "examples/layout_and_controls.rocket"
  "examples/premium_card_table.rocket")
if(NOT EXISTS "${EXAMPLE_ROOT}/assets/card-back.ppm")
  message(FATAL_ERROR "WP33 logical texture asset is missing")
endif()
set(all_sources "")
foreach(relative IN LISTS example_files)
  set(source "${EXAMPLE_ROOT}/${relative}")
  if(NOT EXISTS "${source}")
    message(FATAL_ERROR "WP33 example is missing: ${relative}")
  endif()
  file(READ "${source}" content)
  string(APPEND all_sources "\n${content}")
endforeach()

foreach(target IN ITEMS windows-x64 linux-x64 linux-arm64 macos-arm64)
  foreach(relative IN LISTS example_files)
    execute_process(
      COMMAND "${ROCKETC}" check "${EXAMPLE_ROOT}/${relative}"
        --target "${target}"
      WORKING_DIRECTORY "${EXAMPLE_ROOT}"
      RESULT_VARIABLE target_result OUTPUT_VARIABLE target_output
      ERROR_VARIABLE target_error)
    if(NOT target_result EQUAL 0)
      message(FATAL_ERROR
        "WP33 ${target} source check failed for ${relative}:\n${target_output}${target_error}")
    endif()
  endforeach()
endforeach()

file(READ "${EXAMPLE_ROOT}/README.md" example_readme)
foreach(documented IN ITEMS
    "motion_and_color.rocket" "layout_and_controls.rocket"
    "premium_card_table.rocket" "not evidence" "Scroll2Roll visual fidelity")
  string(FIND "${example_readme}" "${documented}" documented_at)
  if(documented_at EQUAL -1)
    message(FATAL_ERROR "WP33 documentation is missing: ${documented}")
  endif()
endforeach()

set(required_public_usage
  "import std.math"
  "import rocket.motion"
  "import rocket.graphics"
  "import rocket.graphics.shapes"
  "import rocket.graphics.canvas"
  "import rocket.raylib.safe"
  "import rocket.ui"
  "import rocket.ui.controls"
  "import rocket.ui.layout"
  "import rocket.ui.styles"
  "import rocket.ui.theme"
  "math.lerp("
  "motion.policy("
  "reduced_motion"
  "graphics.color_from_hex("
  "graphics.transform2d("
  "graphics.fit_virtual_canvas("
  "shapes.draw_rounded_rect("
  "shapes.draw_gradient_rect("
  "safe.draw_text_layout("
  "layout.anchor_rect("
  "controls.image("
  "controls.button("
  "theme.dark_theme("
  "hovered_card")
foreach(token IN LISTS required_public_usage)
  string(FIND "${all_sources}" "${token}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "WP33 examples do not cover required public API: ${token}")
  endif()
endforeach()
string(FIND "${all_sources}" "Scroll2Roll" prohibited_claim)
if(NOT prohibited_claim EQUAL -1)
  message(FATAL_ERROR "WP33 source must remain a neutral showcase")
endif()

foreach(relative IN LISTS example_files)
  execute_process(
    COMMAND "${ROCKETC}" check "${EXAMPLE_ROOT}/${relative}"
    WORKING_DIRECTORY "${EXAMPLE_ROOT}"
    RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error)
  if(NOT check_result EQUAL 0)
    message(FATAL_ERROR
      "WP33 source check failed for ${relative}:\n${check_output}${check_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check "${EXAMPLE_ROOT}"
  WORKING_DIRECTORY "${EXAMPLE_ROOT}"
  RESULT_VARIABLE package_result OUTPUT_VARIABLE package_output
  ERROR_VARIABLE package_error)
if(NOT package_result EQUAL 0)
  message(FATAL_ERROR "WP33 package check failed:\n${package_output}${package_error}")
endif()

set(relocated "${ARTIFACT_ROOT}/relocated-rocket3-graphics-ui")
file(REMOVE_RECURSE "${relocated}")
file(MAKE_DIRECTORY "${relocated}/native")
file(COPY "${EXAMPLE_ROOT}/rocket.toml" "${EXAMPLE_ROOT}/README.md"
  "${EXAMPLE_ROOT}/assets" "${EXAMPLE_ROOT}/examples"
  DESTINATION "${relocated}")
execute_process(COMMAND "${ROCKETC}" check "${relocated}"
  WORKING_DIRECTORY "${relocated}"
  RESULT_VARIABLE relocated_result OUTPUT_VARIABLE relocated_output
  ERROR_VARIABLE relocated_error)
if(NOT relocated_result EQUAL 0)
  message(FATAL_ERROR
    "WP33 relocated package check failed:\n${relocated_output}${relocated_error}")
endif()
foreach(relative IN LISTS example_files)
  execute_process(COMMAND "${ROCKETC}" check "${relocated}/${relative}"
    WORKING_DIRECTORY "${relocated}"
    RESULT_VARIABLE relocated_source_result
    OUTPUT_VARIABLE relocated_source_output ERROR_VARIABLE relocated_source_error)
  if(NOT relocated_source_result EQUAL 0)
    message(FATAL_ERROR
      "WP33 relocated source check failed for ${relative}:\n${relocated_source_output}${relocated_source_error}")
  endif()
endforeach()

file(READ "${EXAMPLE_ROOT}/rocket.toml" original_manifest)
foreach(relative IN LISTS example_files)
  string(REPLACE "entry = \"examples/premium_card_table.rocket\""
    "entry = \"${relative}\"" runner_manifest "${original_manifest}")
  file(WRITE "${relocated}/rocket.toml" "${runner_manifest}")
  get_filename_component(example_name "${relative}" NAME_WE)
  if(relative STREQUAL "examples/premium_card_table.rocket" AND
     NOT NATIVE_TARGET STREQUAL "windows-x64")
    continue()
  endif()
  set(run_artifact_root "${ARTIFACT_ROOT}/run/${example_name}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
      "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_LIBRARY_ROOT}"
      "ROCKET_ARTIFACT_ROOT=${run_artifact_root}"
      "${ROCKETC}" build "${relocated}"
    WORKING_DIRECTORY "${relocated}"
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
  if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
      "WP33 runnable example build failed for ${relative}:\n${build_output}${build_error}")
  endif()
  set(executable
    "${run_artifact_root}/rocket3_graphics_ui/.rocketc/targets/${NATIVE_TARGET}/rocket3-graphics-ui-showcase${EXECUTABLE_SUFFIX}")
  if(NATIVE_TARGET STREQUAL "windows-x64" AND
     EXISTS "${SOFTWARE_OPENGL_ROOT}/opengl32.dll")
    file(COPY "${SOFTWARE_OPENGL_ROOT}/opengl32.dll"
      "${SOFTWARE_OPENGL_ROOT}/libgallium_wgl.dll"
      DESTINATION
        "${run_artifact_root}/rocket3_graphics_ui/.rocketc/targets/${NATIVE_TARGET}")
  endif()
  execute_process(COMMAND "${executable}"
    WORKING_DIRECTORY "${relocated}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error)
  if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
      "WP33 runnable example failed for ${relative}:\n${run_output}${run_error}")
  endif()
endforeach()
if(NATIVE_TARGET STREQUAL "windows-x64" AND
   NOT EXISTS "${relocated}/rocket3-premium-showcase.png")
  message(FATAL_ERROR "WP33 runnable premium showcase did not capture a PNG")
endif()
file(WRITE "${relocated}/rocket.toml" "${original_manifest}")

execute_process(COMMAND "${VISUAL_TEST}" "${GOLDEN_ROOT}"
  "${ARTIFACT_ROOT}/visual"
  RESULT_VARIABLE visual_result OUTPUT_VARIABLE visual_output
  ERROR_VARIABLE visual_error)
if(NOT visual_result EQUAL 0)
  message(FATAL_ERROR
    "WP33 premium visual validation failed:\n${visual_output}${visual_error}")
endif()
message(STATUS "WP33 examples, package relocation, and visuals passed")

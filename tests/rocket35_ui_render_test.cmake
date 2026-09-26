if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP6 UI render test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/tests/fixtures/rocket35_ui_render_package")
file(MAKE_DIRECTORY "${WORK}")
foreach(source IN ITEMS
    "stdlib/rocket/ui/render.rocket"
    "tests/fixtures/rocket35_ui_render_package/src/main.rocket"
    "tests/fixtures/rocket35_ui_render_package/testing.rocket"
    "tests/fixtures/rocket35_ui_render_package/tests/render_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${SOURCE_DIR}/${source}" --check
    RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
  if(NOT format_result EQUAL 0)
    message(FATAL_ERROR "WP6 formatter failed for ${source}:\n${format_output}${format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP6 package check failed:\n${check_output}${check_error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
    "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
    "ROCKET_UI_PACKAGE_ROOT=${package}"
    "${ROCKETC}" test "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE test_result OUTPUT_VARIABLE test_output ERROR_VARIABLE test_error)
if(NOT test_result EQUAL 0 OR
   NOT "${test_output}${test_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP6 package tests failed:\n${test_output}${test_error}")
endif()

file(MAKE_DIRECTORY "${WORK}/doc-package/src")
file(WRITE "${WORK}/doc-package/rocket.toml"
  "[package]\nname = \"rocket35_ui_render_docs\"\nversion = \"0.1.0\"\nentry = \"src/render.rocket\"\n")
file(COPY_FILE "${SOURCE_DIR}/stdlib/rocket/ui/render.rocket"
  "${WORK}/doc-package/src/render.rocket")
execute_process(COMMAND "${ROCKETC}" doc "${WORK}/doc-package" --output "${WORK}/docs"
  RESULT_VARIABLE doc_result OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP6 docs failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" search_index)
foreach(symbol IN ITEMS ImageFit image_contain image_cover image_stretch
    draw_panel begin_panel_content_clip end_panel_content_clip draw_button
    draw_label draw_image draw_image_from_store draw_progress_bar)
  if(NOT "${search_index}" MATCHES "\\\"name\\\": \\\"${symbol}\\\"")
    message(FATAL_ERROR "WP6 docs/search omitted ${symbol}")
  endif()
endforeach()

if(VISUAL)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
      "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
      "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
      "${ROCKETC}" build "${package}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
  if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "WP6 visual fixture build failed:\n${build_output}${build_error}")
  endif()
  if(WIN32)
    set(suffix ".exe")
  else()
    set(suffix "")
  endif()
  get_filename_component(target "${NATIVE_ROOT}" NAME)
  file(MAKE_DIRECTORY "${WORK}/visual")
  set(executable_directory
    "${WORK}/artifacts/rocket35_ui_render_package/.rocketc/targets/${target}")
  if(WIN32 AND DEFINED SOFTWARE_OPENGL_ROOT AND
     EXISTS "${SOFTWARE_OPENGL_ROOT}/opengl32.dll")
    file(COPY "${SOFTWARE_OPENGL_ROOT}/opengl32.dll"
      "${SOFTWARE_OPENGL_ROOT}/libgallium_wgl.dll"
      DESTINATION "${executable_directory}")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
      "ROCKET_UI_VISUAL_OUTPUT=${WORK}/visual"
      "ROCKET_UI_PACKAGE_ROOT=${package}"
      "${executable_directory}/main${suffix}"
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE visual_result OUTPUT_VARIABLE visual_output ERROR_VARIABLE visual_error)
  if(NOT visual_result EQUAL 0)
    message(FATAL_ERROR "WP6 visual capture failed:\n${visual_output}${visual_error}")
  endif()
  execute_process(COMMAND "${PYTHON}" "${SOURCE_DIR}/tests/rocket35_ui_render_visual_compare.py"
      "${SOURCE_DIR}/tests/visual/goldens/wp6/manifest.json"
      "${SOURCE_DIR}/tests/visual/goldens/wp6" "${WORK}/visual"
    RESULT_VARIABLE compare_result OUTPUT_VARIABLE compare_output ERROR_VARIABLE compare_error)
  if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "WP6 visual comparison failed:\n${compare_output}${compare_error}")
  endif()
  message(STATUS "${compare_output}")
endif()
message(STATUS "WP6 UI renderer package, format, docs, and native seam passed")

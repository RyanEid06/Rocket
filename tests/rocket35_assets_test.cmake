if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP5 asset test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/tests/fixtures/rocket35_assets_package")
file(MAKE_DIRECTORY "${WORK}/elsewhere")
foreach(source IN ITEMS
    "stdlib/rocket/assets.rocket"
    "tests/fixtures/rocket35_assets_package/src/main.rocket"
    "tests/fixtures/rocket35_assets_package/testing.rocket"
    "tests/fixtures/rocket35_assets_package/tests/asset_store_test.rocket"
    "tests/fixtures/rocket35_assets_package/tests/missing_asset_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${SOURCE_DIR}/${source}" --check
    RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
  if(NOT format_result EQUAL 0)
    message(FATAL_ERROR "WP5 formatter failed for ${source}:\n${format_output}${format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP5 package check failed:\n${check_output}${check_error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
    "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
    "ROCKET_ASSET_PACKAGE_ROOT=${package}"
    "${ROCKETC}" test "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE test_result OUTPUT_VARIABLE test_output ERROR_VARIABLE test_error)
if(NOT test_result EQUAL 0 OR
   NOT "${test_output}${test_error}" MATCHES "2 passed; 0 failed")
  message(FATAL_ERROR "WP5 asset tests failed:\n${test_output}${test_error}")
endif()

# The self-hosted compiler currently discovers bundled source relative to the
# repository; run the built program separately to prove that asset resolution
# itself never depends on that compiler discovery directory.
get_filename_component(native_target "${NATIVE_ROOT}" NAME)
if(WIN32)
  set(executable_suffix ".exe")
else()
  set(executable_suffix "")
endif()
set(asset_executable
  "${WORK}/artifacts/rocket35_assets_package/.rocketc/targets/${native_target}/asset_store_test${executable_suffix}")
if(NOT EXISTS "${asset_executable}")
  set(asset_executable
    "${WORK}/artifacts/rocket35_assets_package/.rocketc/targets/${native_target}/tests/test-0${executable_suffix}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_ASSET_PACKAGE_ROOT=${package}" "${asset_executable}"
  WORKING_DIRECTORY "${WORK}/elsewhere"
  RESULT_VARIABLE detached_result OUTPUT_VARIABLE detached_output ERROR_VARIABLE detached_error)
if(NOT detached_result EQUAL 0)
  message(FATAL_ERROR "WP5 detached-cwd asset run failed (${detached_result}):\n${detached_output}${detached_error}")
endif()

# Document the canonical source as a package module so search.json contains
# its public API instead of only the fixture's test harness.
file(MAKE_DIRECTORY "${WORK}/doc-package/src")
file(WRITE "${WORK}/doc-package/rocket.toml"
  "[package]\nname = \"rocket35_assets_docs\"\nversion = \"0.1.0\"\nentry = \"src/assets.rocket\"\n")
file(COPY_FILE "${SOURCE_DIR}/stdlib/rocket/assets.rocket"
  "${WORK}/doc-package/src/assets.rocket")
execute_process(COMMAND "${ROCKETC}" doc "${WORK}/doc-package" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE doc_result OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP5 docs failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" search_index)
foreach(symbol IN ITEMS AssetStore TextureRef FontRef SoundRef MusicRef ShaderRef
    load_texture load_music borrow_shader)
  if(NOT "${search_index}" MATCHES "\\\"name\\\": \\\"${symbol}\\\"")
    message(FATAL_ERROR "WP5 docs/search omitted ${symbol}")
  endif()
endforeach()
message(STATUS "WP5 asset package, detached working directory, format, and docs passed")

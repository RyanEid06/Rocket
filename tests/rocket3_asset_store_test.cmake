if(NOT DEFINED ROCKETC OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK OR
   NOT DEFINED NATIVE_ROOT)
  message(FATAL_ERROR "WP28 asset-store test is missing required arguments")
endif()

set(package "${SOURCE_DIR}/examples/raylib_showcase")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/artifacts")

execute_process(COMMAND "${ROCKETC}" check "${package}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "WP28 asset-store package check failed:\n${check_output}${check_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "ROCKET_NATIVE_LIBRARY_ROOT=${NATIVE_ROOT}"
    "ROCKET_ARTIFACT_ROOT=${WORK}/artifacts"
    "${ROCKETC}" test "${package}" --filter asset_store
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE test_result OUTPUT_VARIABLE test_output ERROR_VARIABLE test_error)
if(NOT test_result EQUAL 0 OR
   NOT "${test_output}${test_error}" MATCHES "1 passed; 0 failed")
  message(FATAL_ERROR "WP28 asset-store Rocket test failed:\n${test_output}${test_error}")
endif()

foreach(source IN ITEMS
    "${package}/src/rocket_assets.rocket"
    "${package}/src/rocket_raylib.rocket"
    "${package}/src/rocket_raylib_adapter.rocket"
    "${package}/src/rocket_raylib_testing.rocket"
    "${package}/tests/asset_store_test.rocket")
  execute_process(COMMAND "${ROCKETC}" fmt "${source}" --check
    RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output ERROR_VARIABLE format_error)
  if(NOT format_result EQUAL 0)
    message(FATAL_ERROR "WP28 asset-store formatter failed for ${source}:\n${format_output}${format_error}")
  endif()
endforeach()

execute_process(COMMAND "${ROCKETC}" doc "${package}" --output "${WORK}/docs"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE doc_result OUTPUT_VARIABLE doc_output ERROR_VARIABLE doc_error)
if(NOT doc_result EQUAL 0)
  message(FATAL_ERROR "WP28 asset-store documentation failed:\n${doc_output}${doc_error}")
endif()
file(READ "${WORK}/docs/search.json" documentation_index)
string(FIND "${documentation_index}" "\"name\": \"AssetStore\", \"kind\": \"struct\"" store_found)
string(FIND "${documentation_index}" "\"name\": \"load_texture\", \"kind\": \"fn\"" load_texture_found)
string(FIND "${documentation_index}" "\"name\": \"load_music\", \"kind\": \"fn\"" load_music_found)
string(FIND "${documentation_index}" "\"name\": \"borrow_shader\", \"kind\": \"fn\"" borrow_shader_found)
string(FIND "${documentation_index}" "\"parameters\": [\"window\", \"audio\", \"package_root\"]" open_parameters_found)
string(FIND "${documentation_index}"
  [=["defaults": [null, null, "\".\""]]=] open_defaults_found)
if(store_found EQUAL -1 OR load_texture_found EQUAL -1 OR
   load_music_found EQUAL -1 OR borrow_shader_found EQUAL -1 OR
   open_parameters_found EQUAL -1 OR open_defaults_found EQUAL -1)
  message(FATAL_ERROR "WP28 asset-store docs/search omitted public metadata:\n${documentation_index}")
endif()

message(STATUS "WP28 asset-store stage/compiler surface passed")

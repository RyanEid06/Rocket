if(NOT DEFINED VISUAL_TEST OR NOT DEFINED MANIFEST OR
   NOT DEFINED GOLDEN_ROOT OR NOT DEFINED ARTIFACT_ROOT)
  message(FATAL_ERROR "WP32 visual regression test is missing arguments")
endif()
if(NOT EXISTS "${MANIFEST}")
  message(FATAL_ERROR "WP32 golden manifest is missing: ${MANIFEST}")
endif()
file(READ "${MANIFEST}" manifest_json)
string(JSON schema ERROR_VARIABLE schema_error GET "${manifest_json}" schema)
if(schema_error OR NOT schema STREQUAL "rocket3-wp32-goldens-v1")
  message(FATAL_ERROR "WP32 golden manifest has an invalid schema")
endif()
string(JSON scene_count ERROR_VARIABLE count_error
  LENGTH "${manifest_json}" scenes)
if(count_error OR NOT scene_count EQUAL 4)
  message(FATAL_ERROR "WP32 golden manifest must contain four scenes")
endif()
math(EXPR last_scene "${scene_count} - 1")
foreach(index RANGE 0 ${last_scene})
  string(JSON reference ERROR_VARIABLE reference_error
    GET "${manifest_json}" scenes ${index} reference)
  string(JSON approval ERROR_VARIABLE approval_error
    GET "${manifest_json}" scenes ${index} approval status)
  string(JSON update_mode ERROR_VARIABLE update_error
    GET "${manifest_json}" scenes ${index} approval update_mode)
  string(JSON expected_hash ERROR_VARIABLE hash_error
    GET "${manifest_json}" scenes ${index} sha256)
  string(JSON maximum_delta ERROR_VARIABLE delta_error
    GET "${manifest_json}" scenes ${index} tolerance max_channel_delta)
  string(JSON maximum_error ERROR_VARIABLE error_error
    GET "${manifest_json}" scenes ${index} tolerance mean_absolute_error)
  string(JSON maximum_ratio ERROR_VARIABLE ratio_error
    GET "${manifest_json}" scenes ${index} tolerance changed_pixel_ratio)
  if(reference_error OR approval_error OR update_error OR hash_error OR
     delta_error OR error_error OR ratio_error OR
     NOT EXISTS "${GOLDEN_ROOT}/${reference}" OR
     NOT approval STREQUAL "approved" OR
     NOT update_mode STREQUAL "review-only" OR
     NOT maximum_delta EQUAL 0 OR NOT maximum_error EQUAL 0 OR
     NOT maximum_ratio EQUAL 0)
    message(FATAL_ERROR "WP32 scene ${index} lacks an approved review-only golden")
  endif()
  file(SHA256 "${GOLDEN_ROOT}/${reference}" actual_hash)
  if(NOT actual_hash STREQUAL expected_hash)
    message(FATAL_ERROR "WP32 scene ${index} golden hash does not match its approval")
  endif()
endforeach()
file(MAKE_DIRECTORY "${ARTIFACT_ROOT}")
execute_process(COMMAND "${VISUAL_TEST}" "${GOLDEN_ROOT}" "${ARTIFACT_ROOT}"
  RESULT_VARIABLE test_result OUTPUT_VARIABLE test_output
  ERROR_VARIABLE test_error)
if(NOT test_result EQUAL 0)
  message(FATAL_ERROR "WP32 visual regression failed:\n${test_output}${test_error}")
endif()
message(STATUS "${test_output}")

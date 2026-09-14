if(NOT DEFINED PERFORMANCE_TEST OR NOT DEFINED BUDGET_RECORD OR
   NOT DEFINED ARTIFACT_ROOT)
  message(FATAL_ERROR "WP31 performance test is missing required arguments")
endif()

if(NOT EXISTS "${BUDGET_RECORD}")
  message(FATAL_ERROR "WP31 final budget record is missing: ${BUDGET_RECORD}")
endif()
file(READ "${BUDGET_RECORD}" budget_json)
string(JSON schema ERROR_VARIABLE json_error GET "${budget_json}" schema)
if(json_error OR NOT schema STREQUAL "rocket3-wp31-performance-budgets-v1")
  message(FATAL_ERROR "WP31 final budget record has an invalid schema")
endif()
string(JSON samples ERROR_VARIABLE sample_error
  GET "${budget_json}" calibration samples)
if(sample_error OR samples LESS 7)
  message(FATAL_ERROR "WP31 budget calibration requires at least seven samples")
endif()
foreach(field IN ITEMS
    native_allocations_per_frame temporary_strings
    layout_allocations_per_frame layout_recomputations
    text_measurements_per_frame asset_lookups_per_frame ffi_calls_per_frame
    render_target_switches_per_frame texture_uploads peak_state_growth
    peak_cache_growth mean_frame_time_us maximum_frame_time_us)
  string(JSON value ERROR_VARIABLE value_error
    GET "${budget_json}" final_budgets "${field}")
  if(value_error)
    message(FATAL_ERROR "WP31 budget record is missing ${field}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${ARTIFACT_ROOT}")
execute_process(COMMAND "${PERFORMANCE_TEST}" "${ARTIFACT_ROOT}"
  RESULT_VARIABLE test_result OUTPUT_VARIABLE test_output
  ERROR_VARIABLE test_error)
if(NOT test_result EQUAL 0)
  message(FATAL_ERROR "WP31 performance budgets failed:\n${test_output}${test_error}")
endif()
message(STATUS "${test_output}")

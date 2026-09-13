if(NOT DEFINED ROCKETC OR NOT DEFINED SELFHOST OR NOT DEFINED SOURCE_DIR OR
   NOT DEFINED WORK)
  message(FATAL_ERROR "WP30 bounded-state target-surface test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
set(asset_package "${SOURCE_DIR}/examples/raylib_showcase")
foreach(compiler_name IN ITEMS stage0 selfhost)
  if(compiler_name STREQUAL "stage0")
    set(compiler "${ROCKETC}")
  else()
    set(compiler "${SELFHOST}")
  endif()
  foreach(target IN ITEMS windows-x64 linux-x64 linux-arm64 macos-arm64)
    execute_process(COMMAND "${compiler}" check
      "${SOURCE_DIR}/tests/fixtures/rocket3_bounded_state_surface.rocket"
      --target "${target}"
      WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE ui_result
      OUTPUT_VARIABLE ui_output ERROR_VARIABLE ui_error)
    if(NOT ui_result EQUAL 0)
      message(FATAL_ERROR "WP30 ${compiler_name} ${target} UI surface failed:\n${ui_output}${ui_error}")
    endif()
    execute_process(COMMAND "${compiler}" check "${asset_package}"
      --target "${target}"
      WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE asset_result
      OUTPUT_VARIABLE asset_output ERROR_VARIABLE asset_error)
    if(NOT asset_result EQUAL 0)
      message(FATAL_ERROR "WP30 ${compiler_name} ${target} asset surface failed:\n${asset_output}${asset_error}")
    endif()
  endforeach()
endforeach()
message(STATUS "WP30 bounded UI/cache surface passed in stage0/self-host for four targets")

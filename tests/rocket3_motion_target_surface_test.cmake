if(NOT DEFINED ROCKETC OR NOT DEFINED SELFHOST OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP13 motion target-surface test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
foreach(compiler_name IN ITEMS stage0 selfhost)
  if(compiler_name STREQUAL "stage0")
    set(compiler "${ROCKETC}")
  else()
    set(compiler "${SELFHOST}")
  endif()
  foreach(target IN ITEMS windows-x64 linux-x64 linux-arm64 macos-arm64)
    execute_process(COMMAND "${compiler}" check
      "${SOURCE_DIR}/tests/fixtures/rocket3_motion_named.rocket" --target "${target}"
      WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE check_result
      OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
    if(NOT check_result EQUAL 0)
      message(FATAL_ERROR "WP13 motion ${compiler_name} target ${target} surface failed:\n${check_output}${check_error}")
    endif()
    if(compiler_name STREQUAL "stage0")
      execute_process(COMMAND "${compiler}" emit-ir
        "${SOURCE_DIR}/tests/fixtures/rocket3_motion_named.rocket" --target "${target}"
        WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE ir_result
        OUTPUT_VARIABLE ir_output ERROR_VARIABLE ir_error)
      if(NOT ir_result EQUAL 0)
        message(FATAL_ERROR "WP13 motion stage0 target ${target} LLVM lowering failed:\n${ir_output}${ir_error}")
      endif()
    endif()
  endforeach()
endforeach()
message(STATUS "WP13 motion public source surface passed in stage0/self-host for four targets")

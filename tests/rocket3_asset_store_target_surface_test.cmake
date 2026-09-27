if(NOT DEFINED ROCKETC OR NOT DEFINED SELFHOST OR NOT DEFINED SOURCE_DIR OR
   NOT DEFINED WORK)
  message(FATAL_ERROR "WP28 asset-store target-surface test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
set(package "${SOURCE_DIR}/examples/raylib_showcase")
set(canonical_package "${SOURCE_DIR}/tests/fixtures/rocket35_assets_package")
foreach(compiler_name IN ITEMS stage0 selfhost)
  if(compiler_name STREQUAL "stage0")
    set(compiler "${ROCKETC}")
  else()
    set(compiler "${SELFHOST}")
  endif()
  foreach(target IN ITEMS windows-x64 linux-x64 linux-arm64 macos-arm64)
    execute_process(COMMAND "${compiler}" check "${package}" --target "${target}"
      WORKING_DIRECTORY "${SOURCE_DIR}"
      RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
    if(NOT check_result EQUAL 0)
      message(FATAL_ERROR "WP28 asset-store ${compiler_name} ${target} surface failed:\n${check_output}${check_error}")
    endif()
    execute_process(COMMAND "${compiler}" check "${canonical_package}" --target "${target}"
      WORKING_DIRECTORY "${SOURCE_DIR}"
      RESULT_VARIABLE canonical_result OUTPUT_VARIABLE canonical_output ERROR_VARIABLE canonical_error)
    if(NOT canonical_result EQUAL 0)
      message(FATAL_ERROR "WP5 asset-store ${compiler_name} ${target} surface failed:\n${canonical_output}${canonical_error}")
    endif()
  endforeach()
endforeach()
message(STATUS "WP28 asset-store source surface passed in stage0/self-host for four targets")

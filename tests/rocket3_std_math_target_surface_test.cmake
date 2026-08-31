if(NOT DEFINED ROCKETC OR NOT DEFINED SELFHOST OR NOT DEFINED SOURCE_DIR OR NOT DEFINED WORK)
  message(FATAL_ERROR "WP12 std.math target-surface test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
set(expected_runtime_symbols
  pi tau e abs abs_int min max min_int max_int clamp clamp_int sign sign_int
  floor ceil round trunc fract sqrt pow exp log log10 sin cos tan asin acos atan
  atan2 radians degrees lerp inverse_lerp remap smoothstep smootherstep approach
  move_towards)
foreach(compiler_name IN ITEMS stage0 selfhost)
  if(compiler_name STREQUAL "stage0")
    set(compiler "${ROCKETC}")
  else()
    set(compiler "${SELFHOST}")
  endif()
  foreach(target IN ITEMS windows-x64 linux-x64 linux-arm64 macos-arm64)
    execute_process(COMMAND "${compiler}" check
      "${SOURCE_DIR}/tests/fixtures/rocket3_std_math_named.rocket" --target "${target}"
      WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE check_result
      OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
    if(NOT check_result EQUAL 0)
      message(FATAL_ERROR "WP12 std.math ${compiler_name} target ${target} surface failed:\n${check_output}${check_error}")
    endif()
    if(compiler_name STREQUAL "stage0")
      execute_process(COMMAND "${compiler}" emit-ir
        "${SOURCE_DIR}/tests/fixtures/rocket3_std_math_named.rocket" --target "${target}"
        WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE ir_result
        OUTPUT_VARIABLE ir_output ERROR_VARIABLE ir_error)
      if(NOT ir_result EQUAL 0)
        message(FATAL_ERROR "WP12 std.math stage0 target ${target} LLVM lowering failed:\n${ir_output}${ir_error}")
      endif()
      foreach(runtime_symbol IN LISTS expected_runtime_symbols)
        if(NOT ir_output MATCHES "rocket_std_math_${runtime_symbol}")
          message(FATAL_ERROR "WP12 std.math target ${target} LLVM lowering omitted rocket_std_math_${runtime_symbol}:\n${ir_output}")
        endif()
      endforeach()
      if(target STREQUAL "windows-x64")
        set(expected_triple "x86_64-pc-windows-msvc")
      elseif(target STREQUAL "linux-x64")
        set(expected_triple "x86_64-unknown-linux-gnu")
      elseif(target STREQUAL "linux-arm64")
        set(expected_triple "aarch64-unknown-linux-gnu")
      else()
        set(expected_triple "arm64-apple-macosx")
      endif()
      if(NOT ir_output MATCHES "target triple = \"${expected_triple}\"")
        message(FATAL_ERROR "WP12 std.math target ${target} emitted the wrong LLVM triple:\n${ir_output}")
      endif()
    endif()
  endforeach()
endforeach()
message(STATUS "WP12 std.math named source surface passed in stage0/self-host for four targets; stage0 LLVM lowering exposed the complete surface for every target triple")

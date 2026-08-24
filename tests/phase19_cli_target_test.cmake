function(run_checked label)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${label} failed (${result}):\n${output}${error}")
  endif()
  set(LAST_OUTPUT "${output}${error}" PARENT_SCOPE)
endfunction()

run_checked("native target query" "${ROCKETC}" target)
string(STRIP "${LAST_OUTPUT}" host_alias)
if(NOT host_alias MATCHES "^(windows-x64|linux-x64|linux-arm64|macos-arm64)$")
  message(FATAL_ERROR "unexpected native target output: ${LAST_OUTPUT}")
endif()

if(host_alias STREQUAL "windows-x64")
  set(query_alias linux-arm64)
  set(query_triple aarch64-unknown-linux-gnu)
  set(query_os linux)
  set(query_architecture arm64)
  set(query_features "threads,dynamic-libraries,dwarf,neon")
  set(query_cross true)
elseif(host_alias STREQUAL "linux-x64")
  set(query_alias windows-x64)
  set(query_triple x86_64-pc-windows-msvc)
  set(query_os windows)
  set(query_architecture x64)
  set(query_features "threads,dynamic-libraries,codeview,sse2")
  set(query_cross true)
elseif(host_alias STREQUAL "linux-arm64")
  set(query_alias windows-x64)
  set(query_triple x86_64-pc-windows-msvc)
  set(query_os windows)
  set(query_architecture x64)
  set(query_features "threads,dynamic-libraries,codeview,sse2")
  set(query_cross false)
else()
  set(query_alias linux-x64)
  set(query_triple x86_64-unknown-linux-gnu)
  set(query_os linux)
  set(query_architecture x64)
  set(query_features "threads,dynamic-libraries,dwarf,sse2")
  set(query_cross false)
endif()

run_checked("triple normalization" "${ROCKETC}" target --target
            "${query_triple}" --verbose)
foreach(expected IN ITEMS
    "host: ${host_alias}" "target: ${query_alias}"
    "triple: ${query_triple}" "os: ${query_os}"
    "architecture: ${query_architecture}" "features: ${query_features}"
    "native: false" "cross-supported: ${query_cross}")
  string(FIND "${LAST_OUTPUT}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "target query omitted '${expected}':\n${LAST_OUTPUT}")
  endif()
endforeach()

run_checked("target overlay check" "${ROCKETC}" check "${PACKAGE}"
            --target linux-arm64)

execute_process(
  COMMAND "${ROCKETC}" target --target WINDOWS-X64
  RESULT_VARIABLE unknown_result OUTPUT_VARIABLE unknown_output
  ERROR_VARIABLE unknown_error)
if(unknown_result EQUAL 0 OR
   NOT "${unknown_output}${unknown_error}" MATCHES "error\\[R6001\\]: unknown target 'WINDOWS-X64'")
  message(FATAL_ERROR "unknown target did not produce stable R6001:\n${unknown_output}${unknown_error}")
endif()

execute_process(
  COMMAND "${ROCKETC}" target --target windows-arm64
  RESULT_VARIABLE unsupported_result OUTPUT_VARIABLE unsupported_output
  ERROR_VARIABLE unsupported_error)
if(unsupported_result EQUAL 0 OR
   NOT "${unsupported_output}${unsupported_error}" MATCHES "error\\[R6002\\]")
  message(FATAL_ERROR "Windows ARM64 did not produce stable R6002:\n${unsupported_output}${unsupported_error}")
endif()

execute_process(
  COMMAND "${ROCKETC}" run "${PACKAGE}" --target "${query_alias}"
  RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error)
if(run_result EQUAL 0 OR
   NOT "${run_output}${run_error}" MATCHES "error\\[R6004\\]: cannot execute target '${query_alias}' on host '${host_alias}'")
  message(FATAL_ERROR "cross execution did not produce stable R6004:\n${run_output}${run_error}")
endif()

message(STATUS "Phase 19 target CLI and conditional-source checks passed")

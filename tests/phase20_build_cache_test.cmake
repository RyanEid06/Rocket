if(NOT DEFINED COMPILER OR NOT DEFINED SOURCE OR NOT DEFINED WORK)
  message(FATAL_ERROR "phase20 build-cache test requires COMPILER, SOURCE, and WORK")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${SOURCE}/" DESTINATION "${WORK}")
# The repository fixture may have ignored artifacts from an earlier matrix.
# Cache tests must always begin from source-only package state.
file(REMOVE_RECURSE "${WORK}/.rocketc")

function(run_build output_name)
  execute_process(
    COMMAND "${COMPILER}" build "${WORK}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "cached build failed (${status}):\n${output}\n${error}")
  endif()
  set(${output_name} "${output}\n${error}" PARENT_SCOPE)
endfunction()

run_build(first)
if(first MATCHES "cache hit")
  message(FATAL_ERROR "first package build unexpectedly hit a cache")
endif()

run_build(second)
if(NOT second MATCHES "cache hit")
  message(FATAL_ERROR "unchanged package build did not hit rocket-build-cache-1")
endif()

file(APPEND "${WORK}/src/math.rocket" "# invalidate the conservative package key\n")
run_build(third)
if(third MATCHES "cache hit")
  message(FATAL_ERROR "source change did not invalidate rocket-build-cache-1")
endif()

run_build(fourth)
if(NOT fourth MATCHES "cache hit")
  message(FATAL_ERROR "rebuilt package was not reusable from the cache")
endif()

execute_process(
  COMMAND "${COMPILER}" run "${WORK}"
  RESULT_VARIABLE run_status
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error)
if(NOT run_status EQUAL 0 OR NOT run_output MATCHES "42")
  message(FATAL_ERROR "cached executable failed (${run_status}):\n${run_output}\n${run_error}")
endif()

message(STATUS "rocket-build-cache-1 invalidation and reuse passed")

if(NOT DEFINED COMPILER OR NOT DEFINED WORK)
  message(FATAL_ERROR "COMPILER and WORK are required")
endif()

file(REMOVE_RECURSE "${WORK}")
execute_process(
  COMMAND "${COMPILER}" new "${WORK}"
  RESULT_VARIABLE NEW_RESULT
  OUTPUT_VARIABLE NEW_OUTPUT
  ERROR_VARIABLE NEW_ERROR)
if(NOT NEW_RESULT EQUAL 0)
  message(FATAL_ERROR
    "self-hosted new failed (${NEW_RESULT})\n${NEW_OUTPUT}\n${NEW_ERROR}")
endif()

set(MANIFEST "${WORK}/rocket.toml")
if(NOT EXISTS "${MANIFEST}")
  message(FATAL_ERROR "self-hosted new did not create rocket.toml")
endif()
file(READ "${MANIFEST}" MANIFEST_TEXT)
if(NOT MANIFEST_TEXT MATCHES "version = \"0\\.1\\.0\"")
  message(FATAL_ERROR
    "self-hosted new must preserve the frozen 0.1.0 package scaffold\n${MANIFEST_TEXT}")
endif()

message(STATUS "self-hosted package scaffold preserves version 0.1.0")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/src")
configure_file("${COMPILER_SOURCE}" "${WORK}/src/main.rocket" COPYONLY)
configure_file("${COMPILER_MANIFEST}" "${WORK}/rocket.toml" COPYONLY)
file(COPY "${PACKAGE}/" DESTINATION "${WORK}/stage0-package"
     PATTERN ".rocketc" EXCLUDE)
file(COPY "${PACKAGE}/" DESTINATION "${WORK}/selfhost-package"
     PATTERN ".rocketc" EXCLUDE)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" build "${WORK}"
  RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "isolated self-hosted compiler build failed:\n${build_output}${build_error}")
endif()
set(SELFHOST "${WORK}/.rocketc/targets/${HOST_ALIAS}/main${EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${SELFHOST}")
  message(FATAL_ERROR "isolated self-hosted compiler artifact is missing")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" build "${WORK}/stage0-package" --target "${HOST_ALIAS}"
  RESULT_VARIABLE stage0_build_result OUTPUT_VARIABLE stage0_build_output
  ERROR_VARIABLE stage0_build_error)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "ROCKET_STAGE0=${STAGE0}"
          "ROCKET_CLANG=${CLANG}" "ROCKET_RUNTIME=${RUNTIME}"
          "${SELFHOST}" build "${WORK}/selfhost-package" --target "${HOST_ALIAS}"
  RESULT_VARIABLE selfhost_build_result OUTPUT_VARIABLE selfhost_build_output
  ERROR_VARIABLE selfhost_build_error)
set(STAGE0_ARTIFACT
    "${WORK}/stage0-package/.rocketc/targets/${HOST_ALIAS}/main${EXECUTABLE_SUFFIX}")
set(SELFHOST_ARTIFACT
    "${WORK}/selfhost-package/.rocketc/targets/${HOST_ALIAS}/main${EXECUTABLE_SUFFIX}")
if(NOT stage0_build_result EQUAL 0 OR NOT selfhost_build_result EQUAL 0 OR
   NOT EXISTS "${STAGE0_ARTIFACT}" OR NOT EXISTS "${SELFHOST_ARTIFACT}" OR
   EXISTS "${WORK}/stage0-package/.rocketc/main${EXECUTABLE_SUFFIX}" OR
   EXISTS "${WORK}/selfhost-package/.rocketc/main${EXECUTABLE_SUFFIX}")
  message(FATAL_ERROR
    "target-qualified native build parity failed:\n"
    "stage0=${stage0_build_output}${stage0_build_error}\n"
    "selfhost=${selfhost_build_output}${selfhost_build_error}")
endif()
execute_process(COMMAND "${STAGE0_ARTIFACT}" RESULT_VARIABLE stage0_run_result)
execute_process(COMMAND "${SELFHOST_ARTIFACT}" RESULT_VARIABLE selfhost_run_result)
if(NOT stage0_run_result EQUAL 0 OR NOT selfhost_run_result EQUAL 0)
  message(FATAL_ERROR "target-qualified native artifacts did not execute")
endif()

if(HOST_ALIAS STREQUAL "windows-x64")
  set(CROSS_TARGET linux-arm64)
  set(CROSS_DIAGNOSTIC R6003)
elseif(HOST_ALIAS STREQUAL "linux-x64")
  set(CROSS_TARGET windows-x64)
  set(CROSS_DIAGNOSTIC R6003)
elseif(HOST_ALIAS STREQUAL "linux-arm64")
  set(CROSS_TARGET windows-x64)
  set(CROSS_DIAGNOSTIC R6004)
else()
  set(CROSS_TARGET linux-x64)
  set(CROSS_DIAGNOSTIC R6004)
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" build "${WORK}/stage0-package" --target "${CROSS_TARGET}"
  RESULT_VARIABLE stage0_cross_result OUTPUT_VARIABLE stage0_cross_output
  ERROR_VARIABLE stage0_cross_error)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "ROCKET_STAGE0=${STAGE0}"
          "${SELFHOST}" build "${WORK}/selfhost-package" --target "${CROSS_TARGET}"
  RESULT_VARIABLE selfhost_cross_result OUTPUT_VARIABLE selfhost_cross_output
  ERROR_VARIABLE selfhost_cross_error)
set(stage0_cross "${stage0_cross_output}${stage0_cross_error}")
set(selfhost_cross "${selfhost_cross_output}${selfhost_cross_error}")
if(stage0_cross_result EQUAL 0 OR selfhost_cross_result EQUAL 0 OR
   NOT stage0_cross STREQUAL selfhost_cross OR
   NOT stage0_cross MATCHES "error\\[${CROSS_DIAGNOSTIC}\\]")
  message(FATAL_ERROR
    "stage0/self-host cross-build diagnostics differ:\n"
    "stage0=${stage0_cross}\nselfhost=${selfhost_cross}")
endif()

file(MAKE_DIRECTORY "${WORK}/incomplete-target-sdk")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" build "${WORK}/stage0-package" --target "${CROSS_TARGET}"
          --target-sdk "${WORK}/incomplete-target-sdk"
  RESULT_VARIABLE stage0_sdk_result OUTPUT_VARIABLE stage0_sdk_output
  ERROR_VARIABLE stage0_sdk_error)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "ROCKET_STAGE0=${STAGE0}"
          "${SELFHOST}" build "${WORK}/selfhost-package" --target "${CROSS_TARGET}"
          --target-sdk "${WORK}/incomplete-target-sdk"
  RESULT_VARIABLE selfhost_sdk_result OUTPUT_VARIABLE selfhost_sdk_output
  ERROR_VARIABLE selfhost_sdk_error)
set(stage0_sdk "${stage0_sdk_output}${stage0_sdk_error}")
set(selfhost_sdk "${selfhost_sdk_output}${selfhost_sdk_error}")
if(stage0_sdk_result EQUAL 0 OR selfhost_sdk_result EQUAL 0 OR
   NOT stage0_sdk STREQUAL selfhost_sdk OR
   NOT stage0_sdk MATCHES "error\\[${CROSS_DIAGNOSTIC}\\]")
  message(FATAL_ERROR
    "stage0/self-host configured-SDK diagnostics differ:\n"
    "stage0=${stage0_sdk}\nselfhost=${selfhost_sdk}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" run "${WORK}/stage0-package" --target "${CROSS_TARGET}"
  RESULT_VARIABLE stage0_run_cross_result OUTPUT_VARIABLE stage0_run_cross_output
  ERROR_VARIABLE stage0_run_cross_error)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "ROCKET_STAGE0=${STAGE0}"
          "${SELFHOST}" run "${WORK}/selfhost-package" --target "${CROSS_TARGET}"
  RESULT_VARIABLE selfhost_run_cross_result OUTPUT_VARIABLE selfhost_run_cross_output
  ERROR_VARIABLE selfhost_run_cross_error)
set(stage0_run_cross "${stage0_run_cross_output}${stage0_run_cross_error}")
set(selfhost_run_cross "${selfhost_run_cross_output}${selfhost_run_cross_error}")
if(stage0_run_cross_result EQUAL 0 OR selfhost_run_cross_result EQUAL 0 OR
   NOT stage0_run_cross STREQUAL selfhost_run_cross OR
   NOT stage0_run_cross MATCHES "error\\[R6004\\]")
  message(FATAL_ERROR
    "stage0/self-host cross-run diagnostics differ:\n"
    "stage0=${stage0_run_cross}\nselfhost=${selfhost_run_cross}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" target --target aarch64-unknown-linux-gnu --verbose
  RESULT_VARIABLE stage0_result OUTPUT_VARIABLE stage0_output ERROR_VARIABLE stage0_error)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "ROCKET_STAGE0=${STAGE0}"
          "${SELFHOST}" target --target aarch64-unknown-linux-gnu --verbose
  RESULT_VARIABLE selfhost_result OUTPUT_VARIABLE selfhost_output
  ERROR_VARIABLE selfhost_error)
if(NOT stage0_result EQUAL 0 OR NOT selfhost_result EQUAL 0 OR
   NOT stage0_output STREQUAL selfhost_output OR
   NOT stage0_error STREQUAL selfhost_error)
  message(FATAL_ERROR
    "stage0/self-host target output differs:\n"
    "stage0=${stage0_output}${stage0_error}\n"
    "selfhost=${selfhost_output}${selfhost_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" target --target WINDOWS-X64
  RESULT_VARIABLE stage0_unknown_result OUTPUT_VARIABLE stage0_unknown_output
  ERROR_VARIABLE stage0_unknown_error)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "ROCKET_STAGE0=${STAGE0}"
          "${SELFHOST}" target --target WINDOWS-X64
  RESULT_VARIABLE selfhost_unknown_result OUTPUT_VARIABLE selfhost_unknown_output
  ERROR_VARIABLE selfhost_unknown_error)
if(stage0_unknown_result EQUAL 0 OR selfhost_unknown_result EQUAL 0)
  message(FATAL_ERROR "unknown target unexpectedly succeeded")
endif()
set(stage0_unknown "${stage0_unknown_output}${stage0_unknown_error}")
set(selfhost_unknown "${selfhost_unknown_output}${selfhost_unknown_error}")
if(NOT stage0_unknown STREQUAL selfhost_unknown OR
   NOT stage0_unknown MATCHES "error\\[R6001\\]")
  message(FATAL_ERROR
    "stage0/self-host R6001 output differs:\n"
    "stage0=${stage0_unknown}\nselfhost=${selfhost_unknown}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "${STAGE0}" check "${PACKAGE}" --target linux-arm64
  RESULT_VARIABLE stage0_check_result OUTPUT_VARIABLE stage0_check_output
  ERROR_VARIABLE stage0_check_error)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=ROCKET_ARTIFACT_ROOT
          "ROCKET_STAGE0=${STAGE0}"
          "${SELFHOST}" check "${PACKAGE}" --target linux-arm64
  RESULT_VARIABLE selfhost_check_result OUTPUT_VARIABLE selfhost_check_output
  ERROR_VARIABLE selfhost_check_error)
if(NOT stage0_check_result EQUAL 0 OR NOT selfhost_check_result EQUAL 0)
  message(FATAL_ERROR
    "target overlay parity check failed:\n"
    "stage0=${stage0_check_output}${stage0_check_error}\n"
    "selfhost=${selfhost_check_output}${selfhost_check_error}")
endif()

message(STATUS "Phase 19 stage0/self-host target parity passed")

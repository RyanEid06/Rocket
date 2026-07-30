if(NOT DEFINED STAGE0 OR NOT DEFINED SELFHOST OR NOT DEFINED PACKAGE OR
   NOT DEFINED HEADER OR NOT DEFINED EXPECTED_BINDINGS OR NOT DEFINED WORK)
  message(FATAL_ERROR "Phase 13 generation test is missing required arguments")
endif()
file(MAKE_DIRECTORY "${WORK}")

set(stage0_header "${WORK}/stage0.h")
set(stage0_header_repeat "${WORK}/stage0-repeat.h")
set(selfhost_header "${WORK}/selfhost.h")
foreach(output IN ITEMS "${stage0_header}" "${stage0_header_repeat}")
  execute_process(COMMAND "${STAGE0}" emit-header "${PACKAGE}" --output "${output}"
                  RESULT_VARIABLE status ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "stage0 header generation failed: ${error}")
  endif()
endforeach()
execute_process(COMMAND "${SELFHOST}" emit-header "${PACKAGE}" --output "${selfhost_header}"
                RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "self-hosted header generation failed: ${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${stage0_header}" "${stage0_header_repeat}"
                RESULT_VARIABLE repeat_status)
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${stage0_header}" "${selfhost_header}"
                RESULT_VARIABLE parity_status)
if(NOT repeat_status EQUAL 0 OR NOT parity_status EQUAL 0)
  message(FATAL_ERROR "generated C headers are not byte deterministic across stage0/self-host")
endif()

set(stage0_bindings "${WORK}/stage0.rocket")
set(stage0_bindings_repeat "${WORK}/stage0-repeat.rocket")
set(selfhost_bindings "${WORK}/selfhost.rocket")
execute_process(COMMAND "${STAGE0}" bind "${HEADER}" --output "${stage0_bindings}"
                RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "stage0 binding generation failed: ${error}")
endif()
execute_process(COMMAND "${STAGE0}" bind "${HEADER}" --output "${stage0_bindings_repeat}"
                RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "repeat stage0 binding generation failed: ${error}")
endif()
execute_process(COMMAND "${SELFHOST}" bind "${HEADER}" --output "${selfhost_bindings}"
                RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "self-hosted binding generation failed: ${error}")
endif()
foreach(candidate IN ITEMS "${stage0_bindings_repeat}" "${selfhost_bindings}" "${EXPECTED_BINDINGS}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                          "${stage0_bindings}" "${candidate}"
                  RESULT_VARIABLE comparison)
  if(NOT comparison EQUAL 0)
    message(FATAL_ERROR "generated Rocket bindings differ: ${candidate}")
  endif()
endforeach()
message(STATUS "Phase 13 headers and bindings are deterministic and stage-parallel")

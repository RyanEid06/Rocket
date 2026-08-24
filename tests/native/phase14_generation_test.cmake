if(NOT DEFINED STAGE0 OR NOT DEFINED SELFHOST OR NOT DEFINED HEADER OR
   NOT DEFINED GENERATED OR NOT DEFINED VENDORED OR NOT DEFINED WORK)
  message(FATAL_ERROR "Phase 14 generation test is missing required arguments")
endif()

file(MAKE_DIRECTORY "${WORK}")
set(stage0 "${WORK}/stage0.rocket")
set(stage0_repeat "${WORK}/stage0-repeat.rocket")
set(selfhost "${WORK}/selfhost.rocket")

foreach(output IN ITEMS "${stage0}" "${stage0_repeat}")
  execute_process(COMMAND "${STAGE0}" bind "${HEADER}" --output "${output}"
                  RESULT_VARIABLE status ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "stage0 raylib binding generation failed: ${error}")
  endif()
endforeach()

execute_process(COMMAND "${SELFHOST}" bind "${HEADER}" --output "${selfhost}"
                RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "self-hosted raylib binding generation failed: ${error}")
endif()

foreach(candidate IN ITEMS "${stage0_repeat}" "${selfhost}" "${GENERATED}"
                           "${VENDORED}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                          "${stage0}" "${candidate}"
                  RESULT_VARIABLE comparison)
  if(NOT comparison EQUAL 0)
    message(FATAL_ERROR "raylib binding output is not deterministic: ${candidate}")
  endif()
endforeach()

file(SHA256 "${stage0}" binding_hash)
message(STATUS "Phase 14 raylib bindings are deterministic: ${binding_hash}")

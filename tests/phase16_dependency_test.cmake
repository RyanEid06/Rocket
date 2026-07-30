if(NOT DEFINED ROCKETC OR NOT DEFINED FIXTURE OR NOT DEFINED WORK)
  message(FATAL_ERROR "Phase 16 dependency test is missing required arguments")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/" DESTINATION "${WORK}")
set(APP "${WORK}/app")

execute_process(COMMAND "${ROCKETC}" resolve "${APP}"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE RESOLVE_OUTPUT ERROR_VARIABLE RESOLVE_ERROR)
if(NOT STATUS EQUAL 0 OR NOT RESOLVE_OUTPUT MATCHES "resolved 3 package")
  message(FATAL_ERROR "Phase 16 resolve failed: ${RESOLVE_OUTPUT}${RESOLVE_ERROR}")
endif()

file(SHA256 "${APP}/rocket.lock" FIRST_LOCK_HASH)
execute_process(COMMAND "${ROCKETC}" resolve "${APP}" --locked
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE LOCKED_OUTPUT ERROR_VARIABLE LOCKED_ERROR)
if(NOT STATUS EQUAL 0 OR NOT LOCKED_OUTPUT MATCHES "locked resolution verified")
  message(FATAL_ERROR "Phase 16 locked resolve failed: ${LOCKED_OUTPUT}${LOCKED_ERROR}")
endif()
file(SHA256 "${APP}/rocket.lock" SECOND_LOCK_HASH)
if(NOT FIRST_LOCK_HASH STREQUAL SECOND_LOCK_HASH)
  message(FATAL_ERROR "Phase 16 lockfile is not deterministic")
endif()

file(READ "${APP}/rocket.toml" ORIGINAL_MANIFEST)
string(REPLACE "math = \"^1.0.0\"" "math = \"=1.0.0\""
  STALE_MANIFEST "${ORIGINAL_MANIFEST}")
file(WRITE "${APP}/rocket.toml" "${STALE_MANIFEST}")
execute_process(COMMAND "${ROCKETC}" resolve "${APP}" --locked
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE STALE_OUTPUT ERROR_VARIABLE STALE_ERROR)
if(STATUS EQUAL 0 OR NOT STALE_ERROR MATCHES "rocket.lock is stale")
  message(FATAL_ERROR "Phase 16 stale lockfile was accepted: ${STALE_OUTPUT}${STALE_ERROR}")
endif()
file(WRITE "${APP}/rocket.toml" "${ORIGINAL_MANIFEST}")

execute_process(COMMAND "${ROCKETC}" tree "${APP}"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE TREE_OUTPUT ERROR_VARIABLE TREE_ERROR)
if(NOT STATUS EQUAL 0 OR NOT TREE_OUTPUT MATCHES "math@1.2.0" OR
   NOT TREE_OUTPUT MATCHES "utility@1.0.0")
  message(FATAL_ERROR "Phase 16 dependency tree failed: ${TREE_OUTPUT}${TREE_ERROR}")
endif()

execute_process(COMMAND "${ROCKETC}" audit "${APP}"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE AUDIT_OUTPUT ERROR_VARIABLE AUDIT_ERROR)
if(NOT STATUS EQUAL 0 OR NOT AUDIT_OUTPUT MATCHES "SHA-256 cache verified")
  message(FATAL_ERROR "Phase 16 audit failed: ${AUDIT_OUTPUT}${AUDIT_ERROR}")
endif()

file(REMOVE_RECURSE "${WORK}/registry")
file(REMOVE_RECURSE "${WORK}/local_text")
execute_process(COMMAND "${ROCKETC}" resolve "${APP}" --offline
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OFFLINE_OUTPUT ERROR_VARIABLE OFFLINE_ERROR)
if(NOT STATUS EQUAL 0 OR NOT OFFLINE_OUTPUT MATCHES "offline resolution verified")
  message(FATAL_ERROR "Phase 16 offline resolve failed: ${OFFLINE_OUTPUT}${OFFLINE_ERROR}")
endif()

file(GLOB CACHE_ENTRIES LIST_DIRECTORIES true "${APP}/.rocketc/cache/sha256/*")
list(GET CACHE_ENTRIES 0 POISONED_CACHE)
file(APPEND "${POISONED_CACHE}/rocket.toml" "# poisoned\n")
execute_process(COMMAND "${ROCKETC}" resolve "${APP}" --offline
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE POISON_OUTPUT ERROR_VARIABLE POISON_ERROR)
if(STATUS EQUAL 0 OR NOT POISON_ERROR MATCHES "checksum mismatch")
  message(FATAL_ERROR "Phase 16 poisoned cache was accepted: ${POISON_OUTPUT}${POISON_ERROR}")
endif()

message(STATUS "Phase 16 dependency workflow passed")

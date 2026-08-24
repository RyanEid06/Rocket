if(NOT DEFINED ROCKETC OR NOT DEFINED WORK)
  message(FATAL_ERROR "Phase 16 ecosystem test is missing required arguments")
endif()

if(NOT DEFINED ENV{ROCKET_ARTIFACT_ROOT} OR
   NOT DEFINED ENV{ROCKET_NATIVE_TARGET})
  message(FATAL_ERROR "Phase 16 ecosystem test requires isolated artifact settings")
endif()
set(ARTIFACT_ROOT "$ENV{ROCKET_ARTIFACT_ROOT}")
set(NATIVE_TARGET "$ENV{ROCKET_NATIVE_TARGET}")
if(NATIVE_TARGET STREQUAL "windows-x64")
  set(EXECUTABLE_SUFFIX ".exe")
  set(STATIC_SUFFIX ".lib")
  set(OBJECT_SUFFIX ".obj")
else()
  set(EXECUTABLE_SUFFIX "")
  set(STATIC_SUFFIX ".a")
  set(OBJECT_SUFFIX ".o")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(REGISTRY "${WORK}/registry")
file(TO_CMAKE_PATH "${REGISTRY}" REGISTRY_URL_PATH)
set(REGISTRY_URL "file://${REGISTRY_URL_PATH}")
set(TOKEN "phase16-owner.phase16-test-secret")
file(WRITE "${WORK}/token.txt" "${TOKEN}\n")

execute_process(COMMAND "${ROCKETC}" registry init "${REGISTRY}"
  --id phase16 --owner acme --token-stdin INPUT_FILE "${WORK}/token.txt"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE INIT_OUTPUT ERROR_VARIABLE INIT_ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "registry init failed: ${INIT_OUTPUT}${INIT_ERROR}")
endif()
string(REGEX MATCH "registry-key = \"(sha256:[0-9a-f]+)\"" KEY_MATCH "${INIT_OUTPUT}")
set(REGISTRY_KEY "${CMAKE_MATCH_1}")
string(LENGTH "${REGISTRY_KEY}" KEY_LENGTH)
if(NOT KEY_LENGTH EQUAL 71)
  message(FATAL_ERROR "registry init returned no signing-key fingerprint: ${INIT_OUTPUT}")
endif()
execute_process(COMMAND "${ROCKETC}" login "${REGISTRY_URL}" --token-stdin
  INPUT_FILE "${WORK}/token.txt" RESULT_VARIABLE STATUS
  OUTPUT_VARIABLE LOGIN_OUTPUT ERROR_VARIABLE LOGIN_ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "registry login failed: ${LOGIN_OUTPUT}${LOGIN_ERROR}")
endif()

function(write_package DIRECTORY NAME VERSION BODY DEPENDENCIES BUILD_KIND)
  file(MAKE_DIRECTORY "${DIRECTORY}/src")
  file(WRITE "${DIRECTORY}/src/main.rocket" "${BODY}")
  set(MANIFEST "[package]\nnamespace = \"acme\"\nname = \"${NAME}\"\nversion = \"${VERSION}\"\nlicense = \"MIT\"\nentry = \"src/main.rocket\"\nregistry = \"${REGISTRY_URL}\"\nregistry-key = \"${REGISTRY_KEY}\"\n")
  if(NOT "${BUILD_KIND}" STREQUAL "")
    string(APPEND MANIFEST "\n[build]\nkind = \"${BUILD_KIND}\"\nname = \"${NAME}\"\n")
  endif()
  if(NOT "${DEPENDENCIES}" STREQUAL "")
    string(APPEND MANIFEST "\n[dependencies]\n${DEPENDENCIES}")
  endif()
  file(WRITE "${DIRECTORY}/rocket.toml" "${MANIFEST}")
endfunction()

write_package("${WORK}/utility" utility 1.0.0
  "pub fn increment(value: Int) -> Int:\n    return value + 1\n" "" "")
file(MAKE_DIRECTORY "${REGISTRY}/packages/acme/utility")
file(MAKE_DIRECTORY "${REGISTRY}/index")
file(WRITE "${REGISTRY}/packages/acme/utility/1.0.0.tar.partial-interrupted" "partial")
file(WRITE "${REGISTRY}/index/utility.toml.partial-interrupted" "partial")
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/utility"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "published acme/utility@1.0.0")
  message(FATAL_ERROR "utility publish failed: ${OUTPUT}${ERROR}")
endif()
if(EXISTS "${REGISTRY}/packages/acme/utility/1.0.0.tar.partial-interrupted" OR
   EXISTS "${REGISTRY}/index/utility.toml.partial-interrupted")
  message(FATAL_ERROR "publish did not recover interrupted registry transactions")
endif()
if(NOT EXISTS "${REGISTRY}/docs/acme/utility/1.0.0/index.html" OR
   NOT EXISTS "${REGISTRY}/docs/acme/utility/1.0.0/search.json")
  message(FATAL_ERROR "publish did not install deterministic package documentation")
endif()
execute_process(COMMAND "${ROCKETC}" doc "${WORK}/utility"
  --output "${WORK}/utility-docs" RESULT_VARIABLE STATUS
  OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "1 public API item")
  message(FATAL_ERROR "package documentation failed: ${OUTPUT}${ERROR}")
endif()
file(SHA256 "${WORK}/utility-docs/index.html" FIRST_DOC_HASH)
execute_process(COMMAND "${ROCKETC}" doc "${WORK}/utility"
  --output "${WORK}/utility-docs" RESULT_VARIABLE STATUS)
file(SHA256 "${WORK}/utility-docs/index.html" SECOND_DOC_HASH)
if(NOT STATUS EQUAL 0 OR NOT FIRST_DOC_HASH STREQUAL SECOND_DOC_HASH)
  message(FATAL_ERROR "package documentation is not deterministic")
endif()

write_package("${WORK}/takeover" takeover 1.0.0
  "pub fn value() -> Int:\n    return 1\n" "" "")
file(READ "${WORK}/takeover/rocket.toml" TAKEOVER_MANIFEST)
string(REPLACE "namespace = \"acme\"" "namespace = \"unowned\""
  TAKEOVER_MANIFEST "${TAKEOVER_MANIFEST}")
file(WRITE "${WORK}/takeover/rocket.toml" "${TAKEOVER_MANIFEST}")
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/takeover"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "error\\[R5004\\].*namespace")
  message(FATAL_ERROR "namespace takeover was accepted: ${OUTPUT}${ERROR}")
endif()

write_package("${WORK}/typosquat" utilitx 1.0.0
  "pub fn value() -> Int:\n    return 1\n" "" "")
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/typosquat"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "error\\[R5004\\].*typosquatting")
  message(FATAL_ERROR "typosquatting package was accepted: ${OUTPUT}${ERROR}")
endif()

write_package("${WORK}/labels" labels 1.0.0
  "pub fn answer_label() -> String:\n    return \"answer\"\n" "" "")
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/labels"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "labels publish failed: ${OUTPUT}${ERROR}")
endif()

write_package("${WORK}/rogue" rogue 1.0.0
  "import labels\n\npub fn stolen() -> String:\n    return labels.answer_label()\n" "" "")
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/rogue"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "rogue fixture publish failed: ${OUTPUT}${ERROR}")
endif()
write_package("${WORK}/graph-bypass" graph_bypass 1.0.0
  "import rogue\n\nfn main() -> Int:\n    print(rogue.stolen())\n    return 0\n"
  "rogue = \"^1.0.0\"\nlabels = \"^1.0.0\"\n" "executable")
execute_process(COMMAND "${ROCKETC}" resolve "${WORK}/graph-bypass"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "graph-bypass fixture resolve failed: ${OUTPUT}${ERROR}")
endif()
execute_process(COMMAND "${ROCKETC}" check "${WORK}/graph-bypass"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "error\\[R3005\\].*not a declared edge")
  message(FATAL_ERROR "dependency import bypassed the exact graph: ${OUTPUT}${ERROR}")
endif()

write_package("${WORK}/math" math 1.0.0
  "import utility\nimport labels\n\npub fn answer() -> Int:\n    return utility.increment(41)\n\npub fn label() -> String:\n    return labels.answer_label()\n"
  "utility = \"^1.0.0\"\nlabels = \"^1.0.0\"\n" "")
execute_process(COMMAND "${ROCKETC}" resolve "${WORK}/math"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "math resolve failed: ${OUTPUT}${ERROR}")
endif()
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/math"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "math publish failed: ${OUTPUT}${ERROR}")
endif()

write_package("${WORK}/app" ecosystem_app 1.6.0
  "import math\n\nfn main() -> Int:\n    print(math.label())\n    print(math.answer())\n    return 0\n"
  "math = \"^1.0.0\"\n" "executable")
file(APPEND "${WORK}/app/rocket.toml"
  "\n[package-policy]\nallowed-licenses = \"MIT\"\ndeny-yanked = \"false\"\n")
execute_process(COMMAND "${ROCKETC}" resolve "${WORK}/app"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "resolved 3 package")
  message(FATAL_ERROR "application resolve failed: ${OUTPUT}${ERROR}")
endif()
execute_process(COMMAND "${ROCKETC}" run "${WORK}/app"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "answer[\r\n]+42")
  message(FATAL_ERROR "transitive application run failed: ${OUTPUT}${ERROR}")
endif()
set(APP_ARTIFACT
  "${ARTIFACT_ROOT}/ecosystem_app/.rocketc/targets/${NATIVE_TARGET}/ecosystem_app${EXECUTABLE_SUFFIX}")
file(SHA256 "${APP_ARTIFACT}" ONLINE_HASH)

write_package("${WORK}/library" ecosystem_library 1.6.0
  "import math\n\npub fn library_answer() -> Int:\n    return math.answer()\n"
  "math = \"^1.0.0\"\n" "static-library")
execute_process(COMMAND "${ROCKETC}" resolve "${WORK}/library"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
execute_process(COMMAND "${ROCKETC}" build "${WORK}/library"
  RESULT_VARIABLE BUILD_STATUS OUTPUT_VARIABLE BUILD_OUTPUT ERROR_VARIABLE BUILD_ERROR)
if(NOT STATUS EQUAL 0 OR NOT BUILD_STATUS EQUAL 0 OR
   NOT BUILD_OUTPUT MATCHES "ecosystem_library${STATIC_SUFFIX}")
  message(FATAL_ERROR "transitive library build failed: ${OUTPUT}${ERROR}${BUILD_OUTPUT}${BUILD_ERROR}")
endif()

execute_process(COMMAND "${ROCKETC}" registry yank "${REGISTRY_URL}"
  acme/math@1.0.0 --reason "ecosystem regression test"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "yanked acme/math@1.0.0")
  message(FATAL_ERROR "package yank failed: ${OUTPUT}${ERROR}")
endif()

execute_process(COMMAND "${ROCKETC}" audit "${WORK}/app"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "warning: yanked dependency acme/math@1.0.0" OR
   NOT OUTPUT MATCHES "provenance: phase16:acme/math@1.0.0")
  message(FATAL_ERROR "signed yank/provenance audit failed: ${OUTPUT}${ERROR}")
endif()

file(WRITE "${WORK}/advisory.toml"
  "id = \"RKT-TEST-0001\"\npackage = \"acme/math\"\naffected = \"=1.0.0\"\nseverity = \"compromised\"\nurl = \"https://example.invalid/RKT-TEST-0001\"\n")
execute_process(COMMAND "${ROCKETC}" registry advisory "${REGISTRY_URL}"
  "${WORK}/advisory.toml" RESULT_VARIABLE STATUS
  OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "signed advisory publication failed: ${OUTPUT}${ERROR}")
endif()
execute_process(COMMAND "${ROCKETC}" audit "${WORK}/app"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "error\\[R5005\\].*compromised dependency.*RKT-TEST-0001")
  message(FATAL_ERROR "compromised dependency was accepted: ${OUTPUT}${ERROR}")
endif()

file(COPY "${REGISTRY}/" DESTINATION "${WORK}/bad-signature-registry")
file(APPEND "${WORK}/bad-signature-registry/index/math.sig" "tampered")
file(COPY "${WORK}/app/" DESTINATION "${WORK}/bad-signature-app")
file(REMOVE_RECURSE "${WORK}/bad-signature-app/.rocketc")
file(READ "${WORK}/bad-signature-app/rocket.toml" BAD_SIGNATURE_MANIFEST)
file(TO_CMAKE_PATH "${WORK}/bad-signature-registry" BAD_SIGNATURE_PATH)
string(REPLACE "${REGISTRY_URL}" "file://${BAD_SIGNATURE_PATH}"
  BAD_SIGNATURE_MANIFEST "${BAD_SIGNATURE_MANIFEST}")
file(WRITE "${WORK}/bad-signature-app/rocket.toml" "${BAD_SIGNATURE_MANIFEST}")
file(READ "${WORK}/bad-signature-app/rocket.lock" BAD_SIGNATURE_LOCK)
string(REPLACE "${REGISTRY_URL}" "file://${BAD_SIGNATURE_PATH}"
  BAD_SIGNATURE_LOCK "${BAD_SIGNATURE_LOCK}")
file(WRITE "${WORK}/bad-signature-app/rocket.lock" "${BAD_SIGNATURE_LOCK}")
execute_process(COMMAND "${ROCKETC}" check "${WORK}/bad-signature-app"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "error\\[R5003\\].*signature")
  message(FATAL_ERROR "tampered registry signature was accepted: ${OUTPUT}${ERROR}")
endif()

file(COPY "${REGISTRY}/" DESTINATION "${WORK}/bad-checksum-registry")
file(APPEND "${WORK}/bad-checksum-registry/packages/acme/math/1.0.0.tar" "tampered")
file(COPY "${WORK}/app/" DESTINATION "${WORK}/bad-checksum-app")
file(REMOVE_RECURSE "${WORK}/bad-checksum-app/.rocketc")
file(READ "${WORK}/bad-checksum-app/rocket.toml" BAD_CHECKSUM_MANIFEST)
file(TO_CMAKE_PATH "${WORK}/bad-checksum-registry" BAD_CHECKSUM_PATH)
string(REPLACE "${REGISTRY_URL}" "file://${BAD_CHECKSUM_PATH}"
  BAD_CHECKSUM_MANIFEST "${BAD_CHECKSUM_MANIFEST}")
file(WRITE "${WORK}/bad-checksum-app/rocket.toml" "${BAD_CHECKSUM_MANIFEST}")
file(READ "${WORK}/bad-checksum-app/rocket.lock" BAD_CHECKSUM_LOCK)
string(REPLACE "${REGISTRY_URL}" "file://${BAD_CHECKSUM_PATH}"
  BAD_CHECKSUM_LOCK "${BAD_CHECKSUM_LOCK}")
file(WRITE "${WORK}/bad-checksum-app/rocket.lock" "${BAD_CHECKSUM_LOCK}")
execute_process(COMMAND "${ROCKETC}" check "${WORK}/bad-checksum-app"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "archive checksum")
  message(FATAL_ERROR "tampered package archive was accepted: ${OUTPUT}${ERROR}")
endif()

execute_process(COMMAND "${ROCKETC}" registry transfer "${REGISTRY_URL}"
  acme bob RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "transferred namespace acme to bob")
  message(FATAL_ERROR "namespace transfer failed: ${OUTPUT}${ERROR}")
endif()
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/utility"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "does not own the package namespace")
  message(FATAL_ERROR "former namespace owner retained publish authority: ${OUTPUT}${ERROR}")
endif()

execute_process(COMMAND "${ROCKETC}" registry revoke "${REGISTRY_URL}"
  phase16-owner RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "credential revocation failed: ${OUTPUT}${ERROR}")
endif()
execute_process(COMMAND "${ROCKETC}" publish "${WORK}/utility"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(STATUS EQUAL 0 OR NOT ERROR MATCHES "credential is revoked")
  message(FATAL_ERROR "revoked credential remained usable: ${OUTPUT}${ERROR}")
endif()

file(REMOVE "${APP_ARTIFACT}")
file(REMOVE
  "${ARTIFACT_ROOT}/ecosystem_app/.rocketc/targets/${NATIVE_TARGET}/ecosystem_app${OBJECT_SUFFIX}")
file(REMOVE_RECURSE "${REGISTRY}")
execute_process(COMMAND "${ROCKETC}" resolve "${WORK}/app" --offline
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0 OR NOT OUTPUT MATCHES "offline resolution verified")
  message(FATAL_ERROR "locked offline resolve failed: ${OUTPUT}${ERROR}")
endif()
execute_process(COMMAND "${ROCKETC}" build "${WORK}/app"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "locked offline build failed: ${OUTPUT}${ERROR}")
endif()
file(SHA256 "${APP_ARTIFACT}" OFFLINE_HASH)
if(NOT ONLINE_HASH STREQUAL OFFLINE_HASH)
  message(FATAL_ERROR "online and locked-offline artifacts differ")
endif()

execute_process(COMMAND "${ROCKETC}" logout "${REGISTRY_URL}"
  RESULT_VARIABLE STATUS OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "registry logout failed: ${OUTPUT}${ERROR}")
endif()
file(REMOVE "${WORK}/token.txt")
message(STATUS "Phase 16 signed package ecosystem workflow passed")

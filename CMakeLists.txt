cmake_minimum_required(VERSION 3.25)
project(Rocket VERSION 2.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
  # The release LLVM compiler searches its own libc++ 22 headers first, but
  # those headers are newer than the libc++ runtime supplied by Xcode 16.
  # Compile every C++ target against the active SDK's matching headers and
  # runtime while retaining the pinned LLVM Clang and LLD executables.
  set(ROCKET_MACOS_SDK_ROOT "$ENV{ROCKET_MACOS_SDK_ROOT}")
  if(NOT ROCKET_MACOS_SDK_ROOT)
    execute_process(
      COMMAND xcrun --sdk macosx --show-sdk-path
      OUTPUT_VARIABLE ROCKET_MACOS_SDK_ROOT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE ROCKET_XCRUN_RESULT)
    if(NOT ROCKET_XCRUN_RESULT EQUAL 0 OR NOT ROCKET_MACOS_SDK_ROOT)
      message(FATAL_ERROR "Unable to locate the active macOS SDK with xcrun")
    endif()
  endif()
  if(NOT IS_DIRECTORY "${ROCKET_MACOS_SDK_ROOT}")
    message(FATAL_ERROR
      "macOS SDK root is missing: ${ROCKET_MACOS_SDK_ROOT}")
  endif()
  set(ROCKET_CXX_STANDARD_INCLUDE "$ENV{ROCKET_CXX_STANDARD_INCLUDE}")
  set(ROCKET_CXX_STANDARD_LIBRARY "$ENV{ROCKET_CXX_STANDARD_LIBRARY}")
  if(NOT ROCKET_CXX_STANDARD_INCLUDE)
    set(ROCKET_CXX_STANDARD_INCLUDE
      "${ROCKET_MACOS_SDK_ROOT}/usr/include/c++/v1")
  endif()
  if(NOT ROCKET_CXX_STANDARD_LIBRARY)
    set(ROCKET_CXX_STANDARD_LIBRARY
      "${ROCKET_MACOS_SDK_ROOT}/usr/lib/libc++.tbd")
  endif()
  if(NOT IS_DIRECTORY "${ROCKET_CXX_STANDARD_INCLUDE}")
    message(FATAL_ERROR
      "macOS libc++ headers are missing: ${ROCKET_CXX_STANDARD_INCLUDE}")
  endif()
  if(NOT EXISTS "${ROCKET_CXX_STANDARD_LIBRARY}")
    message(FATAL_ERROR
      "macOS libc++ runtime stub is missing: ${ROCKET_CXX_STANDARD_LIBRARY}")
  endif()
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>")
  include_directories(SYSTEM BEFORE "${ROCKET_CXX_STANDARD_INCLUDE}")
  # Phase 19 rewrites packaged Mach-O dependency paths with install_name_tool.
  # Reserve deterministic load-command space in every CMake-linked product.
  add_link_options("-Wl,-headerpad_max_install_names")
  message(STATUS
    "Rocket macOS libc++ headers: ${ROCKET_CXX_STANDARD_INCLUDE}")
  message(STATUS
    "Rocket macOS libc++ runtime: ${ROCKET_CXX_STANDARD_LIBRARY}")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ROCKET_SYSTEM_PROCESSOR)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND
   ROCKET_SYSTEM_PROCESSOR MATCHES "^(amd64|x86_64)$")
  set(ROCKET_NATIVE_TARGET "windows-x64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       ROCKET_SYSTEM_PROCESSOR MATCHES "^(amd64|x86_64)$")
  set(ROCKET_NATIVE_TARGET "linux-x64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       ROCKET_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(ROCKET_NATIVE_TARGET "linux-arm64")
elseif(APPLE AND ROCKET_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(ROCKET_NATIVE_TARGET "macos-arm64")
else()
  message(FATAL_ERROR
    "This host is not one of the four Rocket 2.1 production targets")
endif()
set(ROCKET_EXPECTED_NATIVE_TARGET "" CACHE STRING
  "Fail configuration unless the detected native target matches this alias")
if(ROCKET_EXPECTED_NATIVE_TARGET AND
   NOT ROCKET_EXPECTED_NATIVE_TARGET STREQUAL ROCKET_NATIVE_TARGET)
  message(FATAL_ERROR
    "Rocket preset target '${ROCKET_EXPECTED_NATIVE_TARGET}' does not match "
    "the detected native host '${ROCKET_NATIVE_TARGET}'")
endif()
message(STATUS "Rocket native target: ${ROCKET_NATIVE_TARGET}")
set(ROCKET_TEST_ARTIFACT_ROOT "${CMAKE_BINARY_DIR}/rocket-artifacts")
set(ROCKET_SELFHOST_COMPILER
  "${ROCKET_TEST_ARTIFACT_ROOT}/rocket_compiler/.rocketc/targets/${ROCKET_NATIVE_TARGET}/main${CMAKE_EXECUTABLE_SUFFIX}")
set(ROCKET_TARGET_ARTIFACT_SUFFIX
  ".rocketc/targets/${ROCKET_NATIVE_TARGET}")
if(WIN32)
  set(ROCKET_STATIC_LIBRARY_SUFFIX ".lib")
  set(ROCKET_DYNAMIC_LIBRARY_SUFFIX ".dll")
  set(ROCKET_DYNAMIC_LINK_LIBRARY_SUFFIX ".lib")
elseif(APPLE)
  set(ROCKET_STATIC_LIBRARY_SUFFIX ".a")
  set(ROCKET_DYNAMIC_LIBRARY_SUFFIX ".dylib")
  set(ROCKET_DYNAMIC_LINK_LIBRARY_SUFFIX ".dylib")
else()
  set(ROCKET_STATIC_LIBRARY_SUFFIX ".a")
  set(ROCKET_DYNAMIC_LIBRARY_SUFFIX ".so")
  set(ROCKET_DYNAMIC_LINK_LIBRARY_SUFFIX ".so")
endif()
option(ROCKETC_ENABLE_SANITIZERS
  "Enable AddressSanitizer (and UndefinedBehaviorSanitizer where available)" OFF)
if(MSVC)
  if(ROCKETC_ENABLE_SANITIZERS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  add_compile_options(/EHsc)
endif()

if(ROCKETC_ENABLE_SANITIZERS)
  if(MSVC)
    foreach(flag_variable CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
      string(REPLACE "/RTC1" "" ${flag_variable} "${${flag_variable}}")
    endforeach()
    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/INCREMENTAL:NO)
  else()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

option(ROCKETC_ENABLE_LLVM "Enable the production LLVM backend when LLVM is installed" ON)
if(ROCKETC_ENABLE_LLVM)
  find_package(LLVM CONFIG QUIET)
endif()

add_library(rocket_runtime STATIC src/runtime.cpp src/stdlib.cpp)
target_include_directories(rocket_runtime PUBLIC src)
if(NOT WIN32)
  find_package(Threads REQUIRED)
  find_package(CURL REQUIRED)
  find_package(OpenSSL REQUIRED COMPONENTS Crypto)
  find_package(ICU REQUIRED COMPONENTS uc)
  set(ROCKET_POSIX_PLATFORM_LIBRARIES Threads::Threads CURL::libcurl
    OpenSSL::Crypto ICU::uc ${CMAKE_DL_LIBS})
  if(APPLE)
    find_library(ROCKET_SECURITY_FRAMEWORK Security REQUIRED)
    find_library(ROCKET_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    # Keep the SDK libc++ runtime explicit and transitive.  The matching SDK
    # headers were selected before any C++ target was created above.
    list(APPEND ROCKET_POSIX_PLATFORM_LIBRARIES
      "${ROCKET_SECURITY_FRAMEWORK}" "${ROCKET_COREFOUNDATION_FRAMEWORK}"
      "${ROCKET_CXX_STANDARD_LIBRARY}")
  endif()
  target_link_libraries(rocket_runtime PUBLIC ${ROCKET_POSIX_PLATFORM_LIBRARIES})
  target_compile_definitions(rocket_runtime PUBLIC
    ROCKET_HAS_CURL=1 ROCKET_HAS_ICU=1)
endif()
if(MSVC)
  target_compile_options(rocket_runtime PRIVATE /W4 /permissive-)
else()
  target_compile_options(rocket_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(ROCKET_RAYLIB_SOURCE "${CMAKE_SOURCE_DIR}/dependencies/installed/raylib-6.0")
if(NOT EXISTS "${ROCKET_RAYLIB_SOURCE}/src/raylib.h")
  message(FATAL_ERROR
    "Pinned raylib 6.0 is missing. Run python dependencies/bootstrap.py before configuring.")
endif()
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib as a shared library" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "Use Rocket's reviewed raylib feature set" FORCE)
foreach(ROCKET_RAYLIB_DISABLED_FEATURE IN ITEMS
    SUPPORT_MODULE_RMODELS SUPPORT_SCREEN_CAPTURE SUPPORT_COMPRESSION_API
    SUPPORT_AUTOMATION_EVENTS SUPPORT_CLIPBOARD_IMAGE SUPPORT_FILEFORMAT_TGA
    SUPPORT_FILEFORMAT_GIF SUPPORT_FILEFORMAT_QOI SUPPORT_FILEFORMAT_PSD
    SUPPORT_FILEFORMAT_DDS SUPPORT_FILEFORMAT_HDR SUPPORT_FILEFORMAT_PIC
    SUPPORT_FILEFORMAT_KTX SUPPORT_FILEFORMAT_ASTC SUPPORT_FILEFORMAT_PKM
    SUPPORT_FILEFORMAT_PVR SUPPORT_IMAGE_EXPORT SUPPORT_IMAGE_GENERATION
    SUPPORT_FILEFORMAT_BDF SUPPORT_FILEFORMAT_OBJ SUPPORT_FILEFORMAT_MTL
    SUPPORT_FILEFORMAT_IQM SUPPORT_FILEFORMAT_GLTF SUPPORT_FILEFORMAT_VOX
    SUPPORT_FILEFORMAT_M3D SUPPORT_MESH_GENERATION SUPPORT_GPU_SKINNING
    SUPPORT_FILEFORMAT_FLAC SUPPORT_FILEFORMAT_XM SUPPORT_FILEFORMAT_MOD)
  set(${ROCKET_RAYLIB_DISABLED_FEATURE} OFF CACHE BOOL "Rocket raylib profile" FORCE)
endforeach()
foreach(ROCKET_RAYLIB_ENABLED_FEATURE IN ITEMS
    SUPPORT_MODULE_RSHAPES SUPPORT_MODULE_RTEXTURES SUPPORT_MODULE_RTEXT
    SUPPORT_MODULE_RAUDIO SUPPORT_FILEFORMAT_PNG SUPPORT_FILEFORMAT_BMP
    SUPPORT_FILEFORMAT_JPG SUPPORT_FILEFORMAT_PNM SUPPORT_FILEFORMAT_TTF
    SUPPORT_FILEFORMAT_FNT SUPPORT_FILEFORMAT_WAV SUPPORT_FILEFORMAT_OGG
    SUPPORT_FILEFORMAT_MP3 SUPPORT_FILEFORMAT_QOA)
  set(${ROCKET_RAYLIB_ENABLED_FEATURE} ON CACHE BOOL "Rocket raylib profile" FORCE)
endforeach()
set(PLATFORM Desktop CACHE STRING "raylib target platform" FORCE)
add_subdirectory("${ROCKET_RAYLIB_SOURCE}" "${CMAKE_BINARY_DIR}/raylib" EXCLUDE_FROM_ALL)

set(ROCKET_RAYLIB_PACKAGE "${CMAKE_SOURCE_DIR}/examples/raylib_showcase")
set(ROCKET_NATIVE_OUTPUT "${CMAKE_BINARY_DIR}/native/${ROCKET_NATIVE_TARGET}")
set(ROCKET_RAYLIB_NATIVE_OUTPUT "${ROCKET_NATIVE_OUTPUT}")
set_target_properties(raylib PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${ROCKET_RAYLIB_NATIVE_OUTPUT}"
  MSVC_RUNTIME_LIBRARY "MultiThreaded")
if(TARGET glfw)
  set_target_properties(glfw PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

add_library(rocket_raylib_adapter STATIC
  examples/raylib_showcase/native/rocket_raylib_adapter.cpp)
target_include_directories(rocket_raylib_adapter PUBLIC
  examples/raylib_showcase/native)
target_link_libraries(rocket_raylib_adapter PUBLIC raylib)
set_target_properties(rocket_raylib_adapter PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${ROCKET_RAYLIB_NATIVE_OUTPUT}"
  MSVC_RUNTIME_LIBRARY "MultiThreaded")
if(MSVC)
  target_compile_options(rocket_raylib_adapter PRIVATE /W4 /permissive-)
else()
  target_compile_options(rocket_raylib_adapter PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(rocket_phase13_native STATIC tests/native/phase13_native.cpp)
target_include_directories(rocket_phase13_native PUBLIC tests/native)
set(ROCKET_PHASE13_NATIVE_OUTPUT "${ROCKET_NATIVE_OUTPUT}")
set_target_properties(rocket_phase13_native PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${ROCKET_PHASE13_NATIVE_OUTPUT}")

add_library(rocketc_core
  src/target.cpp
  src/toolchain.cpp
  src/lexer.cpp
  src/parser.cpp
  src/module_loader.cpp
  src/language_server.cpp
  src/formatter.cpp
  src/package.cpp
  src/package_docs.cpp
  src/package_registry.cpp
  src/package_git.cpp
  src/native.cpp
  src/type.cpp
  src/hir.cpp
  src/sema.cpp
  src/mir.cpp
  src/codegen.cpp
)
target_include_directories(rocketc_core PUBLIC src)
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/stdlib" ROCKETC_STDLIB_SOURCE_PATH)
target_compile_definitions(rocketc_core PRIVATE
  ROCKETC_STDLIB_SOURCE_PATH="${ROCKETC_STDLIB_SOURCE_PATH}")
if(WIN32)
  target_link_libraries(rocketc_core PRIVATE bcrypt advapi32)
else()
  target_link_libraries(rocketc_core PRIVATE ${ROCKET_POSIX_PLATFORM_LIBRARIES})
  target_compile_definitions(rocketc_core PUBLIC
    ROCKET_HAS_CURL=1 ROCKET_HAS_ICU=1)
endif()

if(MSVC)
  target_compile_options(rocketc_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(rocketc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LLVM_FOUND)
  message(STATUS "LLVM ${LLVM_PACKAGE_VERSION} found; enabling the scalar production backend")

  # The official LLVM Windows archive can encode the build machine's absolute
  # DIA SDK path in LLVMDebugInfoPDB. Resolve the equivalent library from the
  # active MSVC installation when that packaged path does not exist.
  if(MSVC AND TARGET LLVMDebugInfoPDB)
    get_target_property(ROCKETC_PDB_LINK_LIBS LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
    set(ROCKETC_FIXED_PDB_LINK_LIBS)
    foreach(ROCKETC_PDB_LINK_LIB IN LISTS ROCKETC_PDB_LINK_LIBS)
      if(ROCKETC_PDB_LINK_LIB MATCHES "diaguids\\.lib$" AND
         NOT EXISTS "${ROCKETC_PDB_LINK_LIB}")
        find_library(ROCKETC_DIAGUIDS_LIBRARY NAMES diaguids
          PATHS "$ENV{VSINSTALLDIR}/DIA SDK/lib/amd64" NO_DEFAULT_PATH REQUIRED)
        list(APPEND ROCKETC_FIXED_PDB_LINK_LIBS "${ROCKETC_DIAGUIDS_LIBRARY}")
      else()
        list(APPEND ROCKETC_FIXED_PDB_LINK_LIBS "${ROCKETC_PDB_LINK_LIB}")
      endif()
    endforeach()
    set_property(TARGET LLVMDebugInfoPDB PROPERTY INTERFACE_LINK_LIBRARIES
      "${ROCKETC_FIXED_PDB_LINK_LIBS}")
  endif()

  target_sources(rocketc_core PRIVATE src/llvm_codegen.cpp)
  if(MSVC)
    set_source_files_properties(src/llvm_codegen.cpp PROPERTIES COMPILE_OPTIONS /wd4702)
  endif()
  target_include_directories(rocketc_core SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  separate_arguments(ROCKETC_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
  target_compile_options(rocketc_core PRIVATE ${ROCKETC_LLVM_DEFINITIONS})
  llvm_map_components_to_libnames(ROCKETC_LLVM_LIBS
    core support analysis transformutils passes target mc codegen
    x86codegen x86asmparser aarch64codegen aarch64asmparser
  )
  target_link_libraries(rocketc_core PRIVATE ${ROCKETC_LLVM_LIBS})
  target_compile_definitions(rocketc_core PUBLIC ROCKETC_HAS_LLVM=1)
else()
  message(WARNING "LLVM not found; only the stage0 bootstrap C++ fallback is available")
endif()

add_executable(rocketc src/main.cpp)
target_link_libraries(rocketc PRIVATE rocketc_core)
if(APPLE)
  target_compile_definitions(rocketc PRIVATE
    ROCKETC_MACOS_SDK_ROOT="${ROCKET_MACOS_SDK_ROOT}"
    ROCKETC_CXX_STANDARD_INCLUDE="${ROCKET_CXX_STANDARD_INCLUDE}"
    ROCKETC_CXX_STANDARD_LIBRARY="${ROCKET_CXX_STANDARD_LIBRARY}")
endif()

add_executable(rocket_lsp src/language_server_main.cpp)
set_target_properties(rocket_lsp PROPERTIES OUTPUT_NAME "rocket-lsp")
target_link_libraries(rocket_lsp PRIVATE rocketc_core)
target_compile_definitions(rocket_lsp PRIVATE ROCKET_LSP_VERSION="1.0.0")
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/src" ROCKETC_SOURCE_INCLUDE_PATH)
file(TO_CMAKE_PATH "${CMAKE_CXX_COMPILER}" ROCKETC_STAGE0_CXX_PATH)
file(TO_CMAKE_PATH "${CMAKE_AR}" ROCKETC_STAGE0_AR_PATH)
target_compile_definitions(rocketc PRIVATE
  ROCKETC_SOURCE_INCLUDE_PATH="${ROCKETC_SOURCE_INCLUDE_PATH}"
  ROCKETC_STAGE0_CXX_PATH="${ROCKETC_STAGE0_CXX_PATH}"
  ROCKETC_STAGE0_AR_PATH="${ROCKETC_STAGE0_AR_PATH}")
target_compile_definitions(rocketc PRIVATE ROCKETC_VERSION="${PROJECT_VERSION}")
if(LLVM_FOUND)
  file(TO_CMAKE_PATH
    "${LLVM_TOOLS_BINARY_DIR}/clang${CMAKE_EXECUTABLE_SUFFIX}"
    ROCKETC_CLANG_PATH)
  file(TO_CMAKE_PATH
    "${LLVM_TOOLS_BINARY_DIR}/llvm-ar${CMAKE_EXECUTABLE_SUFFIX}"
    ROCKETC_LLVM_AR_PATH)
  file(TO_CMAKE_PATH
    "${LLVM_TOOLS_BINARY_DIR}/llvm-lib${CMAKE_EXECUTABLE_SUFFIX}"
    ROCKETC_LLVM_LIB_PATH)
  if(WIN32)
    set(ROCKETC_TEST_LIBRARIAN_PATH "${ROCKETC_LLVM_LIB_PATH}")
  else()
    set(ROCKETC_TEST_LIBRARIAN_PATH "${ROCKETC_LLVM_AR_PATH}")
  endif()
  target_compile_definitions(rocketc PRIVATE
    ROCKETC_CLANG_PATH="${ROCKETC_CLANG_PATH}"
    ROCKETC_LLVM_AR_PATH="${ROCKETC_LLVM_AR_PATH}"
    ROCKETC_LLVM_LIB_PATH="${ROCKETC_LLVM_LIB_PATH}"
    ROCKETC_RUNTIME_LIBRARY_PATH="$<TARGET_FILE:rocket_runtime>")
  add_dependencies(rocketc rocket_runtime)
endif()

set(ROCKET_RAYLIB_HEADER
  "${ROCKET_RAYLIB_PACKAGE}/native/rocket_raylib_adapter.h")
set(ROCKET_RAYLIB_BINDINGS
  "${CMAKE_BINARY_DIR}/generated/raylib/rocket_raylib_adapter.rocket")
add_custom_command(
  OUTPUT "${ROCKET_RAYLIB_BINDINGS}"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_BINARY_DIR}/generated/raylib"
  COMMAND $<TARGET_FILE:rocketc> bind "${ROCKET_RAYLIB_HEADER}"
          --output "${ROCKET_RAYLIB_BINDINGS}"
  DEPENDS rocketc "${ROCKET_RAYLIB_HEADER}"
  VERBATIM
  COMMENT "Generating deterministic Rocket raylib adapter bindings")
add_custom_target(rocket_raylib_bindings ALL DEPENDS "${ROCKET_RAYLIB_BINDINGS}")
add_dependencies(rocket_raylib_bindings rocket_raylib_adapter)

add_custom_target(rocket_raylib_check
  COMMAND $<TARGET_FILE:rocketc> check "${ROCKET_RAYLIB_PACKAGE}"
  DEPENDS rocket_raylib_bindings
  WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}"
  USES_TERMINAL
  COMMENT "Checking the Rocket raylib showcase")
add_custom_target(rocket_raylib_build
  COMMAND ${CMAKE_COMMAND} -E env
          "ROCKET_NATIVE_LIBRARY_ROOT=${ROCKET_NATIVE_OUTPUT}"
          "ROCKET_ARTIFACT_ROOT=${ROCKET_TEST_ARTIFACT_ROOT}"
          $<TARGET_FILE:rocketc> build "${ROCKET_RAYLIB_PACKAGE}"
  DEPENDS rocket_raylib_bindings
  WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}"
  USES_TERMINAL
  COMMENT "Building the Rocket raylib showcase")
add_custom_target(rocket_raylib_test
  COMMAND ${CMAKE_COMMAND} -E env
          "ROCKET_NATIVE_LIBRARY_ROOT=${ROCKET_NATIVE_OUTPUT}"
          "ROCKET_ARTIFACT_ROOT=${ROCKET_TEST_ARTIFACT_ROOT}"
          $<TARGET_FILE:rocketc> test "${ROCKET_RAYLIB_PACKAGE}"
  DEPENDS rocket_raylib_bindings
  WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}"
  USES_TERMINAL
  COMMENT "Testing the Rocket raylib showcase with the deterministic backend")

# Visual Studio's CMake Targets View exposes these as explicit Rocket actions.
# They operate on a real package instead of treating the compiler repository
# root as if it were a Rocket application package.
set(ROCKET_VISUAL_STUDIO_DEMO "${CMAKE_SOURCE_DIR}/examples/visualstudio_demo")
add_custom_target(rocket_demo_check
  COMMAND $<TARGET_FILE:rocketc> check "${ROCKET_VISUAL_STUDIO_DEMO}"
  DEPENDS rocketc
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Checking the Rocket Visual Studio demo")
add_custom_target(rocket_demo_run
  COMMAND $<TARGET_FILE:rocketc> run "${ROCKET_VISUAL_STUDIO_DEMO}"
  DEPENDS rocketc
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Running the Rocket Visual Studio demo")
add_custom_target(rocket_demo_test
  COMMAND $<TARGET_FILE:rocketc> test "${ROCKET_VISUAL_STUDIO_DEMO}"
  DEPENDS rocketc
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Testing the Rocket Visual Studio demo")

if(WIN32)
  add_custom_target(rocket_visualstudio_extension
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/package-visualstudio-extension.ps1"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Packaging the Rocket Visual Studio 2026 extension")
endif()

enable_testing()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
set(ROCKET_PORTABLE_WORKFLOW_TEST
  "${CMAKE_SOURCE_DIR}/tests/portable_workflow_test.py")
add_test(NAME phase19_release_tooling
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/phase19_release_tooling_test.py"
    --source-dir "${CMAKE_SOURCE_DIR}"
    --work "${CMAKE_SOURCE_DIR}/out/phase19/release-tooling-unit")
set_tests_properties(phase19_release_tooling PROPERTIES
  LABELS "phase19;release;packaging;relocation;determinism;portable")
function(add_rocketc_test name)
  add_executable(rocketc_${name}_tests tests/${name}_tests.cpp)
  target_link_libraries(rocketc_${name}_tests PRIVATE rocketc_core)
  add_test(NAME ${name} COMMAND rocketc_${name}_tests)
endfunction()

add_rocketc_test(lexer)
add_rocketc_test(parser)
add_rocketc_test(sema)
add_rocketc_test(hir)
add_rocketc_test(mir)
add_rocketc_test(codegen)
add_rocketc_test(target)
add_rocketc_test(toolchain)
if(LLVM_FOUND)
  add_rocketc_test(llvm_codegen)
endif()
add_rocketc_test(diagnostics)
add_rocketc_test(language_server)
add_rocketc_test(formatter)
add_rocketc_test(package)
add_rocketc_test(native)
add_rocketc_test(hardening)
set_tests_properties(hardening PROPERTIES LABELS "phase20;hardening;fuzz")

add_executable(rocket_phase14_raylib_adapter_tests
  tests/native/phase14_raylib_adapter_tests.cpp)
target_link_libraries(rocket_phase14_raylib_adapter_tests PRIVATE
  rocket_raylib_adapter)
set_target_properties(rocket_phase14_raylib_adapter_tests PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded")
add_test(NAME phase14_raylib_adapter COMMAND rocket_phase14_raylib_adapter_tests)
set_tests_properties(phase14_raylib_adapter PROPERTIES LABELS "phase14;raylib;native")

add_executable(rocketc_runtime_tests tests/runtime_tests.cpp)
target_link_libraries(rocketc_runtime_tests PRIVATE rocket_runtime)
add_test(NAME runtime COMMAND rocketc_runtime_tests)
set_tests_properties(parser hir mir codegen runtime PROPERTIES LABELS "phase18")
if(LLVM_FOUND)
  set_tests_properties(llvm_codegen PROPERTIES LABELS "phase18;llvm")
endif()

add_executable(rocketc_stdlib_tests tests/stdlib_tests.cpp)
target_link_libraries(rocketc_stdlib_tests PRIVATE rocket_runtime)
add_test(NAME stdlib COMMAND rocketc_stdlib_tests phase9-argument)

add_executable(rocketc_compression_tests tests/compression_tests.cpp)
target_include_directories(rocketc_compression_tests PRIVATE src)
add_test(NAME compression COMMAND rocketc_compression_tests)
set_tests_properties(compression PROPERTIES
  LABELS "phase15;phase19;stdlib;compression;portable")

add_test(NAME cli_check_hello
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/examples/hello.rocket")

add_test(NAME cli_check_phase6_types
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase6_types.rocket")
add_test(NAME cli_check_phase6_modules
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase6_modules.rocket")
add_test(NAME cli_check_phase7_stdlib
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase7_stdlib.rocket")
add_test(NAME cli_check_phase15_binary_io
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_binary_io.rocket")
set_tests_properties(cli_check_phase15_binary_io PROPERTIES LABELS "phase15;stdlib")
add_test(NAME cli_check_phase15_text_streams
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_text_streams.rocket")
set_tests_properties(cli_check_phase15_text_streams PROPERTIES LABELS "phase15;stdlib")
add_test(NAME cli_check_phase15_crypto
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_crypto.rocket")
set_tests_properties(cli_check_phase15_crypto PROPERTIES LABELS "phase15;stdlib;crypto")
add_test(NAME cli_check_phase15_network
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_network.rocket")
set_tests_properties(cli_check_phase15_network PROPERTIES LABELS "phase15;stdlib;network")
add_test(NAME cli_check_phase15_platform
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_platform.rocket")
set_tests_properties(cli_check_phase15_platform PROPERTIES LABELS "phase15;stdlib;platform")
add_test(NAME cli_check_phase15_archive
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_archive.rocket")
set_tests_properties(cli_check_phase15_archive PROPERTIES LABELS "phase15;stdlib;archive")
add_test(NAME cli_check_phase15_sqlite
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_sqlite.rocket")
set_tests_properties(cli_check_phase15_sqlite PROPERTIES LABELS "phase15;stdlib;database")
add_test(NAME cli_check_phase15_testing
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_testing.rocket")
set_tests_properties(cli_check_phase15_testing PROPERTIES LABELS "phase15;stdlib;testing")
add_test(NAME cli_test_phase15_expected_failure
  COMMAND rocketc test "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_test_package")
set_tests_properties(cli_test_phase15_expected_failure PROPERTIES
  PASS_REGULAR_EXPRESSION "1 passed; 0 failed; 1 expected failure"
  LABELS "phase15;testing")
add_test(NAME cli_test_phase15_filter
  COMMAND rocketc test "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_test_package"
    --filter pass)
set_tests_properties(cli_test_phase15_filter PROPERTIES
  PASS_REGULAR_EXPRESSION "1 passed; 0 failed; 0 expected failure"
  LABELS "phase15;testing")
add_test(NAME cli_check_phase8_package
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
add_test(NAME cli_check_phase9_bootstrap_primitives
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase9_bootstrap_primitives.rocket")
add_test(NAME cli_check_phase11_array_mutation
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_mutation.rocket")
add_test(NAME cli_check_phase11_array_growth
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_growth.rocket")
add_test(NAME cli_check_phase11_map_set_tuple
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_map_set_tuple.rocket")
add_test(NAME cli_check_phase12_methods
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_methods.rocket")
add_test(NAME cli_check_phase12_modules
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_modules.rocket")
foreach(feature traits closures iterators associated_constants)
  add_test(NAME cli_check_phase12_${feature}
    COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_${feature}.rocket")
endforeach()
add_test(NAME cli_check_phase12_generic_lambdas
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_generic_lambdas.rocket")
add_test(NAME cli_check_phase13_native
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
add_test(NAME cli_check_phase14_raylib_reference
  COMMAND rocketc check "${ROCKET_RAYLIB_PACKAGE}")
set_tests_properties(cli_check_phase14_raylib_reference PROPERTIES
  LABELS "phase14;raylib"
  WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
add_test(NAME cli_build_phase14_raylib_reference
  COMMAND rocketc build "${ROCKET_RAYLIB_PACKAGE}")
set_tests_properties(cli_build_phase14_raylib_reference PROPERTIES
  PASS_REGULAR_EXPRESSION "rocket-raylib-showcase${CMAKE_EXECUTABLE_SUFFIX}"
  LABELS "phase14;raylib;native"
  WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
add_test(NAME cli_test_phase14_raylib_reference
  COMMAND rocketc test "${ROCKET_RAYLIB_PACKAGE}")
set_tests_properties(cli_test_phase14_raylib_reference PROPERTIES
  PASS_REGULAR_EXPRESSION "4 passed; 0 failed"
  LABELS "phase14;raylib;native"
  WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
add_test(NAME cli_phase14_unsafe_failure
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase14_unsafe_failure.rocket"
    "-DEXPECTED=requires an explicit unsafe block"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
set_tests_properties(cli_phase14_unsafe_failure PROPERTIES LABELS "phase14;raylib")
add_test(NAME cli_phase13_unsafe_failure
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_unsafe_failure.rocket"
    "-DEXPECTED=requires an explicit unsafe block"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_phase12_constraint_failure
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_constraint_failure.rocket"
    "-DEXPECTED=does not implement trait 'Readable'"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_phase12_iterator_failure
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_iterator_failure.rocket"
    "-DEXPECTED=iterator advance() must return Broken"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_phase12_generic_lambda_failure
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_generic_lambda_failure.rocket"
    "-DEXPECTED=String, expected Int"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_phase12_immediate_lambda_failure
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_immediate_lambda_failure.rocket"
    "-DEXPECTED=String, expected Int"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_phase11_invalid_key
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_invalid_key.rocket"
    "-DEXPECTED=Map keys must be Int, Bool, Char, or String"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_run_phase9_bootstrap_primitives
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase9_bootstrap_primitives.rocket"
    -- first second)
set_tests_properties(cli_run_phase9_bootstrap_primitives PROPERTIES
  PASS_REGULAR_EXPRESSION "Rocket[\r\n]+4[\r\n]+2")
foreach(fixture async_join nested_await weak unique_buffer concurrency async_file async_socket async_cancel process task_group task_cancel thread structured_cleanup unsafe_local)
  add_test(NAME cli_check_phase18_${fixture}
    COMMAND rocketc check "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_${fixture}.rocket")
  set_tests_properties(cli_check_phase18_${fixture} PROPERTIES
    LABELS "phase18;ownership;concurrency")
endforeach()
add_test(NAME cli_run_phase18_async_join
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_async_join.rocket")
set_tests_properties(cli_run_phase18_async_join PROPERTIES
  PASS_REGULAR_EXPRESSION "42" LABELS "phase18;async;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_concurrency
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_concurrency.rocket")
set_tests_properties(cli_run_phase18_concurrency PROPERTIES
  PASS_REGULAR_EXPRESSION "published" LABELS "phase18;concurrency;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
if(LLVM_FOUND)
  add_test(NAME cli_emit_ir_phase18_scalar_concurrency
    COMMAND rocketc emit-ir "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_concurrency.rocket")
  set_tests_properties(cli_emit_ir_phase18_scalar_concurrency PROPERTIES
    PASS_REGULAR_EXPRESSION "rocket_std_sync_once_empty_int"
    LABELS "phase18;concurrency;llvm")
endif()
add_test(NAME cli_run_phase18_async_file
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_async_file.rocket")
set_tests_properties(cli_run_phase18_async_file PROPERTIES
  PASS_REGULAR_EXPRESSION "R[\r\n]+T" LABELS "phase18;async;file;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_async_socket
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_async_socket.rocket")
set_tests_properties(cli_run_phase18_async_socket PROPERTIES
  PASS_REGULAR_EXPRESSION "socket-ok" LABELS "phase18;async;socket;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_process
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_process.rocket")
set_tests_properties(cli_run_phase18_process PROPERTIES
  PASS_REGULAR_EXPRESSION "7[\r\n]+1[\r\n]+operation cancelled"
  LABELS "phase18;async;process;cancel;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_task_group
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_task_group.rocket")
set_tests_properties(cli_run_phase18_task_group PROPERTIES
  PASS_REGULAR_EXPRESSION "first[\r\n]+second[\r\n]+third"
  LABELS "phase18;structured;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_task_cancel
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_task_cancel.rocket")
set_tests_properties(cli_run_phase18_task_cancel PROPERTIES
  PASS_REGULAR_EXPRESSION "operation cancelled" LABELS "phase18;structured;cancel;native"
  TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_async_cancel
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_async_cancel.rocket")
set_tests_properties(cli_run_phase18_async_cancel PROPERTIES
  PASS_REGULAR_EXPRESSION "operation cancelled" LABELS "phase18;cancel;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_thread
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_thread.rocket")
set_tests_properties(cli_run_phase18_thread PROPERTIES
  PASS_REGULAR_EXPRESSION "thread-result" LABELS "phase18;thread;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_run_phase18_structured_cleanup
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_structured_cleanup.rocket")
set_tests_properties(cli_run_phase18_structured_cleanup PROPERTIES
  PASS_REGULAR_EXPRESSION "scope-failed" LABELS "phase18;structured;cleanup;native"
  TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_test(NAME cli_run_phase18_example
  COMMAND rocketc run "${CMAKE_SOURCE_DIR}/examples/ownership_concurrency.rocket")
set_tests_properties(cli_run_phase18_example PROPERTIES
  PASS_REGULAR_EXPRESSION "41[\r\n]+3[\r\n]+42"
  LABELS "phase18;example;native" TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
foreach(failure await_context send async_result_send suspension unique_buffer_move scoped_escape weak_share task_weak pointer_send guard_release task_reuse mutex_share capture_move transitive_move buffer_element_share)
  add_test(NAME cli_phase18_${failure}_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_${failure}_failure.rocket"
      "-DEXPECTED=R410"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
  set_tests_properties(cli_phase18_${failure}_failure PROPERTIES
    LABELS "phase18;negative")
endforeach()
add_test(NAME cli_check_self_hosted_compiler
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/compiler")
add_test(NAME cli_build_self_hosted_compiler
  COMMAND rocketc build "${CMAKE_SOURCE_DIR}/compiler")
set_tests_properties(cli_build_self_hosted_compiler PROPERTIES
  FIXTURES_SETUP self_hosted_compiler)
add_test(NAME cli_run_self_hosted_lexer_tests
  COMMAND "${ROCKET_SELFHOST_COMPILER}" --self-test-lexer)
set_tests_properties(cli_run_self_hosted_lexer_tests PROPERTIES
  PASS_REGULAR_EXPRESSION "self-hosted lexer tests passed"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_parser_tests
  COMMAND "${ROCKET_SELFHOST_COMPILER}" --self-test-parser)
set_tests_properties(cli_run_self_hosted_parser_tests PROPERTIES
  PASS_REGULAR_EXPRESSION "self-hosted parser tests passed"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_syntax_check
  COMMAND "${ROCKET_SELFHOST_COMPILER}" --check-syntax
    "${CMAKE_SOURCE_DIR}/examples/hello.rocket")
set_tests_properties(cli_run_self_hosted_syntax_check PROPERTIES
  PASS_REGULAR_EXPRESSION "hello.rocket: syntax check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_hir_check
  COMMAND "${ROCKET_SELFHOST_COMPILER}" --check-hir
    "${CMAKE_SOURCE_DIR}/compiler/src/main.rocket")
set_tests_properties(cli_run_self_hosted_hir_check PROPERTIES
  PASS_REGULAR_EXPRESSION "main.rocket: HIR check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_mir_check
  COMMAND "${ROCKET_SELFHOST_COMPILER}" --check-mir
    "${CMAKE_SOURCE_DIR}/compiler/src/main.rocket")
set_tests_properties(cli_run_self_hosted_mir_check PROPERTIES
  PASS_REGULAR_EXPRESSION "main.rocket: MIR check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_self_hosted_check_package
  COMMAND "${ROCKET_SELFHOST_COMPILER}" check
    "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
set_tests_properties(cli_self_hosted_check_package PROPERTIES
  PASS_REGULAR_EXPRESSION "check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
foreach(fixture async_join nested_await weak unique_buffer concurrency async_file async_socket async_cancel process task_group task_cancel thread structured_cleanup unsafe_local)
  add_test(NAME cli_self_hosted_phase18_${fixture}
    COMMAND "${ROCKET_SELFHOST_COMPILER}" check
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_${fixture}.rocket")
  set_tests_properties(cli_self_hosted_phase18_${fixture} PROPERTIES
    PASS_REGULAR_EXPRESSION "check succeeded"
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase18;selfhost")
endforeach()
foreach(failure await_context send async_result_send suspension unique_buffer_move scoped_escape weak_share task_weak pointer_send guard_release task_reuse mutex_share capture_move transitive_move buffer_element_share)
  add_test(NAME cli_self_hosted_phase18_${failure}_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=${ROCKET_SELFHOST_COMPILER}"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_${failure}_failure.rocket"
      "-DEXPECTED=R410"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
  set_tests_properties(cli_self_hosted_phase18_${failure}_failure PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase18;selfhost;negative")
endforeach()
add_test(NAME cli_self_hosted_fmt_package
  COMMAND "${ROCKET_SELFHOST_COMPILER}" fmt
    "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package" --check)
set_tests_properties(cli_self_hosted_fmt_package PROPERTIES
  PASS_REGULAR_EXPRESSION "format check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_self_hosted_formatter_canonical
  COMMAND ${CMAKE_COMMAND}
    "-DCOMPILER=${ROCKET_SELFHOST_COMPILER}"
    "-DWORK=${CMAKE_BINARY_DIR}/self-hosted-formatter"
    -P "${CMAKE_SOURCE_DIR}/tests/self_hosted_formatter_test.cmake")
set_tests_properties(cli_self_hosted_formatter_canonical PROPERTIES
  PASS_REGULAR_EXPRESSION "format check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_self_hosted_new_package
  COMMAND ${CMAKE_COMMAND}
    "-DCOMPILER=${ROCKET_SELFHOST_COMPILER}"
    "-DWORK=${CMAKE_BINARY_DIR}/self-hosted-new-package"
    -P "${CMAKE_SOURCE_DIR}/tests/self_hosted_new_package_test.cmake")
set_tests_properties(cli_self_hosted_new_package PROPERTIES
  PASS_REGULAR_EXPRESSION "self-hosted package scaffold preserves version 0.1.0"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_fmt_phase8_package
  COMMAND rocketc fmt "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package" --check)
add_test(NAME cli_test_phase8_package
  COMMAND rocketc test "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
set_tests_properties(cli_test_phase8_package PROPERTIES
  PASS_REGULAR_EXPRESSION "2 passed; 0 failed")
add_test(NAME cli_version COMMAND rocketc --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "rocketc 2.1.0")
add_test(NAME cli_help COMMAND rocketc --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "rocketc fmt")

add_test(NAME phase19_target_cli
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase19_target_package"
    -P "${CMAKE_SOURCE_DIR}/tests/phase19_cli_target_test.cmake")
set_tests_properties(phase19_target_cli PROPERTIES
  LABELS "phase19;target;conditional-compilation;diagnostics")
if(LLVM_FOUND)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/phase19-target-query")
  configure_file(
    "${CMAKE_SOURCE_DIR}/tests/fixtures/phase19_target_queries.rocket"
    "${CMAKE_BINARY_DIR}/phase19-target-query/main.rocket" COPYONLY)
  add_test(NAME phase19_target_queries_native
    COMMAND rocketc run
      "${CMAKE_BINARY_DIR}/phase19-target-query/main.rocket")
  set_tests_properties(phase19_target_queries_native PROPERTIES
    LABELS "phase19;target;stdlib;runtime;${ROCKET_NATIVE_TARGET}")
  add_test(NAME phase19_selfhost_target_parity
    COMMAND ${CMAKE_COMMAND}
      "-DSTAGE0=$<TARGET_FILE:rocketc>"
      "-DCOMPILER_SOURCE=${CMAKE_SOURCE_DIR}/compiler/src/main.rocket"
      "-DCOMPILER_MANIFEST=${CMAKE_SOURCE_DIR}/compiler/rocket.toml"
      "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase19_target_package"
      "-DCLANG=${LLVM_TOOLS_BINARY_DIR}/clang${CMAKE_EXECUTABLE_SUFFIX}"
      "-DRUNTIME=$<TARGET_FILE:rocket_runtime>"
      "-DHOST_ALIAS=${ROCKET_NATIVE_TARGET}"
      "-DEXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
      "-DWORK=${CMAKE_BINARY_DIR}/phase19-selfhost-target"
      -P "${CMAKE_SOURCE_DIR}/tests/phase19_selfhost_target_parity.cmake")
  set_tests_properties(phase19_selfhost_target_parity PROPERTIES
    LABELS "phase19;target;selfhost;conditional-compilation;diagnostics"
    TIMEOUT 120)
endif()

add_test(NAME phase20_build_cache
  COMMAND ${CMAKE_COMMAND}
    "-DCOMPILER=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package"
    "-DWORK=${CMAKE_BINARY_DIR}/phase20-build-cache"
    -P "${CMAKE_SOURCE_DIR}/tests/phase20_build_cache_test.cmake")
set_tests_properties(phase20_build_cache PROPERTIES
  LABELS "phase20;cache;compatibility" TIMEOUT 90)

add_test(NAME phase20_crash_minimizer
  COMMAND "${Python3_EXECUTABLE}" "${ROCKET_PORTABLE_WORKFLOW_TEST}"
    minimizer --compiler $<TARGET_FILE:rocketc>
    --input "${CMAKE_SOURCE_DIR}/tests/fixtures/phase20_minimize.rocket"
    --output "${CMAKE_BINARY_DIR}/phase20-minimized.rocket"
    --expected-exit 1 --match "R2001")
set_tests_properties(phase20_crash_minimizer PROPERTIES
  LABELS "phase20;hardening;minimizer" TIMEOUT 60)

add_test(NAME phase20_compatibility
  COMMAND "${Python3_EXECUTABLE}" "${ROCKET_PORTABLE_WORKFLOW_TEST}"
    compatibility --compiler $<TARGET_FILE:rocketc>
    --source-dir "${CMAKE_SOURCE_DIR}"
    --work "${CMAKE_BINARY_DIR}/compatibility"
    --configuration $<CONFIG> --target "${ROCKET_NATIVE_TARGET}")
set_tests_properties(phase20_compatibility PROPERTIES
  LABELS "phase20;compatibility" TIMEOUT 120)

if(LLVM_FOUND)
  add_test(NAME phase20_application_validation
    COMMAND "${Python3_EXECUTABLE}" "${ROCKET_PORTABLE_WORKFLOW_TEST}"
      application --compiler $<TARGET_FILE:rocketc>
      --source-dir "${CMAKE_SOURCE_DIR}"
      --work "${CMAKE_BINARY_DIR}/application-validation"
      --configuration $<CONFIG> --target "${ROCKET_NATIVE_TARGET}"
      --package-count 16 --iterations 3)
  set_tests_properties(phase20_application_validation PROPERTIES
    LABELS "phase20;application;performance" TIMEOUT 240)
endif()
add_test(NAME cli_phase8_invalid_manifest
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_invalid_package"
    "-DEXPECTED=error[R5001]"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_phase16_dependency_workflow
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DFIXTURE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase16_packages"
    "-DWORK=${CMAKE_BINARY_DIR}/phase16-dependencies"
    -P "${CMAKE_SOURCE_DIR}/tests/phase16_dependency_test.cmake")
set_tests_properties(cli_phase16_dependency_workflow PROPERTIES
  LABELS "phase16;package")
add_test(NAME cli_phase16_ecosystem_workflow
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DWORK=${CMAKE_BINARY_DIR}/phase16-ecosystem"
    -P "${CMAKE_SOURCE_DIR}/tests/phase16_ecosystem_test.cmake")
set_tests_properties(cli_phase16_ecosystem_workflow PROPERTIES
  LABELS "phase16;package;registry;integration")
add_test(NAME cli_phase16_self_hosted_package_parity
  COMMAND ${CMAKE_COMMAND}
    "-DSTAGE0=$<TARGET_FILE:rocketc>"
    "-DSELF_HOSTED=${ROCKET_SELFHOST_COMPILER}"
    "-DFIXTURE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase16_packages"
    "-DWORK=${CMAKE_BINARY_DIR}/phase16-self-hosted"
    -P "${CMAKE_SOURCE_DIR}/tests/phase16_self_hosted_test.cmake")
set_tests_properties(cli_phase16_self_hosted_package_parity PROPERTIES
  FIXTURES_REQUIRED self_hosted_compiler
  LABELS "phase16;package;self-hosted;integration"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_phase6_visibility
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase6_visibility.rocket"
    "-DEXPECTED=has no public callable 'hidden'"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
add_test(NAME cli_phase6_import_cycle
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase6_cycle.rocket"
    "-DEXPECTED=import cycle detected"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")

if(LLVM_FOUND)
  add_test(NAME cli_emit_ir
    COMMAND rocketc emit-ir "${CMAKE_SOURCE_DIR}/examples/hello.rocket")
  set_tests_properties(cli_emit_ir PROPERTIES
    PASS_REGULAR_EXPRESSION "define i32 @main\\(i32 %argc, ptr %argv\\)")

  add_test(NAME cli_emit_asm
    COMMAND rocketc emit-asm "${CMAKE_SOURCE_DIR}/examples/hello.rocket")
  set_tests_properties(cli_emit_asm PROPERTIES
    PASS_REGULAR_EXPRESSION "rocket_fn_main_")

  add_test(NAME cli_run_llvm_scalar
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/llvm_scalar.rocket")
  set_tests_properties(cli_run_llvm_scalar PROPERTIES
    PASS_REGULAR_EXPRESSION "8[\r\n]+1.5[\r\n]+R[\r\n]+LLVM")

  add_test(NAME cli_run_llvm_operators
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/llvm_operators.rocket")
  set_tests_properties(cli_run_llvm_operators PROPERTIES
    PASS_REGULAR_EXPRESSION "operators-ok")

  add_test(NAME cli_run_runtime_collections
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/runtime_collections.rocket")
  set_tests_properties(cli_run_runtime_collections PROPERTIES
    PASS_REGULAR_EXPRESSION "rocket[\r\n]+30[\r\n]+20[\r\n]+1[\r\n]+1.5[\r\n]+R")

  add_test(NAME cli_run_phase6_types
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase6_types.rocket")
  set_tests_properties(cli_run_phase6_types PROPERTIES
    PASS_REGULAR_EXPRESSION "10[\r\n]+20[\r\n]+generic[\r\n]+42[\r\n]+10[\r\n]+matched")

  add_test(NAME cli_run_phase6_modules
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase6_modules.rocket")
  set_tests_properties(cli_run_phase6_modules PROPERTIES
    PASS_REGULAR_EXPRESSION "6[\r\n]+4")

  add_test(NAME cli_emit_ir_phase7_stdlib
    COMMAND rocketc emit-ir "${CMAKE_SOURCE_DIR}/tests/fixtures/phase7_stdlib.rocket")
  set_tests_properties(cli_emit_ir_phase7_stdlib PROPERTIES
    PASS_REGULAR_EXPRESSION "rocket_std_json_parse")

  add_test(NAME cli_run_phase7_stdlib
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase7_stdlib.rocket")
  set_tests_properties(cli_run_phase7_stdlib PROPERTIES
    PASS_REGULAR_EXPRESSION "Rocket Standard Library[\r\n]+23")

  add_test(NAME cli_run_phase15_binary_io
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_binary_io.rocket")
  set_tests_properties(cli_run_phase15_binary_io PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-binary-ok"
    LABELS "phase15;stdlib")

  add_test(NAME cli_run_phase15_text_streams
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_text_streams.rocket")
  set_tests_properties(cli_run_phase15_text_streams PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-text-streams-ok"
    LABELS "phase15;stdlib")

  add_test(NAME cli_run_phase15_crypto
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_crypto.rocket")
  set_tests_properties(cli_run_phase15_crypto PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-crypto-ok"
    LABELS "phase15;stdlib;crypto")

  add_test(NAME cli_run_phase15_network
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_network.rocket")
  set_tests_properties(cli_run_phase15_network PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-network-ok"
    LABELS "phase15;stdlib;network")

  add_test(NAME cli_run_phase15_platform
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_platform.rocket")
  set_tests_properties(cli_run_phase15_platform PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-platform-ok"
    LABELS "phase15;stdlib;platform")

  add_test(NAME cli_run_phase15_archive
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_archive.rocket")
  set_tests_properties(cli_run_phase15_archive PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-archive-ok"
    LABELS "phase15;stdlib;archive")

  add_test(NAME cli_run_phase15_sqlite
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_sqlite.rocket")
  set_tests_properties(cli_run_phase15_sqlite PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-sqlite-ok"
    LABELS "phase15;stdlib;database")

  add_test(NAME cli_run_phase15_testing
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_testing.rocket")
  set_tests_properties(cli_run_phase15_testing PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-testing-ok"
    LABELS "phase15;stdlib;testing")

  add_test(NAME cli_run_phase11_array_mutation
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_mutation.rocket")
  set_tests_properties(cli_run_phase11_array_mutation PROPERTIES
    PASS_REGULAR_EXPRESSION "99[\r\n]+20[\r\n]+20[\r\n]+new[\r\n]+old")

  add_test(NAME cli_run_phase11_array_growth
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_growth.rocket")
  set_tests_properties(cli_run_phase11_array_growth PROPERTIES
    PASS_REGULAR_EXPRESSION "8[\r\n]+0[\r\n]+old[\r\n]+new[\r\n]+1[\r\n]+old[\r\n]+first[\r\n]+old[\r\n]+0[\r\n]+8")

  add_test(NAME cli_run_phase11_map_set_tuple
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_map_set_tuple.rocket")
  set_tests_properties(cli_run_phase11_map_set_tuple PROPERTIES
    PASS_REGULAR_EXPRESSION "answer[\r\n]+42[\r\n]+2[\r\n]+1[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+1[\r\n]+1[\r\n]+4567693929835203094[\r\n]+1[\r\n]+3[\r\n]+1[\r\n]+1[\r\n]+2[\r\n]+1[\r\n]+6[\r\n]+7[\r\n]+8[\r\n]+R")

  add_test(NAME cli_run_phase12_methods
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_methods.rocket")
  set_tests_properties(cli_run_phase12_methods PROPERTIES
    PASS_REGULAR_EXPRESSION "42[\r\n]+7[\r\n]+method[\r\n]+3[\r\n]+12")
  add_test(NAME cli_run_phase12_modules
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_modules.rocket")
  set_tests_properties(cli_run_phase12_modules PROPERTIES
    PASS_REGULAR_EXPRESSION "42")
  add_test(NAME cli_run_phase12_traits
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_traits.rocket")
  set_tests_properties(cli_run_phase12_traits PROPERTIES PASS_REGULAR_EXPRESSION "42")
  add_test(NAME cli_run_phase12_closures
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_closures.rocket")
  set_tests_properties(cli_run_phase12_closures PROPERTIES PASS_REGULAR_EXPRESSION "42")
  add_test(NAME cli_run_phase12_iterators
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_iterators.rocket")
  set_tests_properties(cli_run_phase12_iterators PROPERTIES PASS_REGULAR_EXPRESSION "6")
  add_test(NAME cli_run_phase12_associated_constants
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_associated_constants.rocket")
  set_tests_properties(cli_run_phase12_associated_constants PROPERTIES PASS_REGULAR_EXPRESSION "42")
  add_test(NAME cli_run_phase12_generic_lambdas
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_generic_lambdas.rocket")
  set_tests_properties(cli_run_phase12_generic_lambdas PROPERTIES
    PASS_REGULAR_EXPRESSION "42[\r\n]+generic[\r\n]+7[\r\n]+capture[\r\n]+9[\r\n]+immediate[\r\n]+8[\r\n]+nested[\r\n]+10[\r\n]+deep")

  add_test(NAME cli_emit_ir_phase13_native
    COMMAND rocketc emit-ir "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
  set_tests_properties(cli_emit_ir_phase13_native PROPERTIES
    PASS_REGULAR_EXPRESSION "declare i64 @phase13_add\\(i64, i64\\)")
  add_test(NAME cli_run_phase13_native
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
  set_tests_properties(cli_run_phase13_native PROPERTIES
    PASS_REGULAR_EXPRESSION "native-interop-ok")
  add_test(NAME cli_build_phase13_static_library
    COMMAND rocketc build "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library")
  set_tests_properties(cli_build_phase13_static_library PROPERTIES
    PASS_REGULAR_EXPRESSION "phase13_math${ROCKET_STATIC_LIBRARY_SUFFIX}")
  add_test(NAME cli_build_phase13_dynamic_library
    COMMAND rocketc build "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_dynamic_library")
  set_tests_properties(cli_build_phase13_dynamic_library PROPERTIES
    PASS_REGULAR_EXPRESSION "phase13_math_dynamic${ROCKET_DYNAMIC_LIBRARY_SUFFIX}")

  add_test(NAME cli_phase13_static_c_consumer
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library"
      "-DCXX=${CMAKE_CXX_COMPILER}"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/native/phase13_static_consumer.cpp"
      "-DHEADER_DIRECTORY=${ROCKET_TEST_ARTIFACT_ROOT}/phase13_math/${ROCKET_TARGET_ARTIFACT_SUFFIX}"
      "-DLIBRARY=${ROCKET_TEST_ARTIFACT_ROOT}/phase13_math/${ROCKET_TARGET_ARTIFACT_SUFFIX}/phase13_math${ROCKET_STATIC_LIBRARY_SUFFIX}"
      "-DRUNTIME=$<TARGET_FILE:rocket_runtime>"
      "-DEXECUTABLE=${CMAKE_BINARY_DIR}/phase13-consumers/static-consumer${CMAKE_EXECUTABLE_SUFFIX}"
      "-DCONFIGURATION=${CMAKE_BUILD_TYPE}"
      "-DHOST_OS=${CMAKE_SYSTEM_NAME}"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase13_consumer_test.cmake")
  add_test(NAME cli_phase13_dynamic_c_consumer
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_dynamic_library"
      "-DCXX=${CMAKE_CXX_COMPILER}"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/native/phase13_dynamic_consumer.cpp"
      "-DHEADER_DIRECTORY=${ROCKET_TEST_ARTIFACT_ROOT}/phase13_math_dynamic/${ROCKET_TARGET_ARTIFACT_SUFFIX}"
      "-DLIBRARY=${ROCKET_TEST_ARTIFACT_ROOT}/phase13_math_dynamic/${ROCKET_TARGET_ARTIFACT_SUFFIX}/phase13_math_dynamic${ROCKET_DYNAMIC_LINK_LIBRARY_SUFFIX}"
      "-DDYNAMIC_LIBRARY=${ROCKET_TEST_ARTIFACT_ROOT}/phase13_math_dynamic/${ROCKET_TARGET_ARTIFACT_SUFFIX}/phase13_math_dynamic${ROCKET_DYNAMIC_LIBRARY_SUFFIX}"
      "-DEXECUTABLE=${CMAKE_BINARY_DIR}/phase13-consumers/phase13-dynamic-consumer${CMAKE_EXECUTABLE_SUFFIX}"
      "-DCONFIGURATION=${CMAKE_BUILD_TYPE}"
      "-DHOST_OS=${CMAKE_SYSTEM_NAME}"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase13_consumer_test.cmake")

  add_test(NAME cli_self_hosted_check_phase13_native
    COMMAND "${ROCKET_SELFHOST_COMPILER}" check
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
  set_tests_properties(cli_self_hosted_check_phase13_native PROPERTIES
    PASS_REGULAR_EXPRESSION "check succeeded"
    FIXTURES_REQUIRED self_hosted_compiler)
  add_test(NAME cli_self_hosted_run_phase13_native
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
  set_tests_properties(cli_self_hosted_run_phase13_native PROPERTIES
    PASS_REGULAR_EXPRESSION "native-interop-ok"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  add_test(NAME cli_phase13_generation_parity
    COMMAND ${CMAKE_COMMAND}
      "-DSTAGE0=$<TARGET_FILE:rocketc>"
      "-DSELFHOST=${ROCKET_SELFHOST_COMPILER}"
      "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library"
      "-DHEADER=${CMAKE_SOURCE_DIR}/tests/native/phase13_native.h"
      "-DEXPECTED_BINDINGS=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package/native/phase13_native.bindings.rocket"
      "-DWORK=${CMAKE_BINARY_DIR}/phase13-generation"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase13_generation_test.cmake")
  set_tests_properties(cli_phase13_generation_parity PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler)

  add_test(NAME cli_self_hosted_check_phase14_raylib_reference
    COMMAND "${ROCKET_SELFHOST_COMPILER}" check
      "${ROCKET_RAYLIB_PACKAGE}")
  set_tests_properties(cli_self_hosted_check_phase14_raylib_reference PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib"
    PASS_REGULAR_EXPRESSION "check succeeded"
    WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
  add_test(NAME cli_self_hosted_build_phase14_raylib_reference
    COMMAND "${ROCKET_SELFHOST_COMPILER}" build
      "${ROCKET_RAYLIB_PACKAGE}")
  set_tests_properties(cli_self_hosted_build_phase14_raylib_reference PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib;native"
    PASS_REGULAR_EXPRESSION "rocket-raylib-showcase${CMAKE_EXECUTABLE_SUFFIX}"
    WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
  add_test(NAME cli_self_hosted_test_phase14_raylib_reference
    COMMAND "${ROCKET_SELFHOST_COMPILER}" test
      "${ROCKET_RAYLIB_PACKAGE}")
  set_tests_properties(cli_self_hosted_test_phase14_raylib_reference PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib;native"
    PASS_REGULAR_EXPRESSION "4 passed; 0 failed"
    WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
  add_test(NAME cli_phase14_raylib_generation_parity
    COMMAND ${CMAKE_COMMAND}
      "-DSTAGE0=$<TARGET_FILE:rocketc>"
      "-DSELFHOST=${ROCKET_SELFHOST_COMPILER}"
      "-DHEADER=${ROCKET_RAYLIB_HEADER}"
      "-DGENERATED=${ROCKET_RAYLIB_BINDINGS}"
      "-DVENDORED=${ROCKET_RAYLIB_PACKAGE}/src/rocket_raylib_adapter.rocket"
      "-DWORK=${CMAKE_BINARY_DIR}/phase14-generation"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase14_generation_test.cmake")
  set_tests_properties(cli_phase14_raylib_generation_parity PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib;native")

  add_test(NAME cli_self_hosted_run_package
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
  set_tests_properties(cli_self_hosted_run_package PROPERTIES
    PASS_REGULAR_EXPRESSION "42"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_test_package
    COMMAND "${ROCKET_SELFHOST_COMPILER}" test
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
  set_tests_properties(cli_self_hosted_test_package PROPERTIES
    PASS_REGULAR_EXPRESSION "2 passed; 0 failed"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase11_array_mutation
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_mutation.rocket")
  set_tests_properties(cli_self_hosted_run_phase11_array_mutation PROPERTIES
    PASS_REGULAR_EXPRESSION "99[\r\n]+20[\r\n]+20[\r\n]+new[\r\n]+old"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase11_array_growth
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_growth.rocket")
  set_tests_properties(cli_self_hosted_run_phase11_array_growth PROPERTIES
    PASS_REGULAR_EXPRESSION "8[\r\n]+0[\r\n]+old[\r\n]+new[\r\n]+1[\r\n]+old[\r\n]+first[\r\n]+old[\r\n]+0[\r\n]+8"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase11_map_set_tuple
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_map_set_tuple.rocket")
  set_tests_properties(cli_self_hosted_run_phase11_map_set_tuple PROPERTIES
    PASS_REGULAR_EXPRESSION "answer[\r\n]+42[\r\n]+2[\r\n]+1[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+1[\r\n]+1[\r\n]+4567693929835203094[\r\n]+1[\r\n]+3[\r\n]+1[\r\n]+1[\r\n]+2[\r\n]+1[\r\n]+6[\r\n]+7[\r\n]+8[\r\n]+R"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase12_methods
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_methods.rocket")
  set_tests_properties(cli_self_hosted_run_phase12_methods PROPERTIES
    PASS_REGULAR_EXPRESSION "42[\r\n]+7[\r\n]+method[\r\n]+3[\r\n]+12"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  add_test(NAME cli_self_hosted_run_phase12_modules
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_modules.rocket")
  set_tests_properties(cli_self_hosted_run_phase12_modules PROPERTIES
    PASS_REGULAR_EXPRESSION "42"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  foreach(feature traits closures iterators associated_constants)
    add_test(NAME cli_self_hosted_run_phase12_${feature}
      COMMAND "${ROCKET_SELFHOST_COMPILER}" run
        "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_${feature}.rocket")
    set_tests_properties(cli_self_hosted_run_phase12_${feature} PROPERTIES
      PASS_REGULAR_EXPRESSION "(42|6)"
      FIXTURES_REQUIRED self_hosted_compiler
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endforeach()
  add_test(NAME cli_self_hosted_run_phase12_generic_lambdas
    COMMAND "${ROCKET_SELFHOST_COMPILER}" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_generic_lambdas.rocket")
  set_tests_properties(cli_self_hosted_run_phase12_generic_lambdas PROPERTIES
    PASS_REGULAR_EXPRESSION "42[\r\n]+generic[\r\n]+7[\r\n]+capture[\r\n]+9[\r\n]+immediate[\r\n]+8[\r\n]+nested[\r\n]+10[\r\n]+deep"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_array_bounds_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/array_bounds.rocket"
      "-DEXPECTED=rocket runtime error: index 3 out of bounds for length 3"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")

  add_test(NAME cli_phase18_unique_buffer_bounds_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase18_unique_buffer_bounds.rocket"
      "-DEXPECTED=rocket runtime error: index 1 out of bounds for length 1"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")
  set_tests_properties(cli_phase18_unique_buffer_bounds_failure PROPERTIES
    LABELS "phase18;ownership;bounds;native")

  add_test(NAME cli_slice_bounds_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/slice_bounds.rocket"
      "-DEXPECTED=rocket runtime error: slice 1..4 out of bounds for length 3"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")

  add_test(NAME cli_array_update_bounds_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/array_update_bounds.rocket"
      "-DEXPECTED=rocket runtime error: index 2 out of bounds for length 2"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")

  add_test(NAME cli_phase11_negative_reserve_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_negative_reserve.rocket"
      "-DEXPECTED=rocket runtime error: Array reserve capacity cannot be negative"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")

  add_test(NAME cli_phase11_insert_bounds_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_insert_bounds.rocket"
      "-DEXPECTED=rocket runtime error: index 2 out of bounds for length 1"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")

  add_test(NAME cli_phase11_remove_bounds_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_remove_bounds.rocket"
      "-DEXPECTED=rocket runtime error: index 1 out of bounds for length 1"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")

  add_test(NAME cli_int_overflow_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/int_overflow.rocket"
      "-DEXPECTED=rocket runtime error: Int arithmetic overflow"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")

  add_test(NAME cli_int_division_zero_failure
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/int_division_zero.rocket"
      "-DEXPECTED=rocket runtime error: Int division by zero"
      -P "${CMAKE_SOURCE_DIR}/tests/expect_runtime_failure.cmake")
else()
  add_test(NAME cli_run_stage0_phase13_native
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
  set_tests_properties(cli_run_stage0_phase13_native PROPERTIES
    PASS_REGULAR_EXPRESSION "native-interop-ok")
  add_test(NAME cli_build_stage0_phase13_static_library
    COMMAND rocketc build "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library")
  set_tests_properties(cli_build_stage0_phase13_static_library PROPERTIES
    PASS_REGULAR_EXPRESSION "phase13_math${ROCKET_STATIC_LIBRARY_SUFFIX}")
  add_test(NAME cli_run_stage0_phase7_stdlib
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase7_stdlib.rocket")
  set_tests_properties(cli_run_stage0_phase7_stdlib PROPERTIES
    PASS_REGULAR_EXPRESSION "Rocket Standard Library[\r\n]+23")
  add_test(NAME cli_run_stage0_phase15_binary_io
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_binary_io.rocket")
  set_tests_properties(cli_run_stage0_phase15_binary_io PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-binary-ok"
    LABELS "phase15;stdlib;stage0")
  add_test(NAME cli_run_stage0_phase15_text_streams
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_text_streams.rocket")
  set_tests_properties(cli_run_stage0_phase15_text_streams PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-text-streams-ok"
    LABELS "phase15;stdlib;stage0")
  add_test(NAME cli_run_stage0_phase15_crypto
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_crypto.rocket")
  set_tests_properties(cli_run_stage0_phase15_crypto PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-crypto-ok"
    LABELS "phase15;stdlib;crypto;stage0")
  add_test(NAME cli_run_stage0_phase15_network
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_network.rocket")
  set_tests_properties(cli_run_stage0_phase15_network PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-network-ok"
    LABELS "phase15;stdlib;network;stage0")
  add_test(NAME cli_run_stage0_phase15_platform
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_platform.rocket")
  set_tests_properties(cli_run_stage0_phase15_platform PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-platform-ok"
    LABELS "phase15;stdlib;platform;stage0")
  add_test(NAME cli_run_stage0_phase15_archive
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_archive.rocket")
  set_tests_properties(cli_run_stage0_phase15_archive PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-archive-ok"
    LABELS "phase15;stdlib;archive;stage0")
  add_test(NAME cli_run_stage0_phase15_sqlite
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_sqlite.rocket")
  set_tests_properties(cli_run_stage0_phase15_sqlite PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-sqlite-ok"
    LABELS "phase15;stdlib;database;stage0")
  add_test(NAME cli_run_stage0_phase15_testing
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase15_testing.rocket")
  set_tests_properties(cli_run_stage0_phase15_testing PROPERTIES
    PASS_REGULAR_EXPRESSION "phase15-testing-ok"
    LABELS "phase15;stdlib;testing;stage0")
  add_test(NAME cli_run_stage0_phase11_array_mutation
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_mutation.rocket")
  set_tests_properties(cli_run_stage0_phase11_array_mutation PROPERTIES
    PASS_REGULAR_EXPRESSION "99[\r\n]+20[\r\n]+20[\r\n]+new[\r\n]+old")
  add_test(NAME cli_run_stage0_phase11_array_growth
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_growth.rocket")
  set_tests_properties(cli_run_stage0_phase11_array_growth PROPERTIES
    PASS_REGULAR_EXPRESSION "8[\r\n]+0[\r\n]+old[\r\n]+new[\r\n]+1[\r\n]+old[\r\n]+first[\r\n]+old[\r\n]+0[\r\n]+8")
  add_test(NAME cli_run_stage0_phase11_map_set_tuple
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_map_set_tuple.rocket")
  set_tests_properties(cli_run_stage0_phase11_map_set_tuple PROPERTIES
    PASS_REGULAR_EXPRESSION "answer[\r\n]+42[\r\n]+2[\r\n]+1[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+1[\r\n]+1[\r\n]+4567693929835203094[\r\n]+1[\r\n]+3[\r\n]+1[\r\n]+1[\r\n]+2[\r\n]+1[\r\n]+6[\r\n]+7[\r\n]+8[\r\n]+R")
  add_test(NAME cli_run_stage0_phase12_methods
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_methods.rocket")
  set_tests_properties(cli_run_stage0_phase12_methods PROPERTIES
    PASS_REGULAR_EXPRESSION "42[\r\n]+7[\r\n]+method[\r\n]+3[\r\n]+12")
  add_test(NAME cli_run_stage0_phase12_modules
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_modules.rocket")
  set_tests_properties(cli_run_stage0_phase12_modules PROPERTIES
    PASS_REGULAR_EXPRESSION "42")
  foreach(feature traits closures iterators associated_constants)
    add_test(NAME cli_run_stage0_phase12_${feature}
      COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_${feature}.rocket")
    set_tests_properties(cli_run_stage0_phase12_${feature} PROPERTIES
      PASS_REGULAR_EXPRESSION "(42|6)")
  endforeach()
  add_test(NAME cli_run_stage0_phase12_generic_lambdas
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_generic_lambdas.rocket")
  set_tests_properties(cli_run_stage0_phase12_generic_lambdas PROPERTIES
    PASS_REGULAR_EXPRESSION "42[\r\n]+generic[\r\n]+7[\r\n]+capture[\r\n]+9[\r\n]+immediate[\r\n]+8[\r\n]+nested[\r\n]+10[\r\n]+deep")
endif()

if(LLVM_FOUND)
  add_test(NAME phase17_tooling
    COMMAND "${Python3_EXECUTABLE}" "${ROCKET_PORTABLE_WORKFLOW_TEST}"
      tooling --compiler $<TARGET_FILE:rocketc>
      --source-dir "${CMAKE_SOURCE_DIR}"
      --work "${CMAKE_BINARY_DIR}/tooling")
  set_tests_properties(phase17_tooling PROPERTIES LABELS "phase17;tooling")
  add_test(NAME phase17_debugging
    COMMAND "${Python3_EXECUTABLE}" "${ROCKET_PORTABLE_WORKFLOW_TEST}"
      debugging --compiler $<TARGET_FILE:rocketc>
      --source-dir "${CMAKE_SOURCE_DIR}"
      --work "${CMAKE_BINARY_DIR}/debugging"
      --target "${ROCKET_NATIVE_TARGET}"
      --llvm-bin "${LLVM_TOOLS_BINARY_DIR}")
  set_tests_properties(phase17_debugging PROPERTIES LABELS "phase17;debugging")
  add_test(NAME phase17_repl_prototype
    COMMAND "${Python3_EXECUTABLE}" "${ROCKET_PORTABLE_WORKFLOW_TEST}"
      repl --compiler $<TARGET_FILE:rocketc>
      --source-dir "${CMAKE_SOURCE_DIR}"
      --work "${CMAKE_BINARY_DIR}/repl-prototype"
      --expression "1 + 1")
  set_tests_properties(phase17_repl_prototype PROPERTIES LABELS "phase17;repl"
    PASS_REGULAR_EXPRESSION "rocket-repl-evaluation-1")
endif()

find_program(ROCKET_NODE_EXECUTABLE node)
if(ROCKET_NODE_EXECUTABLE)
  add_test(NAME phase17_vscode_client
    COMMAND "${ROCKET_NODE_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/editors/vscode/test/client.test.js")
  set_tests_properties(phase17_vscode_client PROPERTIES LABELS "phase17;editor")
  add_test(NAME phase17_editor_neutral_lsp
    COMMAND "${ROCKET_NODE_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/editors/vscode/test/lsp-conformance.js"
      $<TARGET_FILE:rocketc> $<TARGET_FILE:rocket_lsp>)
  set_tests_properties(phase17_editor_neutral_lsp PROPERTIES
    LABELS "phase17;lsp;editor-neutral")
endif()

# Every test that starts the Rocket-written compiler must bind package-security
# operations to the Stage 0 target from this exact configuration. Applying the
# inert variable to the complete test set prevents stale sibling build trees
# from influencing Debug/Release fixture order.
get_property(ROCKET_CONFIGURED_TESTS DIRECTORY PROPERTY TESTS)
set_property(TEST lexer parser sema hir mir diagnostics language_server
  formatter package native runtime stdlib APPEND PROPERTY LABELS
  "phase20;hardening")
set(ROCKET_CONFIGURED_TEST_ENVIRONMENT
  "ROCKET_STAGE0=$<TARGET_FILE:rocketc>"
  "ROCKET_NATIVE_LIBRARY_ROOT=${ROCKET_NATIVE_OUTPUT}"
  "ROCKET_ARTIFACT_ROOT=${ROCKET_TEST_ARTIFACT_ROOT}"
  "ROCKET_NATIVE_TARGET=${ROCKET_NATIVE_TARGET}")
if(LLVM_FOUND)
  list(APPEND ROCKET_CONFIGURED_TEST_ENVIRONMENT
    "ROCKET_CLANG=${ROCKETC_CLANG_PATH}"
    "ROCKET_LIBRARIAN=${ROCKETC_TEST_LIBRARIAN_PATH}"
    "ROCKET_RUNTIME=$<TARGET_FILE:rocket_runtime>")
endif()
if(APPLE)
  list(APPEND ROCKET_CONFIGURED_TEST_ENVIRONMENT
    "ROCKET_MACOS_SDK_ROOT=${ROCKET_MACOS_SDK_ROOT}")
endif()
set_tests_properties(${ROCKET_CONFIGURED_TESTS} PROPERTIES
  ENVIRONMENT "${ROCKET_CONFIGURED_TEST_ENVIRONMENT}")

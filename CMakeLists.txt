cmake_minimum_required(VERSION 3.25)
project(Rocket VERSION 1.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/EHsc)
endif()

option(ROCKETC_ENABLE_LLVM "Enable the production LLVM backend when LLVM is installed" ON)
if(ROCKETC_ENABLE_LLVM)
  find_package(LLVM CONFIG QUIET)
endif()

add_library(rocket_runtime STATIC src/runtime.cpp src/stdlib.cpp)
target_include_directories(rocket_runtime PUBLIC src)
if(MSVC)
  target_compile_options(rocket_runtime PRIVATE /W4 /permissive-)
else()
  target_compile_options(rocket_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(ROCKET_RAYLIB_SOURCE "${CMAKE_SOURCE_DIR}/dependencies/installed/raylib-6.0")
if(NOT EXISTS "${ROCKET_RAYLIB_SOURCE}/src/raylib.h")
  message(FATAL_ERROR
    "Pinned raylib 6.0 is missing. Run dependencies/bootstrap.ps1 before configuring.")
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
set(ROCKET_RAYLIB_NATIVE_OUTPUT "${ROCKET_RAYLIB_PACKAGE}/.rocketc/native")
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
set_target_properties(rocket_phase13_native PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY
    "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package/.rocketc/native")

add_library(rocketc_core
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
    core support analysis transformutils passes target mc codegen nativecodegen
  )
  target_link_libraries(rocketc_core PRIVATE ${ROCKETC_LLVM_LIBS})
  target_compile_definitions(rocketc_core PUBLIC ROCKETC_HAS_LLVM=1)
else()
  message(WARNING "LLVM not found; only the stage0 bootstrap C++ fallback is available")
endif()

add_executable(rocketc src/main.cpp)
target_link_libraries(rocketc PRIVATE rocketc_core)

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
  file(TO_CMAKE_PATH "${LLVM_TOOLS_BINARY_DIR}/clang.exe" ROCKETC_CLANG_PATH)
  target_compile_definitions(rocketc PRIVATE
    ROCKETC_CLANG_PATH="${ROCKETC_CLANG_PATH}"
    ROCKETC_RUNTIME_LIBRARY_PATH="$<TARGET_FILE:rocket_runtime>")
  add_dependencies(rocketc rocket_runtime)
endif()

set(ROCKET_RAYLIB_HEADER
  "${ROCKET_RAYLIB_PACKAGE}/native/rocket_raylib_adapter.h")
set(ROCKET_RAYLIB_BINDINGS
  "${ROCKET_RAYLIB_PACKAGE}/generated/rocket_raylib_adapter.rocket")
add_custom_command(
  OUTPUT "${ROCKET_RAYLIB_BINDINGS}"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${ROCKET_RAYLIB_PACKAGE}/generated"
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
  COMMAND $<TARGET_FILE:rocketc> build "${ROCKET_RAYLIB_PACKAGE}"
  DEPENDS rocket_raylib_bindings
  WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}"
  USES_TERMINAL
  COMMENT "Building the Rocket raylib showcase")
add_custom_target(rocket_raylib_test
  COMMAND $<TARGET_FILE:rocketc> test "${ROCKET_RAYLIB_PACKAGE}"
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

enable_testing()
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
if(LLVM_FOUND)
  add_rocketc_test(llvm_codegen)
endif()
add_rocketc_test(diagnostics)
add_rocketc_test(language_server)
add_rocketc_test(formatter)
add_rocketc_test(package)
add_rocketc_test(native)

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

add_executable(rocketc_stdlib_tests tests/stdlib_tests.cpp)
target_link_libraries(rocketc_stdlib_tests PRIVATE rocket_runtime)
add_test(NAME stdlib COMMAND rocketc_stdlib_tests phase9-argument)

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
  PASS_REGULAR_EXPRESSION "rocket-raylib-showcase.exe"
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
add_test(NAME cli_check_self_hosted_compiler
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/compiler")
add_test(NAME cli_build_self_hosted_compiler
  COMMAND rocketc build "${CMAKE_SOURCE_DIR}/compiler")
set_tests_properties(cli_build_self_hosted_compiler PROPERTIES
  FIXTURES_SETUP self_hosted_compiler)
add_test(NAME cli_run_self_hosted_lexer_tests
  COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" --self-test-lexer)
set_tests_properties(cli_run_self_hosted_lexer_tests PROPERTIES
  PASS_REGULAR_EXPRESSION "self-hosted lexer tests passed"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_parser_tests
  COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" --self-test-parser)
set_tests_properties(cli_run_self_hosted_parser_tests PROPERTIES
  PASS_REGULAR_EXPRESSION "self-hosted parser tests passed"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_syntax_check
  COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" --check-syntax
    "${CMAKE_SOURCE_DIR}/examples/hello.rocket")
set_tests_properties(cli_run_self_hosted_syntax_check PROPERTIES
  PASS_REGULAR_EXPRESSION "hello.rocket: syntax check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_hir_check
  COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" --check-hir
    "${CMAKE_SOURCE_DIR}/compiler/src/main.rocket")
set_tests_properties(cli_run_self_hosted_hir_check PROPERTIES
  PASS_REGULAR_EXPRESSION "main.rocket: HIR check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_run_self_hosted_mir_check
  COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" --check-mir
    "${CMAKE_SOURCE_DIR}/compiler/src/main.rocket")
set_tests_properties(cli_run_self_hosted_mir_check PROPERTIES
  PASS_REGULAR_EXPRESSION "main.rocket: MIR check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_self_hosted_check_package
  COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" check
    "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
set_tests_properties(cli_self_hosted_check_package PROPERTIES
  PASS_REGULAR_EXPRESSION "check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_self_hosted_fmt_package
  COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" fmt
    "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package" --check)
set_tests_properties(cli_self_hosted_fmt_package PROPERTIES
  PASS_REGULAR_EXPRESSION "format check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler)
add_test(NAME cli_self_hosted_formatter_canonical
  COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
    -File "${CMAKE_SOURCE_DIR}/tests/self_hosted_formatter_test.ps1"
    -Compiler "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe"
    -WorkDirectory "${CMAKE_BINARY_DIR}/self-hosted-formatter")
set_tests_properties(cli_self_hosted_formatter_canonical PROPERTIES
  PASS_REGULAR_EXPRESSION "format check succeeded"
  FIXTURES_REQUIRED self_hosted_compiler
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME cli_self_hosted_new_package
  COMMAND ${CMAKE_COMMAND}
    "-DCOMPILER=${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe"
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
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "rocketc 1.7.0")
add_test(NAME cli_help COMMAND rocketc --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "rocketc fmt")
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
    "-DSELF_HOSTED=${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe"
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
    PASS_REGULAR_EXPRESSION "phase13_math.lib")
  add_test(NAME cli_build_phase13_dynamic_library
    COMMAND rocketc build "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_dynamic_library")
  set_tests_properties(cli_build_phase13_dynamic_library PROPERTIES
    PASS_REGULAR_EXPRESSION "phase13_math_dynamic.dll")

  add_test(NAME cli_phase13_static_c_consumer
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library"
      "-DCXX=${CMAKE_CXX_COMPILER}"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/native/phase13_static_consumer.cpp"
      "-DHEADER_DIRECTORY=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library/.rocketc"
      "-DLIBRARY=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library/.rocketc/phase13_math.lib"
      "-DRUNTIME=$<TARGET_FILE:rocket_runtime>"
      "-DEXECUTABLE=${CMAKE_BINARY_DIR}/phase13-consumers/static-consumer.exe"
      "-DCONFIGURATION=${CMAKE_BUILD_TYPE}"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase13_consumer_test.cmake")
  add_test(NAME cli_phase13_dynamic_c_consumer
    COMMAND ${CMAKE_COMMAND}
      "-DROCKETC=$<TARGET_FILE:rocketc>"
      "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_dynamic_library"
      "-DCXX=${CMAKE_CXX_COMPILER}"
      "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/native/phase13_dynamic_consumer.cpp"
      "-DHEADER_DIRECTORY=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_dynamic_library/.rocketc"
      "-DLIBRARY=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_dynamic_library/.rocketc/phase13_math_dynamic.lib"
      "-DEXECUTABLE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_dynamic_library/.rocketc/phase13-dynamic-consumer.exe"
      "-DCONFIGURATION=${CMAKE_BUILD_TYPE}"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase13_consumer_test.cmake")

  add_test(NAME cli_self_hosted_check_phase13_native
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" check
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
  set_tests_properties(cli_self_hosted_check_phase13_native PROPERTIES
    PASS_REGULAR_EXPRESSION "check succeeded"
    FIXTURES_REQUIRED self_hosted_compiler)
  add_test(NAME cli_self_hosted_run_phase13_native
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package")
  set_tests_properties(cli_self_hosted_run_phase13_native PROPERTIES
    PASS_REGULAR_EXPRESSION "native-interop-ok"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  add_test(NAME cli_phase13_generation_parity
    COMMAND ${CMAKE_COMMAND}
      "-DSTAGE0=$<TARGET_FILE:rocketc>"
      "-DSELFHOST=${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe"
      "-DPACKAGE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_static_library"
      "-DHEADER=${CMAKE_SOURCE_DIR}/tests/native/phase13_native.h"
      "-DEXPECTED_BINDINGS=${CMAKE_SOURCE_DIR}/tests/fixtures/phase13_native_package/native/phase13_native.bindings.rocket"
      "-DWORK=${CMAKE_BINARY_DIR}/phase13-generation"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase13_generation_test.cmake")
  set_tests_properties(cli_phase13_generation_parity PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler)

  add_test(NAME cli_self_hosted_check_phase14_raylib_reference
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" check
      "${ROCKET_RAYLIB_PACKAGE}")
  set_tests_properties(cli_self_hosted_check_phase14_raylib_reference PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib"
    PASS_REGULAR_EXPRESSION "check succeeded"
    WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
  add_test(NAME cli_self_hosted_build_phase14_raylib_reference
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" build
      "${ROCKET_RAYLIB_PACKAGE}")
  set_tests_properties(cli_self_hosted_build_phase14_raylib_reference PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib;native"
    PASS_REGULAR_EXPRESSION "rocket-raylib-showcase.exe"
    WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
  add_test(NAME cli_self_hosted_test_phase14_raylib_reference
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" test
      "${ROCKET_RAYLIB_PACKAGE}")
  set_tests_properties(cli_self_hosted_test_phase14_raylib_reference PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib;native"
    PASS_REGULAR_EXPRESSION "4 passed; 0 failed"
    WORKING_DIRECTORY "${ROCKET_RAYLIB_PACKAGE}")
  add_test(NAME cli_phase14_raylib_generation_parity
    COMMAND ${CMAKE_COMMAND}
      "-DSTAGE0=$<TARGET_FILE:rocketc>"
      "-DSELFHOST=${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe"
      "-DHEADER=${ROCKET_RAYLIB_HEADER}"
      "-DGENERATED=${ROCKET_RAYLIB_BINDINGS}"
      "-DWORK=${CMAKE_BINARY_DIR}/phase14-generation"
      -P "${CMAKE_SOURCE_DIR}/tests/native/phase14_generation_test.cmake")
  set_tests_properties(cli_phase14_raylib_generation_parity PROPERTIES
    FIXTURES_REQUIRED self_hosted_compiler
    LABELS "phase14;raylib;native")

  add_test(NAME cli_self_hosted_run_package
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
  set_tests_properties(cli_self_hosted_run_package PROPERTIES
    PASS_REGULAR_EXPRESSION "42"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_test_package
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" test
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
  set_tests_properties(cli_self_hosted_test_package PROPERTIES
    PASS_REGULAR_EXPRESSION "2 passed; 0 failed"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase11_array_mutation
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_mutation.rocket")
  set_tests_properties(cli_self_hosted_run_phase11_array_mutation PROPERTIES
    PASS_REGULAR_EXPRESSION "99[\r\n]+20[\r\n]+20[\r\n]+new[\r\n]+old"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase11_array_growth
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_array_growth.rocket")
  set_tests_properties(cli_self_hosted_run_phase11_array_growth PROPERTIES
    PASS_REGULAR_EXPRESSION "8[\r\n]+0[\r\n]+old[\r\n]+new[\r\n]+1[\r\n]+old[\r\n]+first[\r\n]+old[\r\n]+0[\r\n]+8"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase11_map_set_tuple
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase11_map_set_tuple.rocket")
  set_tests_properties(cli_self_hosted_run_phase11_map_set_tuple PROPERTIES
    PASS_REGULAR_EXPRESSION "answer[\r\n]+42[\r\n]+2[\r\n]+1[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+2[\r\n]+b[\r\n]+a[\r\n]+1[\r\n]+1[\r\n]+4567693929835203094[\r\n]+1[\r\n]+3[\r\n]+1[\r\n]+1[\r\n]+2[\r\n]+1[\r\n]+6[\r\n]+7[\r\n]+8[\r\n]+R"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  add_test(NAME cli_self_hosted_run_phase12_methods
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_methods.rocket")
  set_tests_properties(cli_self_hosted_run_phase12_methods PROPERTIES
    PASS_REGULAR_EXPRESSION "42[\r\n]+7[\r\n]+method[\r\n]+3[\r\n]+12"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  add_test(NAME cli_self_hosted_run_phase12_modules
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
      "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_modules.rocket")
  set_tests_properties(cli_self_hosted_run_phase12_modules PROPERTIES
    PASS_REGULAR_EXPRESSION "42"
    FIXTURES_REQUIRED self_hosted_compiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  foreach(feature traits closures iterators associated_constants)
    add_test(NAME cli_self_hosted_run_phase12_${feature}
      COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
        "${CMAKE_SOURCE_DIR}/tests/fixtures/phase12_${feature}.rocket")
    set_tests_properties(cli_self_hosted_run_phase12_${feature} PROPERTIES
      PASS_REGULAR_EXPRESSION "(42|6)"
      FIXTURES_REQUIRED self_hosted_compiler
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endforeach()
  add_test(NAME cli_self_hosted_run_phase12_generic_lambdas
    COMMAND "${CMAKE_SOURCE_DIR}/compiler/.rocketc/main.exe" run
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
    PASS_REGULAR_EXPRESSION "phase13_math.lib")
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
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_SOURCE_DIR}/scripts/tooling.ps1"
      -Compiler $<TARGET_FILE:rocketc>)
  set_tests_properties(phase17_tooling PROPERTIES LABELS "phase17;tooling")
  add_test(NAME phase17_debugging
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_SOURCE_DIR}/scripts/debugging.ps1"
      -Compiler $<TARGET_FILE:rocketc>)
  set_tests_properties(phase17_debugging PROPERTIES LABELS "phase17;debugging")
  add_test(NAME phase17_repl_prototype
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_SOURCE_DIR}/scripts/repl-prototype.ps1"
      -Compiler $<TARGET_FILE:rocketc> -Expression "1 + 1" -Measure)
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
set_tests_properties(${ROCKET_CONFIGURED_TESTS} PROPERTIES
  ENVIRONMENT "ROCKET_STAGE0=$<TARGET_FILE:rocketc>")

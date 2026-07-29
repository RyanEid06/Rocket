cmake_minimum_required(VERSION 3.20)
project(Rocket VERSION 1.2.0 LANGUAGES CXX)

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

add_library(rocketc_core
  src/lexer.cpp
  src/parser.cpp
  src/module_loader.cpp
  src/formatter.cpp
  src/package.cpp
  src/type.cpp
  src/hir.cpp
  src/sema.cpp
  src/mir.cpp
  src/codegen.cpp
)
target_include_directories(rocketc_core PUBLIC src)
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
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/src" ROCKETC_SOURCE_INCLUDE_PATH)
file(TO_CMAKE_PATH "${CMAKE_CXX_COMPILER}" ROCKETC_STAGE0_CXX_PATH)
target_compile_definitions(rocketc PRIVATE
  ROCKETC_SOURCE_INCLUDE_PATH="${ROCKETC_SOURCE_INCLUDE_PATH}"
  ROCKETC_STAGE0_CXX_PATH="${ROCKETC_STAGE0_CXX_PATH}")
target_compile_definitions(rocketc PRIVATE ROCKETC_VERSION="${PROJECT_VERSION}")
if(LLVM_FOUND)
  file(TO_CMAKE_PATH "${LLVM_TOOLS_BINARY_DIR}/clang.exe" ROCKETC_CLANG_PATH)
  target_compile_definitions(rocketc PRIVATE
    ROCKETC_CLANG_PATH="${ROCKETC_CLANG_PATH}"
    ROCKETC_RUNTIME_LIBRARY_PATH="$<TARGET_FILE:rocket_runtime>")
  add_dependencies(rocketc rocket_runtime)
endif()

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
add_rocketc_test(formatter)
add_rocketc_test(package)

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
add_test(NAME cli_fmt_phase8_package
  COMMAND rocketc fmt "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package" --check)
add_test(NAME cli_test_phase8_package
  COMMAND rocketc test "${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_package")
set_tests_properties(cli_test_phase8_package PROPERTIES
  PASS_REGULAR_EXPRESSION "2 passed; 0 failed")
add_test(NAME cli_version COMMAND rocketc --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "rocketc 1.2.0")
add_test(NAME cli_help COMMAND rocketc --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "rocketc fmt")
add_test(NAME cli_phase8_invalid_manifest
  COMMAND ${CMAKE_COMMAND}
    "-DROCKETC=$<TARGET_FILE:rocketc>"
    "-DSOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/phase8_invalid_package"
    "-DEXPECTED=error[R5001]"
    -P "${CMAKE_SOURCE_DIR}/tests/expect_compile_failure.cmake")
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
  add_test(NAME cli_run_stage0_phase7_stdlib
    COMMAND rocketc run "${CMAKE_SOURCE_DIR}/tests/fixtures/phase7_stdlib.rocket")
  set_tests_properties(cli_run_stage0_phase7_stdlib PROPERTIES
    PASS_REGULAR_EXPRESSION "Rocket Standard Library[\r\n]+23")
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

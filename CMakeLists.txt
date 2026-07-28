cmake_minimum_required(VERSION 3.20)
project(Rocket VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ROCKETC_ENABLE_LLVM "Enable the production LLVM backend when LLVM is installed" ON)
if(ROCKETC_ENABLE_LLVM)
  find_package(LLVM CONFIG QUIET)
endif()

add_library(rocketc_core
  src/lexer.cpp
  src/parser.cpp
  src/sema.cpp
  src/codegen.cpp
)
target_include_directories(rocketc_core PUBLIC src)
if(MSVC)
  target_compile_options(rocketc_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(rocketc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LLVM_FOUND)
  message(STATUS "LLVM ${LLVM_PACKAGE_VERSION} found; the bootstrap backend remains active until scalar LLVM lowering is implemented")
  target_compile_definitions(rocketc_core PUBLIC ROCKETC_LLVM_DISCOVERED=1)
else()
  message(STATUS "LLVM not found; using the milestone bootstrap C++ backend")
endif()

add_executable(rocketc src/main.cpp)
target_link_libraries(rocketc PRIVATE rocketc_core)

enable_testing()
function(add_rocketc_test name)
  add_executable(rocketc_${name}_tests tests/${name}_tests.cpp)
  target_link_libraries(rocketc_${name}_tests PRIVATE rocketc_core)
  add_test(NAME ${name} COMMAND rocketc_${name}_tests)
endfunction()

add_rocketc_test(lexer)
add_rocketc_test(parser)
add_rocketc_test(sema)
add_rocketc_test(codegen)
add_rocketc_test(diagnostics)

add_test(NAME cli_check_hello
  COMMAND rocketc check "${CMAKE_SOURCE_DIR}/examples/hello.rocket")

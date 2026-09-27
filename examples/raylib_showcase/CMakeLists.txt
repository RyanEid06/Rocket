cmake_minimum_required(VERSION 3.25)
project(RocketRaylibApplication LANGUAGES NONE)

set(ROCKET_ROOT "" CACHE PATH "Path to a Rocket developer checkout")
set(ROCKETC "" CACHE FILEPATH "Path to the Rocket compiler")
set(ROCKET_NATIVE_LIBRARY_ROOT "" CACHE PATH
  "Directory containing the production Rocket raylib libraries")

if(NOT ROCKET_ROOT)
  get_filename_component(ROCKET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()
if(NOT ROCKETC)
  find_program(ROCKETC NAMES rocketc rocketc.exe)
endif()
if(NOT ROCKETC AND WIN32)
  set(ROCKETC "${ROCKET_ROOT}/out/build/windows-release/rocketc.exe")
endif()
if(NOT EXISTS "${ROCKETC}")
  message(FATAL_ERROR "Rocket compiler not found: ${ROCKETC}")
endif()

if(WIN32)
  set(rocket_native_target windows-x64)
  set(adapter_library rocket_raylib_adapter.lib)
  set(raylib_library raylib.lib)
elseif(APPLE)
  set(rocket_native_target macos-arm64)
  set(adapter_library librocket_raylib_adapter.a)
  set(raylib_library libraylib.a)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
  set(rocket_native_target linux-arm64)
  set(adapter_library librocket_raylib_adapter.a)
  set(raylib_library libraylib.a)
else()
  set(rocket_native_target linux-x64)
  set(adapter_library librocket_raylib_adapter.a)
  set(raylib_library libraylib.a)
endif()

if(NOT ROCKET_NATIVE_LIBRARY_ROOT)
  get_filename_component(rocket_compiler_directory "${ROCKETC}" DIRECTORY)
  get_filename_component(rocket_sdk_root "${rocket_compiler_directory}/.." ABSOLUTE)
  if(EXISTS "${rocket_sdk_root}/lib/${adapter_library}")
    set(ROCKET_NATIVE_LIBRARY_ROOT "${rocket_sdk_root}/lib")
  elseif(EXISTS "${rocket_compiler_directory}/native/${rocket_native_target}/${adapter_library}")
    set(ROCKET_NATIVE_LIBRARY_ROOT
      "${rocket_compiler_directory}/native/${rocket_native_target}")
  endif()
endif()

foreach(library IN ITEMS "${adapter_library}" "${raylib_library}")
  if(NOT EXISTS "${ROCKET_NATIVE_LIBRARY_ROOT}/${library}")
    message(FATAL_ERROR
      "Production Rocket raylib library not found: ${ROCKET_NATIVE_LIBRARY_ROOT}/${library}. "
      "Build the repository rocket_raylib_adapter target or use a packaged Rocket SDK.")
  endif()
endforeach()

add_custom_target(rocket_app ALL
  COMMAND ${CMAKE_COMMAND} -E env
          "ROCKET_NATIVE_LIBRARY_ROOT=${ROCKET_NATIVE_LIBRARY_ROOT}"
          "${ROCKETC}" build "${CMAKE_CURRENT_SOURCE_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL)

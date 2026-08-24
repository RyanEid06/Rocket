cmake_minimum_required(VERSION 3.25)
project(RocketRaylibApplication LANGUAGES C CXX)

set(ROCKET_ROOT "" CACHE PATH "Path to the Rocket repository")
set(ROCKETC "" CACHE FILEPATH "Path to a Rocket 1.4 compiler")
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

set(raylib_source "${ROCKET_ROOT}/dependencies/installed/raylib-6.0")
if(NOT EXISTS "${raylib_source}/src/raylib.h")
  message(FATAL_ERROR "Pinned raylib 6.0 is missing; bootstrap the Rocket dependencies")
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
foreach(disabled IN ITEMS
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
  set(${disabled} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(enabled IN ITEMS
    SUPPORT_MODULE_RSHAPES SUPPORT_MODULE_RTEXTURES SUPPORT_MODULE_RTEXT
    SUPPORT_MODULE_RAUDIO SUPPORT_FILEFORMAT_PNG SUPPORT_FILEFORMAT_BMP
    SUPPORT_FILEFORMAT_JPG SUPPORT_FILEFORMAT_PNM SUPPORT_FILEFORMAT_TTF
    SUPPORT_FILEFORMAT_FNT SUPPORT_FILEFORMAT_WAV SUPPORT_FILEFORMAT_OGG
    SUPPORT_FILEFORMAT_MP3 SUPPORT_FILEFORMAT_QOA)
  set(${enabled} ON CACHE BOOL "" FORCE)
endforeach()
set(PLATFORM Desktop CACHE STRING "" FORCE)
add_subdirectory("${raylib_source}" "${CMAKE_BINARY_DIR}/raylib" EXCLUDE_FROM_ALL)

set(native_output "${CMAKE_CURRENT_BINARY_DIR}/native")
set_target_properties(raylib PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${native_output}"
  MSVC_RUNTIME_LIBRARY "MultiThreaded")
if(TARGET glfw)
  set_target_properties(glfw PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
add_library(rocket_raylib_adapter STATIC native/rocket_raylib_adapter.cpp)
target_include_directories(rocket_raylib_adapter PUBLIC native)
target_link_libraries(rocket_raylib_adapter PUBLIC raylib)
set_target_properties(rocket_raylib_adapter PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${native_output}"
  MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(bindings "${CMAKE_CURRENT_BINARY_DIR}/generated/rocket_raylib_adapter.rocket")
add_custom_command(
  OUTPUT "${bindings}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND "${ROCKETC}" bind "${CMAKE_CURRENT_SOURCE_DIR}/native/rocket_raylib_adapter.h"
          --output "${bindings}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/native/rocket_raylib_adapter.h"
  VERBATIM)

add_custom_target(rocket_app ALL
  COMMAND ${CMAKE_COMMAND} -E env
          "ROCKET_NATIVE_LIBRARY_ROOT=${native_output}"
          "${ROCKETC}" build "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS rocket_raylib_adapter "${bindings}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL)

if(NOT DEFINED ROCKETC OR NOT DEFINED PACKAGE OR NOT DEFINED CXX OR
   NOT DEFINED SOURCE OR NOT DEFINED HEADER_DIRECTORY OR NOT DEFINED LIBRARY OR
   NOT DEFINED EXECUTABLE OR NOT DEFINED CONFIGURATION)
  message(FATAL_ERROR "Phase 13 consumer test is missing required arguments")
endif()

execute_process(
  COMMAND "${ROCKETC}" build "${PACKAGE}"
  RESULT_VARIABLE build_status
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error)
if(NOT build_status EQUAL 0)
  message(FATAL_ERROR "Rocket library build failed:\n${build_output}\n${build_error}")
endif()

get_filename_component(executable_directory "${EXECUTABLE}" DIRECTORY)
file(MAKE_DIRECTORY "${executable_directory}")
set(runtime_flag /MT)
if(CONFIGURATION STREQUAL "Debug")
  set(runtime_flag /MTd)
endif()
set(link_inputs "${LIBRARY}")
if(DEFINED RUNTIME AND NOT RUNTIME STREQUAL "")
  list(APPEND link_inputs "${RUNTIME}")
endif()
if(HOST_OS STREQUAL "Windows")
  execute_process(
    COMMAND "${CXX}" /nologo /EHsc ${runtime_flag} "${SOURCE}"
            "/I${HEADER_DIRECTORY}" ${link_inputs} "/Fe:${EXECUTABLE}"
    RESULT_VARIABLE compile_status
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error)
else()
  get_filename_component(library_directory "${LIBRARY}" DIRECTORY)
  set(platform_libraries -lcurl -lcrypto -licuuc -licudata -pthread)
  set(platform_compile_options)
  if(HOST_OS STREQUAL "Linux")
    list(APPEND platform_libraries -ldl)
  elseif(HOST_OS STREQUAL "Darwin")
    if("$ENV{ROCKET_MACOS_SDK_ROOT}" STREQUAL "" OR
       NOT IS_DIRECTORY "$ENV{ROCKET_MACOS_SDK_ROOT}")
      message(FATAL_ERROR
        "Phase 13 macOS consumer test requires ROCKET_MACOS_SDK_ROOT")
    endif()
    list(APPEND platform_compile_options
      -isysroot "$ENV{ROCKET_MACOS_SDK_ROOT}")
    list(APPEND platform_libraries
      -framework Security -framework CoreFoundation)
    if(DEFINED DYNAMIC_LIBRARY AND NOT DYNAMIC_LIBRARY STREQUAL "")
      if(NOT EXISTS "${DYNAMIC_LIBRARY}")
        message(FATAL_ERROR
          "Rocket dynamic library is missing: ${DYNAMIC_LIBRARY}")
      endif()
      execute_process(
        COMMAND /usr/bin/otool -l "${DYNAMIC_LIBRARY}"
        RESULT_VARIABLE uuid_status
        OUTPUT_VARIABLE uuid_output
        ERROR_VARIABLE uuid_error)
      if(NOT uuid_status EQUAL 0 OR NOT uuid_output MATCHES "cmd LC_UUID")
        message(FATAL_ERROR
          "Rocket macOS dynamic library has no LC_UUID:\n${uuid_output}\n${uuid_error}")
      endif()
    endif()
  endif()
  execute_process(
    COMMAND "${CXX}" -std=c++20 "${SOURCE}" "-I${HEADER_DIRECTORY}"
            ${platform_compile_options} ${link_inputs} ${platform_libraries}
            "-Wl,-rpath,${library_directory}" -o "${EXECUTABLE}"
    RESULT_VARIABLE compile_status
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error)
endif()
if(NOT compile_status EQUAL 0)
  message(FATAL_ERROR "C++ ABI consumer compile failed:\n${compile_output}\n${compile_error}")
endif()

if(HOST_OS STREQUAL "Windows" AND DEFINED DYNAMIC_LIBRARY AND
   NOT DYNAMIC_LIBRARY STREQUAL "")
  if(NOT EXISTS "${DYNAMIC_LIBRARY}")
    message(FATAL_ERROR "Rocket dynamic library is missing: ${DYNAMIC_LIBRARY}")
  endif()
  file(COPY "${DYNAMIC_LIBRARY}" DESTINATION "${executable_directory}")
endif()

if(HOST_OS STREQUAL "Darwin")
  set(run_command "${CMAKE_COMMAND}" -E env
      "DYLD_LIBRARY_PATH=${library_directory}" "${EXECUTABLE}")
elseif(HOST_OS STREQUAL "Linux")
  set(run_command "${CMAKE_COMMAND}" -E env
      "LD_LIBRARY_PATH=${library_directory}" "${EXECUTABLE}")
else()
  set(run_command "${EXECUTABLE}")
endif()
execute_process(
  COMMAND ${run_command}
  RESULT_VARIABLE run_status
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error)
if(NOT run_status EQUAL 0)
  message(FATAL_ERROR "C++ ABI consumer failed with ${run_status}:\n${run_output}\n${run_error}")
endif()
message(STATUS "Phase 13 C ABI consumer passed")

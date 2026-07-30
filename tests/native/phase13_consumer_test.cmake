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
execute_process(
  COMMAND "${CXX}" /nologo /EHsc ${runtime_flag} "${SOURCE}"
          "/I${HEADER_DIRECTORY}" ${link_inputs} "/Fe:${EXECUTABLE}"
  RESULT_VARIABLE compile_status
  OUTPUT_VARIABLE compile_output
  ERROR_VARIABLE compile_error)
if(NOT compile_status EQUAL 0)
  message(FATAL_ERROR "C++ ABI consumer compile failed:\n${compile_output}\n${compile_error}")
endif()

execute_process(
  COMMAND "${EXECUTABLE}"
  RESULT_VARIABLE run_status
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error)
if(NOT run_status EQUAL 0)
  message(FATAL_ERROR "C++ ABI consumer failed with ${run_status}:\n${run_output}\n${run_error}")
endif()
message(STATUS "Phase 13 C ABI consumer passed")

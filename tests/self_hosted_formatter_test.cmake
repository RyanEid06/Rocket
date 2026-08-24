if(NOT DEFINED COMPILER OR NOT DEFINED WORK)
  message(FATAL_ERROR "self-hosted formatter test is missing required inputs")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(source_path "${WORK}/formatter-input.rocket")
set(source "import  std.string   \r\n\r\n# module comment   \r\nfn  main( )->Int: # entry   \r\n    let  values : Array [ Int ] = [ 1,2, 3 ]\r\n    let negative=-1\r\n    if  not false and values [0]==1:\r\n        print ( string.concat(\"A#\",\"B\") ) # result\r\n    return negative+2\r\n")
set(expected "import std.string\n\n# module comment\nfn main() -> Int:  # entry\n    let values: Array[Int] = [1, 2, 3]\n    let negative = -1\n    if not false and values[0] == 1:\n        print(string.concat(\"A#\", \"B\"))  # result\n    return negative + 2\n")
file(WRITE "${source_path}" "${source}")

execute_process(
  COMMAND "${COMPILER}" fmt "${source_path}"
  RESULT_VARIABLE format_result OUTPUT_VARIABLE format_output
  ERROR_VARIABLE format_error)
if(NOT format_result EQUAL 0)
  message(FATAL_ERROR "self-hosted format failed:\n${format_output}${format_error}")
endif()
file(READ "${source_path}" actual)
if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "self-hosted formatter did not produce canonical source")
endif()
execute_process(
  COMMAND "${COMPILER}" fmt "${source_path}" --check
  RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output
  ERROR_VARIABLE check_error)
if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "self-hosted format check failed:\n${check_output}${check_error}")
endif()
message(STATUS "${check_output}")

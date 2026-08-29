cmake_minimum_required(VERSION 3.20)
project(rocket3_visual_compare LANGUAGES CXX)

enable_testing()

add_library(rocket3_visual_compare STATIC
    src/comparator.cpp
)
target_compile_features(rocket3_visual_compare PUBLIC cxx_std_20)
target_include_directories(rocket3_visual_compare PUBLIC src)

add_executable(comparator_tests
    tests/comparator_tests.cpp
)
target_compile_features(comparator_tests PRIVATE cxx_std_20)
target_link_libraries(comparator_tests PRIVATE rocket3_visual_compare)

add_test(NAME comparator_tests COMMAND comparator_tests)

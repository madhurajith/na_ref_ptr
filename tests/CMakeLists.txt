cmake_minimum_required(VERSION 3.10)

add_executable(tests  uncounted_tests.cpp counted_tests.cpp tracked_tests.cpp)
target_link_libraries(tests PRIVATE naref GTest::gtest GTest::gtest_main)

gtest_discover_tests(tests)

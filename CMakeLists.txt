cmake_minimum_required(VERSION 3.10)

include(CTest)
include(GoogleTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

find_package(GTest CONFIG REQUIRED)

add_library(naref INTERFACE)
target_include_directories(naref INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

install(DIRECTORY include/ DESTINATION include)

add_subdirectory(tests)
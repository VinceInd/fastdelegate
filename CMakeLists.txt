project(Delegate)
cmake_minimum_required(VERSION 2.8.12)
add_definitions("-std=c++14")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_CXX_FLAGS "-ggdb -Wall -Werror -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include_directories("${PROJECT_SOURCE_DIR}")
add_subdirectory(demo)
add_subdirectory(test)


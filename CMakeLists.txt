cmake_minimum_required(VERSION 3.20)
project(hello_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(HELLO_BUILD_TESTS "Build unit tests" ON)
option(HELLO_BUILD_BENCHMARKS "Build benchmarks" ON)

add_library(hello src/hello.cpp)
target_include_directories(hello PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(hello PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(hello_app src/main.cpp)
target_link_libraries(hello_app PRIVATE hello)

include(FetchContent)

if(HELLO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(HELLO_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

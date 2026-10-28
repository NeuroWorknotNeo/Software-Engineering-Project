cmake_minimum_required(VERSION 3.20)
project(se_project VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SEP_BUILD_TESTS "Build unit tests" ON)
option(SEP_BUILD_BENCHMARKS "Build benchmarks" ON)

add_library(black_scholes src/black_scholes.cpp)
target_include_directories(black_scholes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(black_scholes PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(bs_app src/main.cpp)
target_link_libraries(bs_app PRIVATE black_scholes)

include(FetchContent)

if(SEP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SEP_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

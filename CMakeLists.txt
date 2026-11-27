cmake_minimum_required(VERSION 3.20)
project(markowitz VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MPT_BUILD_TESTS "Build unit tests" ON)
option(MPT_BUILD_BENCHMARKS "Build benchmarks" ON)

include(FetchContent)

# Eigen 3 (только заголовки): берём системный, если установлен, иначе скачиваем.
find_package(Eigen3 3.3 QUIET NO_MODULE)
if(NOT TARGET Eigen3::Eigen)
  FetchContent_Declare(
    eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # Нужны только заголовки: каталог без CMakeLists.txt, чтобы не собирать проект Eigen.
    SOURCE_SUBDIR do-not-configure
  )
  FetchContent_MakeAvailable(eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  target_include_directories(Eigen3::Eigen SYSTEM INTERFACE ${eigen_SOURCE_DIR})
endif()

add_library(mpt_core
  src/data/price_table.cpp
  src/stats/returns.cpp
  src/stats/covariance.cpp
  src/optim/portfolio.cpp
  src/optim/qp_solver.cpp
  src/optim/optimizer.cpp
  src/optim/frontier.cpp
  src/io/cli_options.cpp
  src/io/result_writer.cpp
)
target_include_directories(mpt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mpt_core PUBLIC Eigen3::Eigen)
target_compile_options(mpt_core PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(mpt src/main.cpp)
target_link_libraries(mpt PRIVATE mpt_core)
target_compile_options(mpt PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

if(MPT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MPT_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

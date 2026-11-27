# Google Benchmark: берём системный, если установлен, иначе скачиваем.
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
  FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googlebenchmark)
endif()

add_executable(mpt_bench bench_covariance.cpp bench_optim.cpp)
target_link_libraries(mpt_bench PRIVATE mpt_core benchmark::benchmark benchmark::benchmark_main)

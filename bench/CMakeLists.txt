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

add_executable(bs_bench bench_black_scholes.cpp)
target_link_libraries(bs_bench PRIVATE black_scholes benchmark::benchmark)

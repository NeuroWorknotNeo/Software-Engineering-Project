#include <benchmark/benchmark.h>

#include "hello/hello.hpp"

static void BM_Greeting(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(hello::greeting());
    }
}
BENCHMARK(BM_Greeting);

BENCHMARK_MAIN();

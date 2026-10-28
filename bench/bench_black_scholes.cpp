#include <benchmark/benchmark.h>

#include "quant/black_scholes.hpp"

static void BM_CallPrice(benchmark::State& state) {
    quant::OptionParams p{100.0, 100.0, 0.05, 0.2, 1.0};
    for (auto _ : state) {
        benchmark::DoNotOptimize(p);
        benchmark::DoNotOptimize(quant::callPrice(p));
    }
}
BENCHMARK(BM_CallPrice);

static void BM_PutPrice(benchmark::State& state) {
    quant::OptionParams p{100.0, 100.0, 0.05, 0.2, 1.0};
    for (auto _ : state) {
        benchmark::DoNotOptimize(p);
        benchmark::DoNotOptimize(quant::putPrice(p));
    }
}
BENCHMARK(BM_PutPrice);

BENCHMARK_MAIN();

// Бенчмарки оценок ковариации на синтетических доходностях (1000 дней × N бумаг).
#include <benchmark/benchmark.h>

#include <cstdlib>

#include "quant/stats/covariance.hpp"

namespace {

Eigen::MatrixXd syntheticReturns(Eigen::Index assets) {
    std::srand(42);
    return Eigen::MatrixXd::Random(1000, assets) * 0.02;
}

void BM_SampleCovariance(benchmark::State& state) {
    const Eigen::MatrixXd r = syntheticReturns(state.range(0));
    const quant::SampleCovariance estimator;
    for (auto _ : state) {
        benchmark::DoNotOptimize(estimator.estimate(r));
    }
}
BENCHMARK(BM_SampleCovariance)->Arg(20)->Arg(50)->Arg(200)->Unit(benchmark::kMicrosecond);

void BM_LedoitWolf(benchmark::State& state) {
    const Eigen::MatrixXd r = syntheticReturns(state.range(0));
    for (auto _ : state) {
        benchmark::DoNotOptimize(quant::ledoitWolf(r));
    }
}
BENCHMARK(BM_LedoitWolf)->Arg(20)->Arg(50)->Arg(200)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();

// Бенчмарк бэктеста: последовательный и параллельный расчёт целевых весов (ADR-5).
#include <benchmark/benchmark.h>

#include <random>
#include <string>

#include "quant/backtest/allocation_strategy.hpp"
#include "quant/backtest/backtester.hpp"
#include "quant/optim/optimizer.hpp"
#include "quant/optim/qp_solver.hpp"
#include "quant/stats/covariance.hpp"

namespace {

constexpr int kDayCount = 2016;
constexpr int kDaysPerMonth = 21;
constexpr int kMonthsPerYear = 12;
constexpr int kFirstYear = 2000;
constexpr int kWindowDays = 252;
constexpr double kDailyMean = 0.0004;
constexpr double kDailyVolatility = 0.015;
constexpr double kRiskFreeRate = 0.05;
constexpr unsigned kSeed = 42;
constexpr int kDecimalBase = 10;

std::string FormatTwoDigits(int value) {
    return (value < kDecimalBase ? "0" : "") + std::to_string(value);
}

quant::ReturnMatrix MakeSyntheticReturns(int asset_count) {
    std::mt19937 generator(kSeed);
    std::normal_distribution<double> normal(kDailyMean, kDailyVolatility);
    quant::ReturnMatrix daily;
    daily.returns.resize(kDayCount, asset_count);
    for (int day = 0; day < kDayCount; ++day) {
        const int month_index = day / kDaysPerMonth;
        daily.dates.push_back(std::to_string(kFirstYear + month_index / kMonthsPerYear) + "-" +
                              FormatTwoDigits(1 + month_index % kMonthsPerYear) + "-" +
                              FormatTwoDigits(1 + day % kDaysPerMonth));
        for (int asset = 0; asset < asset_count; ++asset) {
            daily.returns(day, asset) = normal(generator);
        }
    }
    daily.tickers.resize(static_cast<std::size_t>(asset_count));
    return daily;
}

void BM_BacktestLongOnlyMaxSharpe(benchmark::State& state) {
    const quant::ReturnMatrix daily = MakeSyntheticReturns(static_cast<int>(state.range(0)));
    const quant::ActiveSetSolver solver;
    const quant::LongOnlyOptimizer optimizer(solver);
    const quant::MaxSharpeStrategy strategy(optimizer, kRiskFreeRate);
    const quant::LedoitWolfCovariance estimator;
    quant::BacktestConfig config;
    config.window_days = kWindowDays;
    config.parallel = state.range(1) != 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(quant::RunBacktest(daily, strategy, estimator, config));
    }
}
BENCHMARK(BM_BacktestLongOnlyMaxSharpe)
    ->ArgNames({"assets", "parallel"})
    ->Args({20, 0})
    ->Args({20, 1})
    ->Args({50, 0})
    ->Args({50, 1})
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

}  // namespace

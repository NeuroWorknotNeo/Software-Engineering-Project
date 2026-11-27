// Бенчмарки оптимизации: NFR-1 (граница из 50 точек для 50 бумаг × 1000 дней за ≤ 1 с)
// и сравнение решателей QP (ADR-1).
#include <benchmark/benchmark.h>

#include <random>

#include "quant/optim/frontier.hpp"
#include "quant/optim/optimizer.hpp"
#include "quant/optim/qp_solver.hpp"
#include "quant/stats/covariance.hpp"

namespace {

constexpr int kObservationCount = 1000;
constexpr int kFrontierPoints = 50;
constexpr double kDailyVolatility = 0.02;
constexpr double kMarketFactorLoading = 0.5;
constexpr double kTradingDaysPerYear = 252.0;
constexpr unsigned kSeed = 42;

// Доходности с общим рыночным фактором, чтобы ковариация была похожа на реальную.
Eigen::MatrixXd MakeSyntheticReturns(Eigen::Index asset_count) {
    std::mt19937 generator(kSeed);
    std::normal_distribution<double> normal(0.0, kDailyVolatility);
    Eigen::MatrixXd returns(kObservationCount, asset_count);
    for (Eigen::Index day = 0; day < kObservationCount; ++day) {
        const double market = normal(generator);
        for (Eigen::Index asset = 0; asset < asset_count; ++asset) {
            returns(day, asset) = kMarketFactorLoading * market + normal(generator);
        }
    }
    return returns;
}

quant::MarketModel MakeModel(const Eigen::MatrixXd& returns) {
    quant::MarketModel model;
    model.mean_returns = returns.colwise().mean().transpose() * kTradingDaysPerYear;
    model.covariance = quant::EstimateLedoitWolf(returns).covariance * kTradingDaysPerYear;
    return model;
}

// NFR-1: оценка ковариации и граница без коротких продаж вместе.
void BM_LongOnlyFrontierWithEstimation(benchmark::State& state) {
    const Eigen::MatrixXd returns = MakeSyntheticReturns(state.range(0));
    const quant::ActiveSetSolver solver;
    const quant::LongOnlyOptimizer optimizer(solver);
    for (auto _ : state) {
        const quant::MarketModel model = MakeModel(returns);
        benchmark::DoNotOptimize(quant::BuildEfficientFrontier(model, optimizer, kFrontierPoints));
    }
}
BENCHMARK(BM_LongOnlyFrontierWithEstimation)->Arg(20)->Arg(50)->Unit(benchmark::kMillisecond);

void BM_ShortSellingFrontier(benchmark::State& state) {
    const quant::MarketModel model = MakeModel(MakeSyntheticReturns(state.range(0)));
    const quant::ShortSellingOptimizer optimizer;
    for (auto _ : state) {
        benchmark::DoNotOptimize(quant::BuildEfficientFrontier(model, optimizer, kFrontierPoints));
    }
}
BENCHMARK(BM_ShortSellingFrontier)->Arg(20)->Arg(50)->Unit(benchmark::kMillisecond);

// ADR-1: два решателя на одной задаче — портфель минимальной дисперсии без коротких продаж.
template <typename Solver>
void BM_LongOnlyMinimumVariance(benchmark::State& state) {
    const quant::MarketModel model = MakeModel(MakeSyntheticReturns(state.range(0)));
    const Solver solver;
    const quant::LongOnlyOptimizer optimizer(solver);
    for (auto _ : state) {
        benchmark::DoNotOptimize(optimizer.FindMinimumVariance(model));
    }
}
BENCHMARK(BM_LongOnlyMinimumVariance<quant::ActiveSetSolver>)
    ->Arg(20)
    ->Arg(50)
    ->Arg(100)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LongOnlyMinimumVariance<quant::ProjectedGradientSolver>)
    ->Arg(20)
    ->Arg(50)
    ->Arg(100)
    ->Unit(benchmark::kMicrosecond);

}  // namespace

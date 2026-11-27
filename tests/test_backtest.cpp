// Тесты модуля backtest: FR-16 (окно и ребалансировка без заглядывания в будущее),
// FR-17 (стратегии), FR-18 (метрики), FR-19 (издержки), ADR-5 (параллельный расчёт).
#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "quant/backtest/allocation_strategy.hpp"
#include "quant/backtest/backtester.hpp"
#include "quant/backtest/performance.hpp"
#include "quant/optim/optimizer.hpp"
#include "quant/optim/qp_solver.hpp"
#include "quant/stats/covariance.hpp"

namespace {

// Стратегия «всё в бумагу с наибольшей средней доходностью» — по её весам видно,
// какие данные попали в окно оценки.
class BestAssetStrategy final : public quant::AllocationStrategy {
public:
    [[nodiscard]] quant::Allocation Allocate(const quant::MarketModel& model) const override {
        Eigen::Index best_asset = 0;
        model.mean_returns.maxCoeff(&best_asset);
        Eigen::VectorXd weights = Eigen::VectorXd::Zero(model.mean_returns.size());
        weights(best_asset) = 1.0;
        return {weights, false};
    }

    [[nodiscard]] std::string Name() const override {
        return "best-asset";
    }
};

// Две бумаги, окно 2 дня; инвестируем в дни 2 и 3.
quant::ReturnMatrix MakeTwoAssetReturns(const std::string& last_date) {
    quant::ReturnMatrix daily;
    daily.dates = {"2024-01-26", "2024-01-29", "2024-01-30", last_date};
    daily.tickers = {"AAA", "BBB"};
    daily.returns.resize(4, 2);
    daily.returns << 0.01, -0.01,  //
        -0.02, 0.02,               //
        0.10, 0.00,                //
        0.00, 0.10;
    return daily;
}

quant::ReturnMatrix MakeRandomReturns(int day_count, int asset_count, unsigned seed) {
    std::mt19937 generator(seed);
    std::normal_distribution<double> normal(0.0004, 0.015);
    quant::ReturnMatrix daily;
    daily.returns.resize(day_count, asset_count);
    for (int day = 0; day < day_count; ++day) {
        // Ровно 21 торговый день в «месяце»: месяцы 01..12 условного года, далее следующий год.
        const int month_index = day / 21;
        const int year = 2020 + month_index / 12;
        const int month = 1 + month_index % 12;
        const int day_of_month = 1 + day % 21;
        std::ostringstream date;
        date << year << '-' << std::setfill('0') << std::setw(2) << month << '-' << std::setw(2)
             << day_of_month;
        daily.dates.push_back(date.str());
        for (int asset = 0; asset < asset_count; ++asset) {
            daily.returns(day, asset) = normal(generator);
        }
    }
    for (int asset = 0; asset < asset_count; ++asset) {
        daily.tickers.push_back("A" + std::to_string(asset));
    }
    return daily;
}

quant::BacktestConfig MakeConfig(int window_days) {
    quant::BacktestConfig config;
    config.window_days = window_days;
    return config;
}

}  // namespace

TEST(RebalanceDays, FirstDayAfterWindowAndEachNewMonth) {
    const std::vector<std::string> dates = {"2024-01-30", "2024-01-31", "2024-02-01", "2024-02-02",
                                            "2024-03-01", "2024-03-04", "2024-04-01"};
    EXPECT_EQ(quant::FindRebalanceDays(dates, 3), (std::vector<std::size_t>{3, 4, 6}));
    EXPECT_EQ(quant::FindRebalanceDays(dates, 2), (std::vector<std::size_t>{2, 4, 6}));
    EXPECT_THROW(quant::FindRebalanceDays(dates, 7), std::invalid_argument);
    EXPECT_THROW(quant::FindRebalanceDays(dates, 1), std::invalid_argument);
}

TEST(Backtester, WeightsDriftBetweenRebalances) {
    const auto daily = MakeTwoAssetReturns("2024-01-31");
    const auto result = quant::RunBacktest(daily, quant::EqualWeightStrategy{},
                                           quant::SampleCovariance{}, MakeConfig(2));
    ASSERT_EQ(result.equity.size(), 2U);
    // День 2: 0.5·1.10 + 0.5·1.00 = 1.05; день 3: позиции 0.55 и 0.50 → 0.55 + 0.55 = 1.10.
    EXPECT_NEAR(result.equity[0], 1.05, 1e-15);
    EXPECT_NEAR(result.equity[1], 1.10, 1e-15);
    EXPECT_EQ(result.rebalances.size(), 1U);
}

TEST(Backtester, RebalancesAtNewMonthWithTurnover) {
    const auto daily = MakeTwoAssetReturns("2024-02-01");
    const auto result = quant::RunBacktest(daily, quant::EqualWeightStrategy{},
                                           quant::SampleCovariance{}, MakeConfig(2));
    ASSERT_EQ(result.rebalances.size(), 2U);
    EXPECT_EQ(result.rebalances[1].date, "2024-02-01");
    // Перед ребалансировкой веса уплыли к (0.55, 0.50) / 1.05; возврат к (0.5, 0.5):
    // Σ|Δw| = 0.05 / 1.05, оборот — половина этой суммы.
    EXPECT_NEAR(result.rebalances[1].turnover, 0.025 / 1.05, 1e-15);
    EXPECT_NEAR(result.equity[1], 1.05 * 1.05, 1e-15);
}

TEST(Backtester, TransactionCostsReduceValue) {
    const auto daily = MakeTwoAssetReturns("2024-02-01");
    quant::BacktestConfig config = MakeConfig(2);
    config.transaction_cost_rate = 0.01;
    const auto result =
        quant::RunBacktest(daily, quant::EqualWeightStrategy{}, quant::SampleCovariance{}, config);
    // Первая покупка: Σ|Δw| = 1; вторая ребалансировка: Σ|Δw| = 0.05 / 1.05.
    const double first_day = (1.0 - 0.01) * 1.05;
    EXPECT_NEAR(result.equity[0], first_day, 1e-15);
    EXPECT_NEAR(result.equity[1], first_day * (1.0 - 0.01 * 0.05 / 1.05) * 1.05, 1e-15);
}

TEST(Backtester, UsesOnlyPastDataForDecisions) {
    auto daily = MakeRandomReturns(300, 4, 7);
    const BestAssetStrategy strategy;
    const auto config = MakeConfig(60);
    const auto original = quant::RunBacktest(daily, strategy, quant::SampleCovariance{}, config);

    // Портим все доходности начиная с дня ребалансировки: решение на эту дату не должно измениться.
    const auto rebalance_days = quant::FindRebalanceDays(daily.dates, config.window_days);
    const std::size_t checked = 3;
    const auto first_changed_row = static_cast<Eigen::Index>(rebalance_days[checked]);
    daily.returns.bottomRows(daily.returns.rows() - first_changed_row).setConstant(0.5);
    daily.returns.bottomRows(daily.returns.rows() - first_changed_row).col(3).setConstant(-0.5);
    const auto spoiled = quant::RunBacktest(daily, strategy, quant::SampleCovariance{}, config);
    for (std::size_t index = 0; index <= checked; ++index) {
        EXPECT_EQ(spoiled.rebalances[index].weights, original.rebalances[index].weights)
            << "rebalance " << index;
    }
    // А на следующей дате будущие (теперь прошлые) данные уже учитываются.
    EXPECT_EQ(spoiled.rebalances[checked + 1].weights(3), 0.0);
}

TEST(Backtester, ParallelMatchesSequential) {
    const auto daily = MakeRandomReturns(600, 8, 11);
    const quant::ActiveSetSolver solver;
    const quant::LongOnlyOptimizer optimizer(solver);
    const quant::MinimumVarianceStrategy strategy(optimizer);
    quant::BacktestConfig config = MakeConfig(120);
    config.parallel = false;
    const auto sequential =
        quant::RunBacktest(daily, strategy, quant::LedoitWolfCovariance{}, config);
    config.parallel = true;
    const auto parallel =
        quant::RunBacktest(daily, strategy, quant::LedoitWolfCovariance{}, config);
    ASSERT_EQ(parallel.rebalances.size(), sequential.rebalances.size());
    for (std::size_t index = 0; index < parallel.rebalances.size(); ++index) {
        EXPECT_EQ(parallel.rebalances[index].weights, sequential.rebalances[index].weights);
    }
    EXPECT_EQ(parallel.equity, sequential.equity);
}

TEST(Backtester, MaxSharpeFallsBackToMinimumVariance) {
    const auto daily = MakeRandomReturns(300, 5, 3);
    const quant::ActiveSetSolver solver;
    const quant::LongOnlyOptimizer optimizer(solver);
    // При ставке 1000% ни одна бумага не обгоняет безрисковую ставку.
    const quant::MaxSharpeStrategy max_sharpe(optimizer, 10.0);
    const quant::MinimumVarianceStrategy minimum_variance(optimizer);
    const auto config = MakeConfig(60);
    const auto fallback = quant::RunBacktest(daily, max_sharpe, quant::SampleCovariance{}, config);
    const auto reference =
        quant::RunBacktest(daily, minimum_variance, quant::SampleCovariance{}, config);
    EXPECT_EQ(fallback.equity, reference.equity);
    const auto metrics = quant::ComputePerformance(fallback, 0.0);
    EXPECT_EQ(metrics.fallback_count, metrics.rebalance_count);
    EXPECT_EQ(fallback.strategy_name, "max-sharpe/long-only");
}

TEST(Performance, ComputesMetricsFromEquityCurve) {
    quant::BacktestResult result;
    result.equity = {1.1, 0.99, 1.21};
    result.rebalances = {{"d0", Eigen::VectorXd(), 0.5, false},
                         {"d1", Eigen::VectorXd(), 0.2, true},
                         {"d2", Eigen::VectorXd(), 0.4, false}};
    const auto metrics = quant::ComputePerformance(result, 0.0);
    EXPECT_NEAR(metrics.total_return, 0.21, 1e-15);
    EXPECT_NEAR(metrics.annual_return, std::pow(1.21, 252.0 / 3.0) - 1.0, 1e-9);
    EXPECT_NEAR(metrics.max_drawdown, 0.1, 1e-15);
    EXPECT_NEAR(metrics.average_turnover, 0.3, 1e-15);
    EXPECT_EQ(metrics.rebalance_count, 3);
    EXPECT_EQ(metrics.fallback_count, 1);

    // Дневные доходности: 0.1, −0.1, 2/9.
    const double mean = (0.1 - 0.1 + 2.0 / 9.0) / 3.0;
    const double variance =
        (std::pow(0.1 - mean, 2) + std::pow(-0.1 - mean, 2) + std::pow(2.0 / 9.0 - mean, 2)) / 2.0;
    EXPECT_NEAR(metrics.annual_volatility, std::sqrt(variance * 252.0), 1e-12);
    EXPECT_NEAR(metrics.sharpe_ratio, mean * 252.0 / metrics.annual_volatility, 1e-12);
    EXPECT_THROW(quant::ComputePerformance(quant::BacktestResult{}, 0.0), std::invalid_argument);
}

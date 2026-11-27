/**
 * @file performance.cpp
 * @brief Реализация метрик стратегии.
 */
#include "quant/backtest/performance.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "quant/stats/returns.hpp"

namespace quant {

namespace {

Eigen::VectorXd ComputeDailyReturns(const std::vector<double>& equity) {
    Eigen::VectorXd daily_returns(static_cast<Eigen::Index>(equity.size()));
    double previous_value = 1.0;
    for (std::size_t day = 0; day < equity.size(); ++day) {
        daily_returns(static_cast<Eigen::Index>(day)) =
            previous_value > 0.0 ? equity[day] / previous_value - 1.0 : 0.0;
        previous_value = equity[day];
    }
    return daily_returns;
}

double ComputeMaxDrawdown(const std::vector<double>& equity) {
    double peak = 1.0;
    double max_drawdown = 0.0;
    for (const double value : equity) {
        peak = std::max(peak, value);
        max_drawdown = std::max(max_drawdown, 1.0 - value / peak);
    }
    return max_drawdown;
}

}  // namespace

PerformanceMetrics ComputePerformance(const BacktestResult& result, double risk_free_rate) {
    if (result.equity.size() < 2) {
        throw std::invalid_argument("equity curve needs at least two days");
    }
    const auto day_count = static_cast<double>(result.equity.size());
    const Eigen::VectorXd daily_returns = ComputeDailyReturns(result.equity);
    const double mean_daily_return = daily_returns.mean();
    const double daily_variance =
        (daily_returns.array() - mean_daily_return).square().sum() / (day_count - 1.0);

    PerformanceMetrics metrics;
    const double final_value = result.equity.back();
    metrics.total_return = final_value - 1.0;
    metrics.annual_return = std::pow(final_value, kTradingDaysPerYear / day_count) - 1.0;
    metrics.annual_volatility = std::sqrt(daily_variance * kTradingDaysPerYear);
    if (metrics.annual_volatility > 0.0) {
        metrics.sharpe_ratio =
            (mean_daily_return * kTradingDaysPerYear - risk_free_rate) / metrics.annual_volatility;
    }
    metrics.max_drawdown = ComputeMaxDrawdown(result.equity);

    metrics.rebalance_count = static_cast<int>(result.rebalances.size());
    double turnover_sum = 0.0;
    for (std::size_t index = 0; index < result.rebalances.size(); ++index) {
        if (index > 0) {
            turnover_sum += result.rebalances[index].turnover;
        }
        if (result.rebalances[index].is_fallback) {
            ++metrics.fallback_count;
        }
    }
    if (metrics.rebalance_count > 1) {
        metrics.average_turnover = turnover_sum / (metrics.rebalance_count - 1);
    }
    return metrics;
}

}  // namespace quant

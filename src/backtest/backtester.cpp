/**
 * @file backtester.cpp
 * @brief Реализация бэктеста.
 */
#include "quant/backtest/backtester.hpp"

#include <algorithm>
#include <functional>
#include <future>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace quant {

namespace {

// Позиция месяца в дате YYYY-MM-DD.
constexpr std::size_t kYearMonthLength = 7;

std::string_view YearMonth(const std::string& date) {
    return std::string_view(date).substr(0, kYearMonthLength);
}

MarketModel EstimateWindowModel(const ReturnMatrix& daily, std::size_t rebalance_day,
                                int window_days, const CovarianceEstimator& estimator) {
    const auto first_row = static_cast<Eigen::Index>(rebalance_day) - window_days;
    const Eigen::MatrixXd window = daily.returns.middleRows(first_row, window_days);
    MarketModel model;
    model.tickers = daily.tickers;
    model.mean_returns = ComputeMeanReturns(window) * kTradingDaysPerYear;
    model.covariance = estimator.Estimate(window) * kTradingDaysPerYear;
    return model;
}

std::vector<Allocation> AllocateRange(const ReturnMatrix& daily,
                                      const std::vector<std::size_t>& rebalance_days,
                                      std::size_t begin, std::size_t end,
                                      const AllocationStrategy& strategy,
                                      const CovarianceEstimator& estimator, int window_days) {
    std::vector<Allocation> allocations;
    allocations.reserve(end - begin);
    for (std::size_t index = begin; index < end; ++index) {
        allocations.push_back(strategy.Allocate(
            EstimateWindowModel(daily, rebalance_days[index], window_days, estimator)));
    }
    return allocations;
}

// Целевые веса на все даты ребалансировки. Они не зависят друг от друга, поэтому при
// config.parallel даты делятся на блоки по числу ядер и считаются в std::async.
std::vector<Allocation> AllocateAll(const ReturnMatrix& daily,
                                    const std::vector<std::size_t>& rebalance_days,
                                    const AllocationStrategy& strategy,
                                    const CovarianceEstimator& estimator,
                                    const BacktestConfig& config) {
    const std::size_t date_count = rebalance_days.size();
    const std::size_t thread_count =
        config.parallel ? std::max(1U, std::thread::hardware_concurrency()) : 1;
    if (thread_count == 1 || date_count < 2) {
        return AllocateRange(daily, rebalance_days, 0, date_count, strategy, estimator,
                             config.window_days);
    }

    const std::size_t block_size = (date_count + thread_count - 1) / thread_count;
    std::vector<std::future<std::vector<Allocation>>> blocks;
    for (std::size_t begin = 0; begin < date_count; begin += block_size) {
        const std::size_t end = std::min(begin + block_size, date_count);
        blocks.push_back(std::async(std::launch::async, AllocateRange, std::cref(daily),
                                    std::cref(rebalance_days), begin, end, std::cref(strategy),
                                    std::cref(estimator), config.window_days));
    }
    std::vector<Allocation> allocations;
    allocations.reserve(date_count);
    for (auto& block : blocks) {
        auto block_allocations = block.get();
        std::move(block_allocations.begin(), block_allocations.end(),
                  std::back_inserter(allocations));
    }
    return allocations;
}

}  // namespace

std::vector<std::size_t> FindRebalanceDays(const std::vector<std::string>& dates, int window_days) {
    if (window_days < 2) {
        throw std::invalid_argument("backtest window must be at least two days");
    }
    const auto first_day = static_cast<std::size_t>(window_days);
    if (dates.size() <= first_day) {
        throw std::invalid_argument("not enough data: " + std::to_string(dates.size()) +
                                    " daily returns for a window of " +
                                    std::to_string(window_days) + " days");
    }
    std::vector<std::size_t> rebalance_days = {first_day};
    for (std::size_t day = first_day + 1; day < dates.size(); ++day) {
        if (YearMonth(dates[day]) != YearMonth(dates[day - 1])) {
            rebalance_days.push_back(day);
        }
    }
    return rebalance_days;
}

BacktestResult RunBacktest(const ReturnMatrix& daily, const AllocationStrategy& strategy,
                           const CovarianceEstimator& estimator, const BacktestConfig& config) {
    const auto rebalance_days = FindRebalanceDays(daily.dates, config.window_days);
    const auto allocations = AllocateAll(daily, rebalance_days, strategy, estimator, config);

    BacktestResult result;
    result.strategy_name = strategy.Name();
    const auto asset_count = daily.returns.cols();
    Eigen::VectorXd weights = Eigen::VectorXd::Zero(asset_count);
    double value = 1.0;
    std::size_t next_rebalance = 0;

    for (std::size_t day = rebalance_days.front(); day < daily.dates.size(); ++day) {
        if (next_rebalance < rebalance_days.size() && rebalance_days[next_rebalance] == day) {
            const Allocation& allocation = allocations[next_rebalance];
            const double traded = (allocation.weights - weights).cwiseAbs().sum();
            value *= 1.0 - config.transaction_cost_rate * traded;
            result.rebalances.push_back(
                {daily.dates[day], allocation.weights, traded / 2.0, allocation.is_fallback});
            weights = allocation.weights;
            ++next_rebalance;
        }

        const Eigen::VectorXd asset_growth =
            daily.returns.row(static_cast<Eigen::Index>(day)).transpose().array() + 1.0;
        const double portfolio_growth = weights.dot(asset_growth);
        value = std::max(value * portfolio_growth, 0.0);
        if (portfolio_growth > 0.0) {
            weights = weights.cwiseProduct(asset_growth) / portfolio_growth;
        }
        result.dates.push_back(daily.dates[day]);
        result.equity.push_back(value);
    }
    return result;
}

}  // namespace quant

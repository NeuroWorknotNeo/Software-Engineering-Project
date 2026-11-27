/**
 * @file allocation_strategy.cpp
 * @brief Реализация стратегий распределения.
 */
#include "quant/backtest/allocation_strategy.hpp"

#include <stdexcept>

namespace quant {

MinimumVarianceStrategy::MinimumVarianceStrategy(const PortfolioOptimizer& optimizer)
    : optimizer_(optimizer) {}

Allocation MinimumVarianceStrategy::Allocate(const MarketModel& model) const {
    return {optimizer_.FindMinimumVariance(model), false};
}

std::string MinimumVarianceStrategy::Name() const {
    return "min-variance/" + optimizer_.Name();
}

MaxSharpeStrategy::MaxSharpeStrategy(const PortfolioOptimizer& optimizer, double risk_free_rate)
    : optimizer_(optimizer), risk_free_rate_(risk_free_rate) {}

Allocation MaxSharpeStrategy::Allocate(const MarketModel& model) const {
    try {
        return {optimizer_.FindMaxSharpe(model, risk_free_rate_), false};
    } catch (const std::domain_error&) {
        return {optimizer_.FindMinimumVariance(model), true};
    }
}

std::string MaxSharpeStrategy::Name() const {
    return "max-sharpe/" + optimizer_.Name();
}

Allocation EqualWeightStrategy::Allocate(const MarketModel& model) const {
    const auto asset_count = model.mean_returns.size();
    if (asset_count == 0) {
        throw std::invalid_argument("market model has no assets");
    }
    return {Eigen::VectorXd::Constant(asset_count, 1.0 / static_cast<double>(asset_count)), false};
}

std::string EqualWeightStrategy::Name() const {
    return "equal-weight";
}

}  // namespace quant

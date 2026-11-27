/**
 * @file frontier.cpp
 * @brief Реализация построения эффективной границы.
 */
#include "quant/optim/frontier.hpp"

#include <algorithm>
#include <stdexcept>

namespace quant {

std::vector<Portfolio> BuildEfficientFrontier(const MarketModel& model,
                                              const PortfolioOptimizer& optimizer,
                                              int point_count) {
    if (point_count < 2) {
        throw std::invalid_argument("efficient frontier needs at least two points");
    }
    const Portfolio minimum_variance =
        EvaluatePortfolio(model, optimizer.FindMinimumVariance(model));
    const double lowest_target = minimum_variance.expected_return;
    const double highest_target = std::max(model.mean_returns.maxCoeff(), lowest_target);
    const double return_step = (highest_target - lowest_target) / (point_count - 1);

    std::vector<Portfolio> frontier;
    frontier.reserve(static_cast<std::size_t>(point_count));
    frontier.push_back(minimum_variance);
    for (int point = 1; point < point_count; ++point) {
        const double target_return = lowest_target + point * return_step;
        frontier.push_back(
            EvaluatePortfolio(model, optimizer.FindMinimumVarianceForReturn(model, target_return)));
    }
    return frontier;
}

}  // namespace quant

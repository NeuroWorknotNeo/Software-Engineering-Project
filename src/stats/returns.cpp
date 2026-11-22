/**
 * @file returns.cpp
 * @brief Реализация расчёта доходностей.
 */
#include "quant/stats/returns.hpp"

#include <stdexcept>

namespace quant {

ReturnMatrix ComputeSimpleReturns(const PriceTable& table) {
    const auto& prices = table.prices;
    if (prices.rows() < 2) {
        throw std::invalid_argument("need at least two dates to compute returns");
    }
    if (prices.hasNaN()) {
        throw std::invalid_argument("price table has missing values; fill them first");
    }

    ReturnMatrix result;
    result.dates.assign(table.dates.begin() + 1, table.dates.end());
    result.tickers = table.tickers;
    const auto return_count = prices.rows() - 1;
    result.returns =
        (prices.bottomRows(return_count).array() / prices.topRows(return_count).array() - 1.0)
            .matrix();
    return result;
}

Eigen::VectorXd ComputeMeanReturns(const Eigen::MatrixXd& returns) {
    if (returns.rows() == 0 || returns.cols() == 0) {
        throw std::invalid_argument("return matrix is empty");
    }
    return returns.colwise().mean().transpose();
}

}  // namespace quant

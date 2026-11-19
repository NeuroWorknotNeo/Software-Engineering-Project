/**
 * @file returns.cpp
 * @brief Реализация расчёта доходностей.
 */
#include "quant/stats/returns.hpp"

#include <stdexcept>

namespace quant {

ReturnMatrix simpleReturns(const PriceTable& prices) {
    const auto& p = prices.prices;
    if (p.rows() < 2) {
        throw std::invalid_argument("need at least two dates to compute returns");
    }
    if (p.hasNaN()) {
        throw std::invalid_argument("price table has missing values; call forwardFill first");
    }

    ReturnMatrix result;
    result.dates.assign(prices.dates.begin() + 1, prices.dates.end());
    result.tickers = prices.tickers;
    const auto t = p.rows() - 1;
    result.returns = (p.bottomRows(t).array() / p.topRows(t).array() - 1.0).matrix();
    return result;
}

Eigen::VectorXd meanReturns(const Eigen::MatrixXd& returns) {
    if (returns.rows() == 0 || returns.cols() == 0) {
        throw std::invalid_argument("return matrix is empty");
    }
    return returns.colwise().mean().transpose();
}

}  // namespace quant

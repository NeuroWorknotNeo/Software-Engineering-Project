/**
 * @file portfolio.cpp
 * @brief Реализация характеристик портфеля.
 */
#include "quant/optim/portfolio.hpp"

#include <cmath>
#include <stdexcept>

namespace quant {

namespace {

constexpr double kSymmetryTolerance = 1e-10;

}  // namespace

void ValidateMarketModel(const MarketModel& model) {
    const auto asset_count = model.mean_returns.size();
    if (asset_count == 0) {
        throw std::invalid_argument("market model has no assets");
    }
    if (model.covariance.rows() != asset_count || model.covariance.cols() != asset_count) {
        throw std::invalid_argument("covariance size does not match the number of assets");
    }
    if (!model.tickers.empty() && static_cast<Eigen::Index>(model.tickers.size()) != asset_count) {
        throw std::invalid_argument("number of tickers does not match the number of assets");
    }
    const double asymmetry =
        (model.covariance - model.covariance.transpose()).cwiseAbs().maxCoeff();
    if (asymmetry > kSymmetryTolerance * (1.0 + model.covariance.cwiseAbs().maxCoeff())) {
        throw std::invalid_argument("covariance matrix is not symmetric");
    }
}

Portfolio EvaluatePortfolio(const MarketModel& model, const Eigen::VectorXd& weights) {
    if (weights.size() != model.mean_returns.size()) {
        throw std::invalid_argument("number of weights does not match the number of assets");
    }
    Portfolio portfolio;
    portfolio.weights = weights;
    portfolio.expected_return = model.mean_returns.dot(weights);
    const double variance = weights.dot(model.covariance * weights);
    portfolio.volatility = std::sqrt(std::fmax(variance, 0.0));
    return portfolio;
}

double ComputeSharpeRatio(const Portfolio& portfolio, double risk_free_rate) {
    if (!(portfolio.volatility > 0.0)) {
        throw std::invalid_argument("Sharpe ratio is undefined for a zero-volatility portfolio");
    }
    return (portfolio.expected_return - risk_free_rate) / portfolio.volatility;
}

}  // namespace quant

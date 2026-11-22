/**
 * @file covariance.cpp
 * @brief Реализация оценок ковариации.
 */
#include "quant/stats/covariance.hpp"

#include <algorithm>
#include <stdexcept>

namespace quant {

namespace {

Eigen::MatrixXd CenterColumns(const Eigen::MatrixXd& returns) {
    if (returns.rows() < 2 || returns.cols() == 0) {
        throw std::invalid_argument("need at least two observations to estimate covariance");
    }
    return returns.rowwise() - returns.colwise().mean();
}

}  // namespace

Eigen::MatrixXd SampleCovariance::Estimate(const Eigen::MatrixXd& returns) const {
    const Eigen::MatrixXd centered = CenterColumns(returns);
    return (centered.transpose() * centered) / static_cast<double>(centered.rows() - 1);
}

std::string SampleCovariance::Name() const {
    return "sample";
}

LedoitWolfResult EstimateLedoitWolf(const Eigen::MatrixXd& returns) {
    const Eigen::MatrixXd centered = CenterColumns(returns);
    const auto observation_count = static_cast<double>(centered.rows());
    const auto asset_count = static_cast<double>(centered.cols());

    const Eigen::MatrixXd biased_covariance = (centered.transpose() * centered) / observation_count;
    const double target_scale = biased_covariance.trace() / asset_count;

    // Оптимальное сжатие δ = min(b², d²) / d², где d² = ‖S − m·I‖² / N — разброс S
    // вокруг цели, b² = Σ_k ‖x_k·x_kᵀ − S‖² / (N·T²) = (Σ_k ‖x_k‖⁴ / T − ‖S‖²) / (N·T) —
    // оценка шума в S; x_k — k-е центрированное наблюдение.
    const Eigen::MatrixXd squared = centered.array().square().matrix();
    const double sum_of_fourth_powers = (squared.transpose() * squared).sum();
    const double covariance_squared_norm = biased_covariance.squaredNorm();
    const double estimation_noise =
        (sum_of_fourth_powers / observation_count - covariance_squared_norm) /
        (asset_count * observation_count);
    const double distance_to_target =
        (covariance_squared_norm - 2.0 * target_scale * biased_covariance.trace() +
         asset_count * target_scale * target_scale) /
        asset_count;

    LedoitWolfResult result;
    if (distance_to_target > 0.0) {
        result.shrinkage =
            std::clamp(estimation_noise, 0.0, distance_to_target) / distance_to_target;
    }
    result.covariance = (1.0 - result.shrinkage) * biased_covariance;
    result.covariance.diagonal().array() += result.shrinkage * target_scale;
    return result;
}

Eigen::MatrixXd LedoitWolfCovariance::Estimate(const Eigen::MatrixXd& returns) const {
    return EstimateLedoitWolf(returns).covariance;
}

std::string LedoitWolfCovariance::Name() const {
    return "ledoit-wolf";
}

std::unique_ptr<CovarianceEstimator> MakeCovarianceEstimator(const std::string& name) {
    if (name == "sample") {
        return std::make_unique<SampleCovariance>();
    }
    if (name == "ledoit-wolf" || name == "lw") {
        return std::make_unique<LedoitWolfCovariance>();
    }
    throw std::invalid_argument("unknown covariance estimator '" + name +
                                "', expected 'sample' or 'ledoit-wolf'");
}

}  // namespace quant

/**
 * @file covariance.cpp
 * @brief Реализация оценок ковариации.
 */
#include "quant/stats/covariance.hpp"

#include <algorithm>
#include <stdexcept>

namespace quant {

namespace {

Eigen::MatrixXd centered(const Eigen::MatrixXd& returns) {
    if (returns.rows() < 2 || returns.cols() == 0) {
        throw std::invalid_argument("need at least two observations to estimate covariance");
    }
    return returns.rowwise() - returns.colwise().mean();
}

}  // namespace

Eigen::MatrixXd SampleCovariance::estimate(const Eigen::MatrixXd& returns) const {
    const Eigen::MatrixXd x = centered(returns);
    return (x.transpose() * x) / static_cast<double>(x.rows() - 1);
}

LedoitWolfResult ledoitWolf(const Eigen::MatrixXd& returns) {
    const Eigen::MatrixXd x = centered(returns);
    const auto t = static_cast<double>(x.rows());
    const auto n = static_cast<double>(x.cols());

    const Eigen::MatrixXd s = (x.transpose() * x) / t;
    const double m = s.trace() / n;

    // δ* = min(b̄², d²) / d², где d² = ‖S − mI‖²/N — разброс S вокруг цели,
    // b̄² = (1/T²)·Σ_k ‖x_k x_kᵀ − S‖²/N = (Σ_k ‖x_k‖⁴/T − ‖S‖²)/(N·T) — оценка шума S.
    const Eigen::MatrixXd x2 = x.array().square().matrix();
    const double sumFourth = (x2.transpose() * x2).sum();  // Σ_k ‖x_k‖⁴
    const double sumS2 = s.squaredNorm();                  // ‖S‖²
    const double b2 = (sumFourth / t - sumS2) / (n * t);
    const double d2 = (sumS2 - 2.0 * m * s.trace() + n * m * m) / n;

    double shrinkage = 0.0;
    if (d2 > 0.0) {
        shrinkage = std::clamp(b2, 0.0, d2) / d2;
    }

    LedoitWolfResult result;
    result.shrinkage = shrinkage;
    result.covariance = (1.0 - shrinkage) * s;
    result.covariance.diagonal().array() += shrinkage * m;
    return result;
}

std::unique_ptr<CovarianceEstimator> makeCovarianceEstimator(const std::string& name) {
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

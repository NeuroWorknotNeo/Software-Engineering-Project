/**
 * @file covariance.hpp
 * @brief Оценки ковариационной матрицы доходностей.
 */
#pragma once

#include <Eigen/Dense>

#include <memory>
#include <string>

namespace quant {

/**
 * @brief Интерфейс оценки ковариационной матрицы (паттерн Strategy).
 */
class CovarianceEstimator {
public:
    virtual ~CovarianceEstimator() = default;

    /**
     * @brief Оценивает ковариацию по матрице доходностей.
     * @param returns Доходности T × N (строка — наблюдение, столбец — бумага).
     * @return Симметричная матрица N × N.
     * @throws std::invalid_argument если наблюдений недостаточно.
     */
    [[nodiscard]] virtual Eigen::MatrixXd estimate(const Eigen::MatrixXd& returns) const = 0;

    /// Короткое имя метода для вывода: "sample" или "ledoit-wolf".
    [[nodiscard]] virtual std::string name() const = 0;
};

/**
 * @brief Несмещённая выборочная ковариация (делитель T − 1).
 */
class SampleCovariance final : public CovarianceEstimator {
public:
    /// @copydoc CovarianceEstimator::estimate
    [[nodiscard]] Eigen::MatrixXd estimate(const Eigen::MatrixXd& returns) const override;
    [[nodiscard]] std::string name() const override { return "sample"; }
};

/**
 * @brief Результат оценки Ledoit–Wolf.
 */
struct LedoitWolfResult {
    Eigen::MatrixXd covariance;  ///< Оценка ковариации.
    double shrinkage;            ///< Коэффициент сжатия δ ∈ [0, 1].
};

/**
 * @brief Ковариация со сжатием Ledoit–Wolf к масштабированной единичной матрице.
 *
 * Σ = (1 − δ)·S + δ·m·I, где S — выборочная ковариация (делитель T),
 * m = tr(S) / N, а δ выбирается так, чтобы минимизировать ожидаемую ошибку
 * (Ledoit, Wolf, 2004, "A well-conditioned estimator for large-dimensional
 * covariance matrices"). Формулы совпадают с sklearn.covariance.ledoit_wolf.
 *
 * @param returns Доходности T × N, T ≥ 2.
 * @return Оценка и коэффициент сжатия.
 * @throws std::invalid_argument если T < 2.
 */
LedoitWolfResult ledoitWolf(const Eigen::MatrixXd& returns);

/**
 * @brief Оценка Ledoit–Wolf как стратегия; коэффициент сжатия — через ledoitWolf().
 */
class LedoitWolfCovariance final : public CovarianceEstimator {
public:
    /// @copydoc CovarianceEstimator::estimate
    [[nodiscard]] Eigen::MatrixXd estimate(const Eigen::MatrixXd& returns) const override {
        return ledoitWolf(returns).covariance;
    }
    [[nodiscard]] std::string name() const override { return "ledoit-wolf"; }
};

/**
 * @brief Создаёт оценщик по имени.
 * @param name "sample" или "ledoit-wolf" (также "lw").
 * @return Оценщик.
 * @throws std::invalid_argument для неизвестного имени.
 */
std::unique_ptr<CovarianceEstimator> makeCovarianceEstimator(const std::string& name);

}  // namespace quant

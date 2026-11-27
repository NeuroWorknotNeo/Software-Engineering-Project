/**
 * @file optimizer.hpp
 * @brief Оптимальные портфели Марковица: с короткими продажами (аналитически)
 *        и без них (численно, через QpSolver).
 */
#pragma once

#include <Eigen/Dense>
#include <string>

#include "quant/optim/portfolio.hpp"
#include "quant/optim/qp_solver.hpp"

namespace quant {

/**
 * @brief Интерфейс оптимизатора портфеля (паттерн Strategy).
 *
 * Все портфели полностью инвестированы: сумма весов равна 1.
 */
class PortfolioOptimizer {
public:
    virtual ~PortfolioOptimizer() = default;

    /**
     * @brief Портфель минимальной дисперсии (GMV).
     * @param model Модель рынка.
     * @return Веса портфеля.
     * @throws std::invalid_argument если ковариация не положительно определена.
     */
    [[nodiscard]] virtual Eigen::VectorXd FindMinimumVariance(const MarketModel& model) const = 0;

    /**
     * @brief Портфель минимальной дисперсии с заданной ожидаемой доходностью.
     * @param model Модель рынка.
     * @param target_return Целевая доходность μᵀw.
     * @return Веса портфеля.
     * @throws std::invalid_argument если доходность недостижима.
     */
    [[nodiscard]] virtual Eigen::VectorXd FindMinimumVarianceForReturn(
        const MarketModel& model, double target_return) const = 0;

    /**
     * @brief Портфель максимального коэффициента Шарпа (касательный портфель).
     * @param model Модель рынка.
     * @param risk_free_rate Безрисковая ставка (годовая).
     * @return Веса портфеля.
     * @throws std::domain_error если портфеля с положительным коэффициентом Шарпа нет.
     */
    [[nodiscard]] virtual Eigen::VectorXd FindMaxSharpe(const MarketModel& model,
                                                        double risk_free_rate) const = 0;

    /// Короткое имя для вывода: "short" или "long-only".
    [[nodiscard]] virtual std::string Name() const = 0;
};

/**
 * @brief Оптимизатор с разрешёнными короткими продажами: решения через множители Лагранжа.
 *
 * GMV: w = Σ⁻¹1 / (1ᵀΣ⁻¹1). Касательный портфель: w = Σ⁻¹(μ − r_f·1) / 1ᵀΣ⁻¹(μ − r_f·1).
 * Граница: w = Σ⁻¹(λ·1 + γ·μ), где λ = (C − B·r)/D, γ = (A·r − B)/D,
 * A = 1ᵀΣ⁻¹1, B = 1ᵀΣ⁻¹μ, C = μᵀΣ⁻¹μ, D = AC − B².
 * Системы решаются разложением Холецкого, без явного обращения Σ.
 */
class ShortSellingOptimizer final : public PortfolioOptimizer {
public:
    [[nodiscard]] Eigen::VectorXd FindMinimumVariance(const MarketModel& model) const override;

    [[nodiscard]] Eigen::VectorXd FindMinimumVarianceForReturn(const MarketModel& model,
                                                               double target_return) const override;

    [[nodiscard]] Eigen::VectorXd FindMaxSharpe(const MarketModel& model,
                                                double risk_free_rate) const override;

    [[nodiscard]] std::string Name() const override;
};

/**
 * @brief Оптимизатор без коротких продаж (w ≥ 0): задачи сводятся к QP и решаются QpSolver.
 *
 * GMV: min ½·wᵀΣw при 1ᵀw = 1, w ≥ 0.
 * Заданная доходность: то же плюс μᵀw = r.
 * Касательный портфель: min ½·yᵀΣy при (μ − r_f·1)ᵀy = 1, y ≥ 0, затем w = y / 1ᵀy.
 */
class LongOnlyOptimizer final : public PortfolioOptimizer {
public:
    /**
     * @brief Создаёт оптимизатор.
     * @param solver Решатель QP; должен жить дольше оптимизатора.
     */
    explicit LongOnlyOptimizer(const QpSolver& solver);

    [[nodiscard]] Eigen::VectorXd FindMinimumVariance(const MarketModel& model) const override;

    [[nodiscard]] Eigen::VectorXd FindMinimumVarianceForReturn(const MarketModel& model,
                                                               double target_return) const override;

    [[nodiscard]] Eigen::VectorXd FindMaxSharpe(const MarketModel& model,
                                                double risk_free_rate) const override;

    [[nodiscard]] std::string Name() const override;

private:
    const QpSolver& solver_;
};

}  // namespace quant

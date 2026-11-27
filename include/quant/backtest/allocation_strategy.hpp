/**
 * @file allocation_strategy.hpp
 * @brief Правила распределения капитала для бэктеста.
 */
#pragma once

#include <Eigen/Dense>
#include <string>

#include "quant/optim/optimizer.hpp"
#include "quant/optim/portfolio.hpp"

namespace quant {

/**
 * @brief Решение стратегии на дату ребалансировки.
 */
struct Allocation {
    /// Целевые веса бумаг, в сумме 1.
    Eigen::VectorXd weights;

    /// true, если основной портфель не существует и использован запасной вариант.
    bool is_fallback = false;
};

/**
 * @brief Интерфейс стратегии распределения (паттерн Strategy).
 *
 * Реализации не хранят изменяемого состояния, поэтому их можно вызывать
 * из нескольких потоков одновременно.
 */
class AllocationStrategy {
public:
    virtual ~AllocationStrategy() = default;

    /**
     * @brief Выбирает веса по модели рынка, оценённой на данных до даты ребалансировки.
     * @param model Модель рынка (годовые μ и Σ).
     * @return Решение стратегии.
     */
    [[nodiscard]] virtual Allocation Allocate(const MarketModel& model) const = 0;

    /// Короткое имя стратегии для вывода, например "max-sharpe/long-only".
    [[nodiscard]] virtual std::string Name() const = 0;
};

/**
 * @brief Портфель минимальной дисперсии.
 */
class MinimumVarianceStrategy final : public AllocationStrategy {
public:
    /**
     * @param optimizer Оптимизатор; должен жить дольше стратегии.
     */
    explicit MinimumVarianceStrategy(const PortfolioOptimizer& optimizer);

    [[nodiscard]] Allocation Allocate(const MarketModel& model) const override;

    [[nodiscard]] std::string Name() const override;

private:
    const PortfolioOptimizer& optimizer_;
};

/**
 * @brief Портфель максимального коэффициента Шарпа.
 *
 * Если такого портфеля нет (PortfolioOptimizer::FindMaxSharpe бросает std::domain_error),
 * берётся портфель минимальной дисперсии и решение помечается как запасное.
 */
class MaxSharpeStrategy final : public AllocationStrategy {
public:
    /**
     * @param optimizer Оптимизатор; должен жить дольше стратегии.
     * @param risk_free_rate Безрисковая ставка (годовая).
     */
    MaxSharpeStrategy(const PortfolioOptimizer& optimizer, double risk_free_rate);

    [[nodiscard]] Allocation Allocate(const MarketModel& model) const override;

    [[nodiscard]] std::string Name() const override;

private:
    const PortfolioOptimizer& optimizer_;
    double risk_free_rate_;
};

/**
 * @brief Равновзвешенный портфель 1/N — эталон для сравнения.
 */
class EqualWeightStrategy final : public AllocationStrategy {
public:
    [[nodiscard]] Allocation Allocate(const MarketModel& model) const override;

    [[nodiscard]] std::string Name() const override;
};

}  // namespace quant

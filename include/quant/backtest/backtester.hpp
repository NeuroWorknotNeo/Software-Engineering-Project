/**
 * @file backtester.hpp
 * @brief Бэктест стратегии со скользящим окном и ежемесячной ребалансировкой.
 */
#pragma once

#include <Eigen/Dense>
#include <string>
#include <vector>

#include "quant/backtest/allocation_strategy.hpp"
#include "quant/stats/covariance.hpp"
#include "quant/stats/returns.hpp"

namespace quant {

/// Окно оценки по умолчанию: один торговый год.
inline constexpr int kDefaultBacktestWindowDays = 252;

/**
 * @brief Параметры бэктеста.
 */
struct BacktestConfig {
    /// Длина окна оценки μ и Σ в торговых днях.
    int window_days = kDefaultBacktestWindowDays;

    /// Пропорциональные издержки: доля от суммы |Δw| при ребалансировке.
    double transaction_cost_rate = 0.0;

    /// Считать целевые веса на разных датах параллельно (std::async).
    bool parallel = true;
};

/**
 * @brief Одна ребалансировка.
 */
struct Rebalance {
    /// Дата, с доходности которой портфель держится в новых весах.
    std::string date;

    /// Целевые веса.
    Eigen::VectorXd weights;

    /// Оборот: ½·Σ|w_new − w_old|, где w_old — веса, «уплывшие» с прошлой ребалансировки.
    double turnover = 0.0;

    /// true, если стратегия использовала запасной портфель.
    bool is_fallback = false;
};

/**
 * @brief Результат бэктеста одной стратегии.
 */
struct BacktestResult {
    /// Имя стратегии.
    std::string strategy_name;

    /// Даты, за которые посчитана стоимость портфеля.
    std::vector<std::string> dates;

    /// Стоимость портфеля на конец каждого дня; начальная стоимость — 1.
    std::vector<double> equity;

    /// Ребалансировки в порядке дат.
    std::vector<Rebalance> rebalances;
};

/**
 * @brief Индексы дней ребалансировки: первый день после окна и первый торговый день
 *        каждого следующего месяца.
 * @param dates Даты доходностей YYYY-MM-DD.
 * @param window_days Длина окна оценки.
 * @return Индексы строк матрицы доходностей.
 * @throws std::invalid_argument если данных не больше, чем длина окна.
 */
std::vector<std::size_t> FindRebalanceDays(const std::vector<std::string>& dates, int window_days);

/**
 * @brief Прогоняет стратегию по истории.
 *
 * На дату ребалансировки t модель рынка оценивается только по доходностям дней
 * [t − window, t): решение принимается по закрытию дня t − 1 и не использует будущее.
 * Между ребалансировками веса меняются вместе с ценами (портфель не докупается).
 *
 * @param daily Дневные доходности.
 * @param strategy Стратегия распределения.
 * @param estimator Оценка ковариации для окна.
 * @param config Параметры.
 * @return Кривая капитала и история ребалансировок.
 * @throws std::invalid_argument если окно меньше двух дней или данных не хватает.
 */
BacktestResult RunBacktest(const ReturnMatrix& daily, const AllocationStrategy& strategy,
                           const CovarianceEstimator& estimator, const BacktestConfig& config);

}  // namespace quant

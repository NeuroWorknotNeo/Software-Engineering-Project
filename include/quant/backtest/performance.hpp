/**
 * @file performance.hpp
 * @brief Метрики качества стратегии по результатам бэктеста.
 */
#pragma once

#include "quant/backtest/backtester.hpp"

namespace quant {

/**
 * @brief Метрики стратегии.
 */
struct PerformanceMetrics {
    /// Полная доходность за период: V_end / V_0 − 1.
    double total_return = 0.0;

    /// Среднегодовая доходность (CAGR): (V_end / V_0)^(252 / дней) − 1.
    double annual_return = 0.0;

    /// Годовая волатильность: стандартное отклонение дневных доходностей × √252.
    double annual_volatility = 0.0;

    /// Коэффициент Шарпа: (средняя дневная доходность × 252 − r_f) / годовая волатильность.
    double sharpe_ratio = 0.0;

    /// Максимальная просадка: наибольшее падение стоимости от предыдущего максимума (≥ 0).
    double max_drawdown = 0.0;

    /// Средний оборот на ребалансировку, без первой (первая — покупка с нуля).
    double average_turnover = 0.0;

    /// Число ребалансировок.
    int rebalance_count = 0;

    /// Число ребалансировок, на которых использован запасной портфель.
    int fallback_count = 0;
};

/**
 * @brief Считает метрики по кривой капитала.
 * @param result Результат бэктеста (начальная стоимость — 1).
 * @param risk_free_rate Безрисковая ставка (годовая) для коэффициента Шарпа.
 * @return Метрики.
 * @throws std::invalid_argument если в кривой капитала меньше двух дней.
 */
PerformanceMetrics ComputePerformance(const BacktestResult& result, double risk_free_rate);

}  // namespace quant

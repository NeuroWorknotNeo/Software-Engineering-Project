/**
 * @file portfolio.hpp
 * @brief Модель рынка (μ, Σ) и характеристики портфеля.
 */
#pragma once

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace quant {

/**
 * @brief Входные данные оптимизации: ожидаемые доходности и ковариация бумаг.
 *
 * Все величины — годовые.
 */
struct MarketModel {
    /// Тикеры в порядке элементов @ref mean_returns.
    std::vector<std::string> tickers;

    /// Ожидаемые доходности μ (длина N).
    Eigen::VectorXd mean_returns;

    /// Ковариационная матрица Σ (N × N), симметричная положительно определённая.
    Eigen::MatrixXd covariance;
};

/**
 * @brief Портфель и его характеристики.
 */
struct Portfolio {
    /// Доли бумаг, в сумме 1; отрицательная доля — короткая позиция.
    Eigen::VectorXd weights;

    /// Ожидаемая доходность μᵀw.
    double expected_return = 0.0;

    /// Волатильность √(wᵀΣw).
    double volatility = 0.0;
};

/**
 * @brief Считает доходность и волатильность портфеля с заданными весами.
 * @param model Модель рынка.
 * @param weights Веса бумаг (длина N).
 * @return Портфель с посчитанными характеристиками.
 * @throws std::invalid_argument если длина весов не совпадает с числом бумаг.
 */
Portfolio EvaluatePortfolio(const MarketModel& model, const Eigen::VectorXd& weights);

/**
 * @brief Коэффициент Шарпа (μ_p − r_f) / σ_p.
 * @param portfolio Портфель.
 * @param risk_free_rate Безрисковая ставка (годовая).
 * @return Коэффициент Шарпа.
 * @throws std::invalid_argument если волатильность портфеля нулевая.
 */
double ComputeSharpeRatio(const Portfolio& portfolio, double risk_free_rate);

/**
 * @brief Проверяет согласованность размеров μ и Σ и симметричность Σ.
 * @param model Модель рынка.
 * @throws std::invalid_argument если модель некорректна.
 */
void ValidateMarketModel(const MarketModel& model);

}  // namespace quant

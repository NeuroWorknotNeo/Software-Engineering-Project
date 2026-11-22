/**
 * @file returns.hpp
 * @brief Доходности и их средние.
 */
#pragma once

#include <Eigen/Dense>
#include <string>
#include <vector>

#include "quant/data/price_table.hpp"

namespace quant {

/// Число торговых дней в году для перевода дневных величин в годовые.
inline constexpr double kTradingDaysPerYear = 252.0;

/**
 * @brief Дневные доходности бумаг.
 *
 * Строка матрицы @ref returns — день, столбец — бумага.
 */
struct ReturnMatrix {
    /// Дата, на конец которой посчитана доходность.
    std::vector<std::string> dates;

    /// Тикеры в порядке столбцов.
    std::vector<std::string> tickers;

    /// Доходности: dates.size() × tickers.size().
    Eigen::MatrixXd returns;
};

/**
 * @brief Простые дневные доходности r_t = P_t / P_{t−1} − 1.
 * @param table Таблица цен без пропусков (см. FillMissingPrices()).
 * @return Доходности; дней на один меньше, чем в таблице цен.
 * @throws std::invalid_argument если в таблице меньше двух дней или есть пропуски.
 */
ReturnMatrix ComputeSimpleReturns(const PriceTable& table);

/**
 * @brief Средняя доходность каждой бумаги (по столбцам).
 * @param returns Матрица доходностей T × N, T ≥ 1.
 * @return Вектор средних длины N.
 * @throws std::invalid_argument если матрица пустая.
 */
Eigen::VectorXd ComputeMeanReturns(const Eigen::MatrixXd& returns);

}  // namespace quant

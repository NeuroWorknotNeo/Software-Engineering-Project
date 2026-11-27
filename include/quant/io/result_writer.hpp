/**
 * @file result_writer.hpp
 * @brief Запись результатов в CSV.
 */
#pragma once

#include <Eigen/Dense>
#include <string>
#include <vector>

#include "quant/backtest/backtester.hpp"
#include "quant/backtest/performance.hpp"
#include "quant/optim/portfolio.hpp"

namespace quant {

/**
 * @brief Портфель с подписями для вывода.
 */
struct LabeledPortfolio {
    /// Режим: "short" или "long-only".
    std::string mode;

    /// Название: "min-variance", "max-sharpe" или номер точки границы.
    std::string name;

    /// Портфель.
    Portfolio portfolio;
};

/**
 * @brief Записывает квадратную матрицу с подписями строк и столбцов.
 * @param path Путь к файлу.
 * @param tickers Подписи (длина равна размеру матрицы).
 * @param matrix Матрица.
 * @throws std::runtime_error если файл не открывается на запись.
 */
void WriteMatrixCsv(const std::string& path, const std::vector<std::string>& tickers,
                    const Eigen::MatrixXd& matrix);

/**
 * @brief Записывает портфели: `mode,name,expected_return,volatility,sharpe,TICKER1,...`.
 * @param path Путь к файлу.
 * @param tickers Тикеры в порядке весов.
 * @param portfolios Портфели.
 * @param risk_free_rate Безрисковая ставка для коэффициента Шарпа.
 * @throws std::runtime_error если файл не открывается на запись.
 */
void WritePortfoliosCsv(const std::string& path, const std::vector<std::string>& tickers,
                        const std::vector<LabeledPortfolio>& portfolios, double risk_free_rate);

/**
 * @brief Записывает кривые капитала: `date,STRATEGY1,STRATEGY2,...`.
 *
 * Все результаты должны быть посчитаны на одних и тех же датах.
 *
 * @param path Путь к файлу.
 * @param results Результаты бэктестов.
 * @throws std::invalid_argument если даты результатов не совпадают.
 * @throws std::runtime_error если файл не открывается на запись.
 */
void WriteEquityCsv(const std::string& path, const std::vector<BacktestResult>& results);

/**
 * @brief Записывает историю ребалансировок:
 *        `strategy,date,turnover,is_fallback,TICKER1,...`.
 * @param path Путь к файлу.
 * @param tickers Тикеры в порядке весов.
 * @param results Результаты бэктестов.
 * @throws std::runtime_error если файл не открывается на запись.
 */
void WriteRebalancesCsv(const std::string& path, const std::vector<std::string>& tickers,
                        const std::vector<BacktestResult>& results);

/**
 * @brief Записывает метрики стратегий: `strategy,total_return,annual_return,...`.
 * @param path Путь к файлу.
 * @param strategy_names Имена стратегий.
 * @param metrics Метрики в том же порядке.
 * @throws std::runtime_error если файл не открывается на запись.
 */
void WriteMetricsCsv(const std::string& path, const std::vector<std::string>& strategy_names,
                     const std::vector<PerformanceMetrics>& metrics);

}  // namespace quant

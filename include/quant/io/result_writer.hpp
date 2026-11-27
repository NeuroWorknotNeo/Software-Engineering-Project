/**
 * @file result_writer.hpp
 * @brief Запись результатов в CSV.
 */
#pragma once

#include <Eigen/Dense>
#include <string>
#include <vector>

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

}  // namespace quant

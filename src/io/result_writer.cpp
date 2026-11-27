/**
 * @file result_writer.cpp
 * @brief Реализация записи результатов в CSV.
 */
#include "quant/io/result_writer.hpp"

#include <fstream>
#include <stdexcept>

namespace quant {

namespace {

constexpr int kCsvPrecision = 12;

std::ofstream OpenOutputFile(const std::string& path) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write file: " + path);
    }
    output.precision(kCsvPrecision);
    return output;
}

}  // namespace

void WriteMatrixCsv(const std::string& path, const std::vector<std::string>& tickers,
                    const Eigen::MatrixXd& matrix) {
    auto output = OpenOutputFile(path);
    output << "ticker";
    for (const auto& ticker : tickers) {
        output << ',' << ticker;
    }
    output << '\n';
    for (Eigen::Index row = 0; row < matrix.rows(); ++row) {
        output << tickers[static_cast<std::size_t>(row)];
        for (Eigen::Index column = 0; column < matrix.cols(); ++column) {
            output << ',' << matrix(row, column);
        }
        output << '\n';
    }
}

void WritePortfoliosCsv(const std::string& path, const std::vector<std::string>& tickers,
                        const std::vector<LabeledPortfolio>& portfolios, double risk_free_rate) {
    auto output = OpenOutputFile(path);
    output << "mode,name,expected_return,volatility,sharpe";
    for (const auto& ticker : tickers) {
        output << ',' << ticker;
    }
    output << '\n';
    for (const auto& labeled : portfolios) {
        const Portfolio& portfolio = labeled.portfolio;
        output << labeled.mode << ',' << labeled.name << ',' << portfolio.expected_return << ','
               << portfolio.volatility << ',' << ComputeSharpeRatio(portfolio, risk_free_rate);
        for (Eigen::Index asset = 0; asset < portfolio.weights.size(); ++asset) {
            output << ',' << portfolio.weights(asset);
        }
        output << '\n';
    }
}

}  // namespace quant

/**
 * @file result_writer.cpp
 * @brief Реализация записи результатов в CSV.
 */
#include "quant/io/result_writer.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

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

void WriteEquityCsv(const std::string& path, const std::vector<BacktestResult>& results) {
    if (results.empty()) {
        throw std::invalid_argument("no backtest results to write");
    }
    for (const auto& result : results) {
        if (result.dates != results.front().dates) {
            throw std::invalid_argument("backtest results cover different dates");
        }
    }
    auto output = OpenOutputFile(path);
    output << "date";
    for (const auto& result : results) {
        output << ',' << result.strategy_name;
    }
    output << '\n';
    const auto& dates = results.front().dates;
    for (std::size_t day = 0; day < dates.size(); ++day) {
        output << dates[day];
        for (const auto& result : results) {
            output << ',' << result.equity[day];
        }
        output << '\n';
    }
}

void WriteRebalancesCsv(const std::string& path, const std::vector<std::string>& tickers,
                        const std::vector<BacktestResult>& results) {
    auto output = OpenOutputFile(path);
    output << "strategy,date,turnover,is_fallback";
    for (const auto& ticker : tickers) {
        output << ',' << ticker;
    }
    output << '\n';
    for (const auto& result : results) {
        for (const auto& rebalance : result.rebalances) {
            output << result.strategy_name << ',' << rebalance.date << ',' << rebalance.turnover
                   << ',' << (rebalance.is_fallback ? 1 : 0);
            for (Eigen::Index asset = 0; asset < rebalance.weights.size(); ++asset) {
                output << ',' << rebalance.weights(asset);
            }
            output << '\n';
        }
    }
}

void WriteMetricsCsv(const std::string& path, const std::vector<std::string>& strategy_names,
                     const std::vector<PerformanceMetrics>& metrics) {
    auto output = OpenOutputFile(path);
    output << "strategy,total_return,annual_return,annual_volatility,sharpe_ratio,max_drawdown,"
              "average_turnover,rebalance_count,fallback_count\n";
    for (std::size_t index = 0; index < metrics.size(); ++index) {
        const auto& item = metrics[index];
        output << strategy_names[index] << ',' << item.total_return << ',' << item.annual_return
               << ',' << item.annual_volatility << ',' << item.sharpe_ratio << ','
               << item.max_drawdown << ',' << item.average_turnover << ',' << item.rebalance_count
               << ',' << item.fallback_count << '\n';
    }
}

}  // namespace quant

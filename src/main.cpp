/**
 * @file main.cpp
 * @brief Консольная утилита mpt: портфельный анализ по Марковицу.
 *
 * Сейчас реализована команда `stats`; `optimize`, `frontier` и `backtest`
 * в разработке (см. docs/REQUIREMENTS.md).
 */
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "quant/data/price_table.hpp"
#include "quant/stats/covariance.hpp"
#include "quant/stats/returns.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBadInput = 1;
constexpr int kExitCalcError = 2;

const char* const kUsage = R"(Usage: mpt <command> [options]

Commands:
  stats      Annual mean returns, volatilities and covariance of assets
  optimize   Optimal portfolios (in development)
  frontier   Efficient frontier (in development)
  backtest   Rolling backtest (in development)

Options for 'stats':
  --prices PATH             CSV with prices: date,TICKER1,TICKER2,... (required)
  --splits PATH             CSV with splits: date,ticker,ratio
  --cov sample|ledoit-wolf  Covariance estimator (default: sample)
  --anomaly-threshold X     Warn about daily |returns| above X (default: 0.5)
  --out PATH                Write the annual covariance matrix to CSV
  --help                    Show this help
)";

/// Разобранные параметры командной строки: имя → значение.
using Options = std::map<std::string, std::string>;

Options parseOptions(int argc, char* argv[], int first) {
    Options options;
    for (int i = first; i < argc; ++i) {
        const std::string key = argv[i];
        if (key.rfind("--", 0) != 0) {
            throw std::invalid_argument("unexpected argument '" + key + "'");
        }
        if (key == "--help") {
            options["help"] = "";
            continue;
        }
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value for " + key);
        }
        options[key.substr(2)] = argv[++i];
    }
    return options;
}

std::string require(const Options& options, const std::string& name) {
    const auto it = options.find(name);
    if (it == options.end()) {
        throw std::invalid_argument("--" + name + " is required");
    }
    return it->second;
}

std::string valueOr(const Options& options, const std::string& name, std::string fallback) {
    const auto it = options.find(name);
    return it == options.end() ? std::move(fallback) : it->second;
}

double toDouble(const std::string& text, const std::string& name) {
    std::size_t pos = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos != text.size() || text.empty()) {
        throw std::invalid_argument("--" + name + " must be a number, got '" + text + "'");
    }
    return value;
}

void writeMatrixCsv(const std::string& path, const std::vector<std::string>& tickers,
                    const Eigen::MatrixXd& m) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write file: " + path);
    }
    out << std::setprecision(12) << "ticker";
    for (const auto& t : tickers) {
        out << ',' << t;
    }
    out << '\n';
    for (Eigen::Index i = 0; i < m.rows(); ++i) {
        out << tickers[static_cast<std::size_t>(i)];
        for (Eigen::Index j = 0; j < m.cols(); ++j) {
            out << ',' << m(i, j);
        }
        out << '\n';
    }
}

int runStats(const Options& options) {
    auto prices = quant::readPriceCsv(require(options, "prices"));
    if (const auto it = options.find("splits"); it != options.end()) {
        quant::applySplits(prices, quant::readSplitsCsv(it->second));
    }
    quant::forwardFill(prices);

    const double threshold =
        toDouble(valueOr(options, "anomaly-threshold", "0.5"), "anomaly-threshold");
    for (const auto& a : quant::findAnomalies(prices, threshold)) {
        std::cerr << "warning: " << a.date << ' ' << a.ticker << " daily return "
                  << std::fixed << std::setprecision(1) << a.dailyReturn * 100.0
                  << "% (unadjusted split or bad data?)\n";
    }

    const auto returns = quant::simpleReturns(prices);
    const auto estimator = quant::makeCovarianceEstimator(valueOr(options, "cov", "sample"));
    const Eigen::VectorXd mu = quant::meanReturns(returns.returns) * quant::kTradingDaysPerYear;
    const Eigen::MatrixXd sigma =
        estimator->estimate(returns.returns) * quant::kTradingDaysPerYear;

    std::cout << "Period: " << returns.dates.front() << " .. " << returns.dates.back() << " ("
              << returns.returns.rows() << " daily returns, " << returns.tickers.size()
              << " assets)\n"
              << "Covariance: " << estimator->name();
    if (estimator->name() == "ledoit-wolf") {
        std::cout << ", shrinkage " << std::fixed << std::setprecision(4)
                  << quant::ledoitWolf(returns.returns).shrinkage;
    }
    std::cout << "\n\n"
              << std::left << std::setw(8) << "ticker" << std::right << std::setw(12)
              << "mean, %/y" << std::setw(12) << "vol, %/y" << '\n'
              << std::fixed << std::setprecision(2);
    for (std::size_t j = 0; j < returns.tickers.size(); ++j) {
        const auto k = static_cast<Eigen::Index>(j);
        std::cout << std::left << std::setw(8) << returns.tickers[j] << std::right
                  << std::setw(12) << mu(k) * 100.0 << std::setw(12)
                  << std::sqrt(sigma(k, k)) * 100.0 << '\n';
    }

    if (const auto it = options.find("out"); it != options.end()) {
        writeMatrixCsv(it->second, returns.tickers, sigma);
        std::cout << "\nCovariance matrix written to " << it->second << '\n';
    }
    return kExitOk;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "help") {
        std::cout << kUsage;
        return argc < 2 ? kExitBadInput : kExitOk;
    }

    const std::string command = argv[1];
    try {
        const Options options = parseOptions(argc, argv, 2);
        if (options.contains("help")) {
            std::cout << kUsage;
            return kExitOk;
        }
        if (command == "stats") {
            return runStats(options);
        }
        if (command == "optimize" || command == "frontier" || command == "backtest") {
            std::cerr << "error: command '" << command << "' is not implemented yet\n";
            return kExitBadInput;
        }
        std::cerr << "error: unknown command '" << command << "'\n\n" << kUsage;
        return kExitBadInput;
    } catch (const std::invalid_argument& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kExitBadInput;
    } catch (const std::runtime_error& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kExitBadInput;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kExitCalcError;
    }
}

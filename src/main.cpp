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
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "quant/data/price_table.hpp"
#include "quant/stats/covariance.hpp"
#include "quant/stats/returns.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBadInput = 1;
constexpr int kExitCalculationError = 2;

constexpr double kPercent = 100.0;
constexpr int kTickerColumnWidth = 8;
constexpr int kNumberColumnWidth = 12;
constexpr int kPercentPrecision = 2;
constexpr int kShrinkagePrecision = 4;
constexpr int kCsvPrecision = 12;

const char* const kDefaultAnomalyThreshold = "0.5";
const char* const kDefaultCovarianceEstimator = "sample";

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

/// Параметры командной строки: имя без "--" → значение.
using Options = std::map<std::string, std::string>;

/// Годовые оценки доходности и риска бумаг.
struct AnnualStatistics {
    std::vector<std::string> tickers;
    std::string first_date;
    std::string last_date;
    Eigen::Index observation_count = 0;
    Eigen::VectorXd mean_returns;
    Eigen::MatrixXd covariance;
};

Options ParseOptions(const std::vector<std::string>& arguments) {
    Options options;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& key = arguments[index];
        if (!key.starts_with("--")) {
            throw std::invalid_argument("unexpected argument '" + key + "'");
        }
        if (key == "--help") {
            options["help"] = "";
            continue;
        }
        if (index + 1 >= arguments.size()) {
            throw std::invalid_argument("missing value for " + key);
        }
        ++index;
        options[key.substr(2)] = arguments[index];
    }
    return options;
}

std::string GetRequiredOption(const Options& options, const std::string& name) {
    const auto found = options.find(name);
    if (found == options.end()) {
        throw std::invalid_argument("--" + name + " is required");
    }
    return found->second;
}

std::string GetOptionOrDefault(const Options& options, const std::string& name,
                               const std::string& default_value) {
    const auto found = options.find(name);
    return found == options.end() ? default_value : found->second;
}

double ParseDoubleOption(const std::string& text, const std::string& name) {
    std::size_t parsed_length = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &parsed_length);
    } catch (const std::exception&) {
        parsed_length = 0;
    }
    if (text.empty() || parsed_length != text.size()) {
        throw std::invalid_argument("--" + name + " must be a number, got '" + text + "'");
    }
    return value;
}

quant::PriceTable LoadPrices(const Options& options) {
    quant::PriceTable table = quant::ReadPriceCsv(GetRequiredOption(options, "prices"));
    const auto splits_path = options.find("splits");
    if (splits_path != options.end()) {
        table = quant::ApplySplits(std::move(table), quant::ReadSplitsCsv(splits_path->second));
    }
    return quant::FillMissingPrices(std::move(table));
}

void PrintAnomalyWarnings(const quant::PriceTable& table, double threshold) {
    for (const auto& anomaly : quant::FindPriceAnomalies(table, threshold)) {
        std::cerr << "warning: " << anomaly.date << ' ' << anomaly.ticker << " daily return "
                  << std::fixed << std::setprecision(1) << anomaly.daily_return * kPercent
                  << "% (unadjusted split or bad data?)\n";
    }
}

AnnualStatistics ComputeAnnualStatistics(const quant::ReturnMatrix& daily,
                                         const quant::CovarianceEstimator& estimator) {
    AnnualStatistics statistics;
    statistics.tickers = daily.tickers;
    statistics.first_date = daily.dates.front();
    statistics.last_date = daily.dates.back();
    statistics.observation_count = daily.returns.rows();
    statistics.mean_returns = quant::ComputeMeanReturns(daily.returns) * quant::kTradingDaysPerYear;
    statistics.covariance = estimator.Estimate(daily.returns) * quant::kTradingDaysPerYear;
    return statistics;
}

void PrintStatistics(const AnnualStatistics& statistics) {
    std::cout << std::left << std::setw(kTickerColumnWidth) << "ticker" << std::right
              << std::setw(kNumberColumnWidth) << "mean, %/y" << std::setw(kNumberColumnWidth)
              << "vol, %/y" << '\n'
              << std::fixed << std::setprecision(kPercentPrecision);
    for (std::size_t asset = 0; asset < statistics.tickers.size(); ++asset) {
        const auto index = static_cast<Eigen::Index>(asset);
        const double volatility = std::sqrt(statistics.covariance(index, index));
        std::cout << std::left << std::setw(kTickerColumnWidth) << statistics.tickers[asset]
                  << std::right << std::setw(kNumberColumnWidth)
                  << statistics.mean_returns(index) * kPercent << std::setw(kNumberColumnWidth)
                  << volatility * kPercent << '\n';
    }
}

void WriteMatrixCsv(const std::string& path, const std::vector<std::string>& tickers,
                    const Eigen::MatrixXd& matrix) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write file: " + path);
    }
    output << std::setprecision(kCsvPrecision) << "ticker";
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

int RunStats(const Options& options) {
    const quant::PriceTable table = LoadPrices(options);
    const double threshold = ParseDoubleOption(
        GetOptionOrDefault(options, "anomaly-threshold", kDefaultAnomalyThreshold),
        "anomaly-threshold");
    PrintAnomalyWarnings(table, threshold);

    const auto estimator = quant::MakeCovarianceEstimator(
        GetOptionOrDefault(options, "cov", kDefaultCovarianceEstimator));
    const quant::ReturnMatrix daily = quant::ComputeSimpleReturns(table);
    const AnnualStatistics statistics = ComputeAnnualStatistics(daily, *estimator);

    std::cout << "Period: " << statistics.first_date << " .. " << statistics.last_date << " ("
              << statistics.observation_count << " daily returns, " << statistics.tickers.size()
              << " assets)\n"
              << "Covariance: " << estimator->Name();
    if (estimator->Name() == "ledoit-wolf") {
        const double shrinkage = quant::EstimateLedoitWolf(daily.returns).shrinkage;
        std::cout << ", shrinkage " << std::fixed << std::setprecision(kShrinkagePrecision)
                  << shrinkage;
    }
    std::cout << "\n\n";
    PrintStatistics(statistics);

    const auto output_path = options.find("out");
    if (output_path != options.end()) {
        WriteMatrixCsv(output_path->second, statistics.tickers, statistics.covariance);
        std::cout << "\nCovariance matrix written to " << output_path->second << '\n';
    }
    return kExitOk;
}

int RunCommand(const std::string& command, const Options& options) {
    if (options.contains("help")) {
        std::cout << kUsage;
        return kExitOk;
    }
    if (command == "stats") {
        return RunStats(options);
    }
    if (command == "optimize" || command == "frontier" || command == "backtest") {
        std::cerr << "error: command '" << command << "' is not implemented yet\n";
        return kExitBadInput;
    }
    std::cerr << "error: unknown command '" << command << "'\n\n" << kUsage;
    return kExitBadInput;
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty()) {
        std::cout << kUsage;
        return kExitBadInput;
    }
    if (arguments.front() == "--help" || arguments.front() == "help") {
        std::cout << kUsage;
        return kExitOk;
    }

    int exit_code = kExitOk;
    try {
        const std::vector<std::string> option_arguments(arguments.begin() + 1, arguments.end());
        exit_code = RunCommand(arguments.front(), ParseOptions(option_arguments));
    } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        exit_code = kExitBadInput;
    } catch (const std::runtime_error& error) {
        std::cerr << "error: " << error.what() << '\n';
        exit_code = kExitBadInput;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        exit_code = kExitCalculationError;
    }
    return exit_code;
}

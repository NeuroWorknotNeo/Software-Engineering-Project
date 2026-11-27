/**
 * @file main.cpp
 * @brief Консольная утилита mpt: портфельный анализ по Марковицу.
 *
 * Команды `stats`, `optimize` и `frontier`; `backtest` в разработке
 * (см. docs/REQUIREMENTS.md).
 */
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "quant/data/price_table.hpp"
#include "quant/io/cli_options.hpp"
#include "quant/io/result_writer.hpp"
#include "quant/optim/frontier.hpp"
#include "quant/optim/optimizer.hpp"
#include "quant/optim/portfolio.hpp"
#include "quant/optim/qp_solver.hpp"
#include "quant/stats/covariance.hpp"
#include "quant/stats/returns.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBadInput = 1;
constexpr int kExitCalculationError = 2;

constexpr double kPercent = 100.0;
constexpr int kTickerColumnWidth = 8;
constexpr int kNameColumnWidth = 14;
constexpr int kNumberColumnWidth = 12;
constexpr int kPercentPrecision = 2;
constexpr int kRatioPrecision = 4;

constexpr double kDefaultAnomalyThreshold = 0.5;
constexpr double kDefaultRiskFreeRate = 0.10;
constexpr int kDefaultFrontierPoints = 50;
const char* const kDefaultCovarianceEstimator = "sample";

const char* const kUsage = R"(Usage: mpt <command> [options]

Commands:
  stats      Annual mean returns, volatilities and covariance of assets
  optimize   Minimum-variance and max-Sharpe portfolios, with and without short selling
  frontier   Efficient frontier, with and without short selling
  backtest   Rolling backtest (in development)

Common options:
  --prices PATH             CSV with prices: date,TICKER1,TICKER2,... (required)
  --splits PATH             CSV with splits: date,ticker,ratio
  --cov sample|ledoit-wolf  Covariance estimator (default: sample)
  --anomaly-threshold X     Warn about daily |returns| above X (default: 0.5)
  --help                    Show this help

Options for 'optimize' and 'frontier':
  --risk-free R             Annual risk-free rate (default: 0.10)
  --points N                Number of frontier points, 'frontier' only (default: 50)

Output:
  --out PATH                'stats': annual covariance matrix; 'optimize', 'frontier':
                            portfolios with weights (CSV)
)";

/// Годовая модель рынка и сведения о данных, по которым она построена.
struct MarketData {
    std::string first_date;
    std::string last_date;
    Eigen::Index observation_count = 0;
    std::string estimator_name;
    double shrinkage = std::numeric_limits<double>::quiet_NaN();
    quant::MarketModel model;
};

/// Результаты одной команды оптимизации: найденные портфели и причины, по которым
/// какой-то портфель не существует.
struct OptimizationResults {
    std::vector<quant::LabeledPortfolio> portfolios;
    std::vector<std::string> notes;
};

quant::PriceTable LoadPrices(const quant::CommandLineOptions& options) {
    quant::PriceTable table = quant::ReadPriceCsv(quant::GetRequiredOption(options, "prices"));
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

MarketData LoadMarketData(const quant::CommandLineOptions& options) {
    const quant::PriceTable table = LoadPrices(options);
    PrintAnomalyWarnings(
        table, quant::GetDoubleOption(options, "anomaly-threshold", kDefaultAnomalyThreshold));

    const quant::ReturnMatrix daily = quant::ComputeSimpleReturns(table);
    const auto estimator = quant::MakeCovarianceEstimator(
        quant::GetOptionOrDefault(options, "cov", kDefaultCovarianceEstimator));

    MarketData data;
    data.first_date = daily.dates.front();
    data.last_date = daily.dates.back();
    data.observation_count = daily.returns.rows();
    data.estimator_name = estimator->Name();
    if (data.estimator_name == "ledoit-wolf") {
        data.shrinkage = quant::EstimateLedoitWolf(daily.returns).shrinkage;
    }
    data.model.tickers = daily.tickers;
    data.model.mean_returns = quant::ComputeMeanReturns(daily.returns) * quant::kTradingDaysPerYear;
    data.model.covariance = estimator->Estimate(daily.returns) * quant::kTradingDaysPerYear;
    return data;
}

void PrintDataSummary(const MarketData& data) {
    std::cout << "Period: " << data.first_date << " .. " << data.last_date << " ("
              << data.observation_count << " daily returns, " << data.model.tickers.size()
              << " assets)\n"
              << "Covariance: " << data.estimator_name;
    if (!std::isnan(data.shrinkage)) {
        std::cout << ", shrinkage " << std::fixed << std::setprecision(kRatioPrecision)
                  << data.shrinkage;
    }
    std::cout << '\n';
}

void PrintRiskFreeRate(double risk_free_rate) {
    std::cout << "Risk-free rate: " << std::fixed << std::setprecision(kPercentPrecision)
              << risk_free_rate * kPercent << "%\n";
}

void WriteOutputIfRequested(const quant::CommandLineOptions& options, const MarketData& data,
                            const std::vector<quant::LabeledPortfolio>& portfolios,
                            double risk_free_rate) {
    const auto output_path = options.find("out");
    if (output_path != options.end()) {
        quant::WritePortfoliosCsv(output_path->second, data.model.tickers, portfolios,
                                  risk_free_rate);
        std::cout << "\nPortfolios written to " << output_path->second << '\n';
    }
}

int RunStats(const quant::CommandLineOptions& options) {
    const MarketData data = LoadMarketData(options);
    PrintDataSummary(data);

    std::cout << '\n'
              << std::left << std::setw(kTickerColumnWidth) << "ticker" << std::right
              << std::setw(kNumberColumnWidth) << "mean, %/y" << std::setw(kNumberColumnWidth)
              << "vol, %/y" << '\n'
              << std::fixed << std::setprecision(kPercentPrecision);
    for (std::size_t asset = 0; asset < data.model.tickers.size(); ++asset) {
        const auto index = static_cast<Eigen::Index>(asset);
        const double volatility = std::sqrt(data.model.covariance(index, index));
        std::cout << std::left << std::setw(kTickerColumnWidth) << data.model.tickers[asset]
                  << std::right << std::setw(kNumberColumnWidth)
                  << data.model.mean_returns(index) * kPercent << std::setw(kNumberColumnWidth)
                  << volatility * kPercent << '\n';
    }

    const auto output_path = options.find("out");
    if (output_path != options.end()) {
        quant::WriteMatrixCsv(output_path->second, data.model.tickers, data.model.covariance);
        std::cout << "\nCovariance matrix written to " << output_path->second << '\n';
    }
    return kExitOk;
}

void AddOptimalPortfolios(const MarketData& data, const quant::PortfolioOptimizer& optimizer,
                          double risk_free_rate, OptimizationResults* results) {
    results->portfolios.push_back(
        {optimizer.Name(), "min-variance",
         quant::EvaluatePortfolio(data.model, optimizer.FindMinimumVariance(data.model))});
    try {
        results->portfolios.push_back(
            {optimizer.Name(), "max-sharpe",
             quant::EvaluatePortfolio(data.model,
                                      optimizer.FindMaxSharpe(data.model, risk_free_rate))});
    } catch (const std::domain_error& error) {
        results->notes.emplace_back(error.what());
    }
}

void PrintPortfolioSummary(const std::vector<quant::LabeledPortfolio>& portfolios,
                           double risk_free_rate) {
    std::cout << '\n'
              << std::left << std::setw(kNameColumnWidth) << "portfolio"
              << std::setw(kNameColumnWidth) << "mode" << std::right
              << std::setw(kNumberColumnWidth) << "return, %" << std::setw(kNumberColumnWidth)
              << "vol, %" << std::setw(kNumberColumnWidth) << "Sharpe" << '\n';
    for (const auto& labeled : portfolios) {
        const auto& portfolio = labeled.portfolio;
        std::cout << std::left << std::setw(kNameColumnWidth) << labeled.name
                  << std::setw(kNameColumnWidth) << labeled.mode << std::right << std::fixed
                  << std::setprecision(kPercentPrecision) << std::setw(kNumberColumnWidth)
                  << portfolio.expected_return * kPercent << std::setw(kNumberColumnWidth)
                  << portfolio.volatility * kPercent << std::setprecision(kRatioPrecision)
                  << std::setw(kNumberColumnWidth)
                  << quant::ComputeSharpeRatio(portfolio, risk_free_rate) << '\n';
    }
}

// Заголовок столбца весов: "minvar/S", "maxsharpe/L" и т. п.
std::string MakeWeightsColumnHeader(const quant::LabeledPortfolio& labeled) {
    std::string header = labeled.name == "min-variance" ? "minvar" : "maxsharpe";
    header += labeled.mode == "short" ? "/S" : "/L";
    return header;
}

void PrintWeights(const std::vector<std::string>& tickers,
                  const std::vector<quant::LabeledPortfolio>& portfolios) {
    std::cout << "\nWeights, %:\n" << std::left << std::setw(kTickerColumnWidth) << "ticker";
    for (const auto& labeled : portfolios) {
        std::cout << std::right << std::setw(kNumberColumnWidth)
                  << MakeWeightsColumnHeader(labeled);
    }
    std::cout << '\n' << std::fixed << std::setprecision(kPercentPrecision);
    for (std::size_t asset = 0; asset < tickers.size(); ++asset) {
        std::cout << std::left << std::setw(kTickerColumnWidth) << tickers[asset] << std::right;
        for (const auto& labeled : portfolios) {
            const double weight = labeled.portfolio.weights(static_cast<Eigen::Index>(asset));
            std::cout << std::setw(kNumberColumnWidth) << weight * kPercent;
        }
        std::cout << '\n';
    }
    std::cout << "(S — short selling allowed, L — long only)\n";
}

int RunOptimize(const quant::CommandLineOptions& options) {
    const MarketData data = LoadMarketData(options);
    const double risk_free_rate =
        quant::GetDoubleOption(options, "risk-free", kDefaultRiskFreeRate);
    PrintDataSummary(data);
    PrintRiskFreeRate(risk_free_rate);

    const quant::ActiveSetSolver solver;
    OptimizationResults results;
    AddOptimalPortfolios(data, quant::ShortSellingOptimizer{}, risk_free_rate, &results);
    AddOptimalPortfolios(data, quant::LongOnlyOptimizer(solver), risk_free_rate, &results);

    PrintPortfolioSummary(results.portfolios, risk_free_rate);
    for (const auto& note : results.notes) {
        std::cout << "note: " << note << '\n';
    }
    PrintWeights(data.model.tickers, results.portfolios);
    WriteOutputIfRequested(options, data, results.portfolios, risk_free_rate);
    return kExitOk;
}

std::vector<quant::LabeledPortfolio> LabelFrontier(const std::vector<quant::Portfolio>& frontier,
                                                   const std::string& mode) {
    std::vector<quant::LabeledPortfolio> labeled;
    labeled.reserve(frontier.size());
    for (std::size_t point = 0; point < frontier.size(); ++point) {
        labeled.push_back({mode, std::to_string(point), frontier[point]});
    }
    return labeled;
}

void PrintFrontierTable(const std::vector<quant::Portfolio>& short_frontier,
                        const std::vector<quant::Portfolio>& long_only_frontier,
                        double risk_free_rate) {
    std::cout << "\n" << std::setw(kTickerColumnWidth) << "point";
    for (const char* mode : {"S", "L"}) {
        const std::string suffix = std::string("/") + mode;
        std::cout << std::setw(kNumberColumnWidth) << "return" + suffix
                  << std::setw(kNumberColumnWidth) << "vol" + suffix
                  << std::setw(kNumberColumnWidth) << "Sharpe" + suffix;
    }
    std::cout << '\n';
    for (std::size_t point = 0; point < short_frontier.size(); ++point) {
        std::cout << std::setw(kTickerColumnWidth) << point;
        for (const auto* frontier : {&short_frontier, &long_only_frontier}) {
            const auto& portfolio = (*frontier)[point];
            std::cout << std::fixed << std::setprecision(kPercentPrecision)
                      << std::setw(kNumberColumnWidth) << portfolio.expected_return * kPercent
                      << std::setw(kNumberColumnWidth) << portfolio.volatility * kPercent
                      << std::setprecision(kRatioPrecision) << std::setw(kNumberColumnWidth)
                      << quant::ComputeSharpeRatio(portfolio, risk_free_rate);
        }
        std::cout << '\n';
    }
    std::cout << "(returns and volatilities in % per year; S — short selling allowed, "
                 "L — long only)\n";
}

int RunFrontier(const quant::CommandLineOptions& options) {
    const MarketData data = LoadMarketData(options);
    const double risk_free_rate =
        quant::GetDoubleOption(options, "risk-free", kDefaultRiskFreeRate);
    const int point_count = quant::GetIntOption(options, "points", kDefaultFrontierPoints);
    PrintDataSummary(data);
    PrintRiskFreeRate(risk_free_rate);

    const quant::ActiveSetSolver solver;
    const auto short_frontier =
        quant::BuildEfficientFrontier(data.model, quant::ShortSellingOptimizer{}, point_count);
    const auto long_only_frontier =
        quant::BuildEfficientFrontier(data.model, quant::LongOnlyOptimizer(solver), point_count);
    PrintFrontierTable(short_frontier, long_only_frontier, risk_free_rate);

    auto portfolios = LabelFrontier(short_frontier, "short");
    const auto long_only_labeled = LabelFrontier(long_only_frontier, "long-only");
    portfolios.insert(portfolios.end(), long_only_labeled.begin(), long_only_labeled.end());
    WriteOutputIfRequested(options, data, portfolios, risk_free_rate);
    return kExitOk;
}

int RunCommand(const std::string& command, const quant::CommandLineOptions& options) {
    if (options.contains("help")) {
        std::cout << kUsage;
        return kExitOk;
    }
    if (command == "stats") {
        return RunStats(options);
    }
    if (command == "optimize") {
        return RunOptimize(options);
    }
    if (command == "frontier") {
        return RunFrontier(options);
    }
    if (command == "backtest") {
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
        exit_code = RunCommand(arguments.front(), quant::ParseCommandLineOptions(option_arguments));
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

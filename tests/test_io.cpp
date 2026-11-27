// Тесты модуля io: FR-20 (параметры командной строки), FR-21 (запись CSV).
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "quant/io/cli_options.hpp"
#include "quant/io/result_writer.hpp"

TEST(CommandLineOptions, ParsesNamedValuesAndHelp) {
    const auto options =
        quant::ParseCommandLineOptions({"--prices", "p.csv", "--points", "20", "--help"});
    EXPECT_EQ(quant::GetRequiredOption(options, "prices"), "p.csv");
    EXPECT_EQ(quant::GetIntOption(options, "points", 50), 20);
    EXPECT_EQ(quant::GetIntOption(options, "missing", 50), 50);
    EXPECT_DOUBLE_EQ(quant::GetDoubleOption(options, "risk-free", 0.1), 0.1);
    EXPECT_EQ(quant::GetOptionOrDefault(options, "cov", "sample"), "sample");
    EXPECT_TRUE(options.contains("help"));
}

TEST(CommandLineOptions, RejectsMalformedArguments) {
    EXPECT_THROW(quant::ParseCommandLineOptions({"prices"}), std::invalid_argument);
    EXPECT_THROW(quant::ParseCommandLineOptions({"--prices"}), std::invalid_argument);
    const auto options = quant::ParseCommandLineOptions({"--risk-free", "ten", "--points", "2.5"});
    EXPECT_THROW(quant::GetDoubleOption(options, "risk-free", 0.1), std::invalid_argument);
    EXPECT_THROW(quant::GetIntOption(options, "points", 50), std::invalid_argument);
    EXPECT_THROW(quant::GetRequiredOption(options, "prices"), std::invalid_argument);
}

TEST(ResultWriter, WritesPortfoliosCsv) {
    const auto path = std::filesystem::temp_directory_path() / "mpt_test_portfolios.csv";
    quant::Portfolio portfolio;
    portfolio.weights = Eigen::Vector2d(0.25, 0.75);
    portfolio.expected_return = 0.12;
    portfolio.volatility = 0.2;
    quant::WritePortfoliosCsv(path.string(), {"AAA", "BBB"},
                              {{"long-only", "max-sharpe", portfolio}}, 0.02);

    std::ifstream input(path);
    std::stringstream content;
    content << input.rdbuf();
    EXPECT_EQ(content.str(),
              "mode,name,expected_return,volatility,sharpe,AAA,BBB\n"
              "long-only,max-sharpe,0.12,0.2,0.5,0.25,0.75\n");
    std::filesystem::remove(path);
}

TEST(ResultWriter, FailsOnUnwritablePath) {
    EXPECT_THROW(quant::WriteMatrixCsv("/nonexistent-dir/matrix.csv", {"AAA"},
                                       Eigen::MatrixXd::Identity(1, 1)),
                 std::runtime_error);
}

// Тесты модуля stats: FR-6 (доходности, средние, выборочная ковариация),
// FR-7 (Ledoit–Wolf), FR-8 (выбор метода).
#include <gtest/gtest.h>

#include <stdexcept>

#include "quant/data/price_table.hpp"
#include "quant/stats/covariance.hpp"
#include "quant/stats/returns.hpp"

namespace {

// 8 наблюдений × 3 бумаги; эталоны посчитаны numpy.cov и sklearn.covariance.ledoit_wolf.
Eigen::MatrixXd MakeReferenceReturns() {
    Eigen::MatrixXd returns(8, 3);
    returns << 0.010, -0.020, 0.005,  //
        0.003, 0.011, -0.004,         //
        -0.012, 0.007, 0.002,         //
        0.020, -0.005, 0.013,         //
        -0.004, 0.016, -0.009,        //
        0.006, -0.010, 0.001,         //
        0.015, 0.002, 0.007,          //
        -0.008, -0.013, -0.006;
    return returns;
}

void ExpectMatrixNear(const Eigen::MatrixXd& actual, const Eigen::MatrixXd& expected,
                      double tolerance) {
    ASSERT_EQ(actual.rows(), expected.rows());
    ASSERT_EQ(actual.cols(), expected.cols());
    for (Eigen::Index row = 0; row < actual.rows(); ++row) {
        for (Eigen::Index column = 0; column < actual.cols(); ++column) {
            EXPECT_NEAR(actual(row, column), expected(row, column), tolerance)
                << "at (" << row << ", " << column << ")";
        }
    }
}

}  // namespace

TEST(Returns, SimpleReturnsFromPrices) {
    quant::PriceTable table;
    table.dates = {"2024-01-03", "2024-01-04", "2024-01-05"};
    table.tickers = {"AAA", "BBB"};
    table.prices.resize(3, 2);
    table.prices << 100, 50, 110, 50, 99, 55;

    const auto daily = quant::ComputeSimpleReturns(table);
    EXPECT_EQ(daily.dates, (std::vector<std::string>{"2024-01-04", "2024-01-05"}));
    EXPECT_EQ(daily.tickers, table.tickers);
    EXPECT_NEAR(daily.returns(0, 0), 0.10, 1e-15);
    EXPECT_NEAR(daily.returns(1, 0), -0.10, 1e-15);
    EXPECT_NEAR(daily.returns(0, 1), 0.0, 1e-15);
    EXPECT_NEAR(daily.returns(1, 1), 0.10, 1e-15);
}

TEST(Returns, RejectsGapsAndShortTables) {
    quant::PriceTable table;
    table.dates = {"2024-01-03", "2024-01-04"};
    table.tickers = {"AAA"};
    table.prices.resize(2, 1);
    table.prices << 100, std::nan("");
    EXPECT_THROW(quant::ComputeSimpleReturns(table), std::invalid_argument);

    table.dates.pop_back();
    table.prices.conservativeResize(1, 1);
    table.prices(0, 0) = 100;
    EXPECT_THROW(quant::ComputeSimpleReturns(table), std::invalid_argument);
}

TEST(Returns, MeanReturns) {
    const Eigen::VectorXd mean_returns = quant::ComputeMeanReturns(MakeReferenceReturns());
    EXPECT_NEAR(mean_returns(0), 0.00375, 1e-15);
    EXPECT_NEAR(mean_returns(1), -0.00150, 1e-15);
    EXPECT_NEAR(mean_returns(2), 0.00112500, 1e-15);
    EXPECT_THROW(quant::ComputeMeanReturns(Eigen::MatrixXd(0, 3)), std::invalid_argument);
}

TEST(Covariance, SampleMatchesNumpy) {
    Eigen::MatrixXd expected(3, 3);
    expected << 0.0001259285714285714, -4.228571428571428e-05, 6.217857142857143e-05,
        -4.228571428571428e-05, 0.00015800000000000002, -3.478571428571428e-05,
        6.217857142857143e-05, -3.478571428571428e-05, 5.298214285714285e-05;
    ExpectMatrixNear(quant::SampleCovariance{}.Estimate(MakeReferenceReturns()), expected, 1e-15);
}

TEST(Covariance, LedoitWolfMatchesSklearn) {
    Eigen::MatrixXd expected(3, 3);
    expected << 0.00010426184084218651, -1.8609487698948494e-05, 2.7364120003268018e-05,
        -1.8609487698948494e-05, 0.00011837613134358497, -1.530881842802013e-05,
        2.7364120003268018e-05, -1.530881842802013e-05, 7.21589028142285e-05;
    const auto ledoit_wolf = quant::EstimateLedoitWolf(MakeReferenceReturns());
    EXPECT_NEAR(ledoit_wolf.shrinkage, 0.49704087300139194, 1e-12);
    ExpectMatrixNear(ledoit_wolf.covariance, expected, 1e-15);
}

TEST(Covariance, LedoitWolfIsWellConditionedWhenAssetsExceedObservations) {
    // 5 наблюдений × 10 бумаг: выборочная ковариация вырождена, Ledoit–Wolf — нет.
    const Eigen::MatrixXd returns = Eigen::MatrixXd::Random(5, 10) * 0.01;
    const Eigen::MatrixXd sample = quant::SampleCovariance{}.Estimate(returns);
    const Eigen::MatrixXd shrunk = quant::LedoitWolfCovariance{}.Estimate(returns);

    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> sample_eigen(sample);
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> shrunk_eigen(shrunk);
    EXPECT_LT(sample_eigen.eigenvalues().minCoeff(), 1e-15);
    EXPECT_GT(shrunk_eigen.eigenvalues().minCoeff(), 1e-8);
    EXPECT_TRUE(shrunk.isApprox(shrunk.transpose()));
}

TEST(Covariance, RejectsTooFewObservations) {
    EXPECT_THROW(quant::SampleCovariance{}.Estimate(Eigen::MatrixXd::Ones(1, 3)),
                 std::invalid_argument);
    EXPECT_THROW(quant::EstimateLedoitWolf(Eigen::MatrixXd::Ones(1, 3)), std::invalid_argument);
}

TEST(Covariance, FactoryByName) {
    EXPECT_EQ(quant::MakeCovarianceEstimator("sample")->Name(), "sample");
    EXPECT_EQ(quant::MakeCovarianceEstimator("ledoit-wolf")->Name(), "ledoit-wolf");
    EXPECT_EQ(quant::MakeCovarianceEstimator("lw")->Name(), "ledoit-wolf");
    EXPECT_THROW(quant::MakeCovarianceEstimator("magic"), std::invalid_argument);
}

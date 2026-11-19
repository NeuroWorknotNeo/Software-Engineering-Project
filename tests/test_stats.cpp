// Тесты модуля stats: FR-6 (доходности, средние, выборочная ковариация),
// FR-7 (Ledoit–Wolf), FR-8 (выбор метода).
#include <gtest/gtest.h>

#include <stdexcept>

#include "quant/data/price_table.hpp"
#include "quant/stats/covariance.hpp"
#include "quant/stats/returns.hpp"

namespace {

// 8 наблюдений × 3 бумаги; эталоны посчитаны numpy.cov и sklearn.covariance.ledoit_wolf.
Eigen::MatrixXd sampleReturns() {
    Eigen::MatrixXd x(8, 3);
    x << 0.010, -0.020, 0.005,  //
        0.003, 0.011, -0.004,   //
        -0.012, 0.007, 0.002,   //
        0.020, -0.005, 0.013,   //
        -0.004, 0.016, -0.009,  //
        0.006, -0.010, 0.001,   //
        0.015, 0.002, 0.007,    //
        -0.008, -0.013, -0.006;
    return x;
}

void expectMatrixNear(const Eigen::MatrixXd& actual, const Eigen::MatrixXd& expected,
                      double tol) {
    ASSERT_EQ(actual.rows(), expected.rows());
    ASSERT_EQ(actual.cols(), expected.cols());
    for (Eigen::Index i = 0; i < actual.rows(); ++i) {
        for (Eigen::Index j = 0; j < actual.cols(); ++j) {
            EXPECT_NEAR(actual(i, j), expected(i, j), tol) << "at (" << i << ", " << j << ")";
        }
    }
}

}  // namespace

TEST(Returns, SimpleReturnsFromPrices) {
    quant::PriceTable t;
    t.dates = {"2024-01-03", "2024-01-04", "2024-01-05"};
    t.tickers = {"AAA", "BBB"};
    t.prices.resize(3, 2);
    t.prices << 100, 50, 110, 50, 99, 55;

    const auto r = quant::simpleReturns(t);
    EXPECT_EQ(r.dates, (std::vector<std::string>{"2024-01-04", "2024-01-05"}));
    EXPECT_EQ(r.tickers, t.tickers);
    EXPECT_NEAR(r.returns(0, 0), 0.10, 1e-15);
    EXPECT_NEAR(r.returns(1, 0), -0.10, 1e-15);
    EXPECT_NEAR(r.returns(0, 1), 0.0, 1e-15);
    EXPECT_NEAR(r.returns(1, 1), 0.10, 1e-15);
}

TEST(Returns, RejectsGapsAndShortTables) {
    quant::PriceTable t;
    t.dates = {"2024-01-03", "2024-01-04"};
    t.tickers = {"AAA"};
    t.prices.resize(2, 1);
    t.prices << 100, std::nan("");
    EXPECT_THROW(quant::simpleReturns(t), std::invalid_argument);

    t.dates.pop_back();
    t.prices.conservativeResize(1, 1);
    t.prices(0, 0) = 100;
    EXPECT_THROW(quant::simpleReturns(t), std::invalid_argument);
}

TEST(Returns, MeanReturns) {
    const Eigen::VectorXd mu = quant::meanReturns(sampleReturns());
    EXPECT_NEAR(mu(0), 0.00375, 1e-15);
    EXPECT_NEAR(mu(1), -0.00150, 1e-15);
    EXPECT_NEAR(mu(2), 0.00112500, 1e-15);
    EXPECT_THROW(quant::meanReturns(Eigen::MatrixXd(0, 3)), std::invalid_argument);
}

TEST(Covariance, SampleMatchesNumpy) {
    Eigen::MatrixXd expected(3, 3);
    expected << 0.0001259285714285714, -4.228571428571428e-05, 6.217857142857143e-05,
        -4.228571428571428e-05, 0.00015800000000000002, -3.478571428571428e-05,
        6.217857142857143e-05, -3.478571428571428e-05, 5.298214285714285e-05;
    expectMatrixNear(quant::SampleCovariance{}.estimate(sampleReturns()), expected, 1e-15);
}

TEST(Covariance, LedoitWolfMatchesSklearn) {
    Eigen::MatrixXd expected(3, 3);
    expected << 0.00010426184084218651, -1.8609487698948494e-05, 2.7364120003268018e-05,
        -1.8609487698948494e-05, 0.00011837613134358497, -1.530881842802013e-05,
        2.7364120003268018e-05, -1.530881842802013e-05, 7.21589028142285e-05;
    const auto lw = quant::ledoitWolf(sampleReturns());
    EXPECT_NEAR(lw.shrinkage, 0.49704087300139194, 1e-12);
    expectMatrixNear(lw.covariance, expected, 1e-15);
}

TEST(Covariance, LedoitWolfIsWellConditionedWhenAssetsExceedObservations) {
    // 5 наблюдений × 10 бумаг: выборочная ковариация вырождена, Ledoit–Wolf — нет.
    const Eigen::MatrixXd x = Eigen::MatrixXd::Random(5, 10) * 0.01;
    const Eigen::MatrixXd sample = quant::SampleCovariance{}.estimate(x);
    const Eigen::MatrixXd lw = quant::LedoitWolfCovariance{}.estimate(x);

    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> sampleEig(sample);
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> lwEig(lw);
    EXPECT_LT(sampleEig.eigenvalues().minCoeff(), 1e-15);
    EXPECT_GT(lwEig.eigenvalues().minCoeff(), 1e-8);
    EXPECT_TRUE(lw.isApprox(lw.transpose()));
}

TEST(Covariance, RejectsTooFewObservations) {
    EXPECT_THROW(quant::SampleCovariance{}.estimate(Eigen::MatrixXd::Ones(1, 3)),
                 std::invalid_argument);
    EXPECT_THROW(quant::ledoitWolf(Eigen::MatrixXd::Ones(1, 3)), std::invalid_argument);
}

TEST(Covariance, FactoryByName) {
    EXPECT_EQ(quant::makeCovarianceEstimator("sample")->name(), "sample");
    EXPECT_EQ(quant::makeCovarianceEstimator("ledoit-wolf")->name(), "ledoit-wolf");
    EXPECT_EQ(quant::makeCovarianceEstimator("lw")->name(), "ledoit-wolf");
    EXPECT_THROW(quant::makeCovarianceEstimator("magic"), std::invalid_argument);
}

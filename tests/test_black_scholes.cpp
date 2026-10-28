#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "quant/black_scholes.hpp"

using quant::OptionParams;

TEST(NormalCdfTest, KnownValues) {
    EXPECT_DOUBLE_EQ(quant::normalCdf(0.0), 0.5);
    EXPECT_NEAR(quant::normalCdf(1.96), 0.9750021, 1e-7);
    EXPECT_NEAR(quant::normalCdf(-1.0), 0.1586553, 1e-7);
}

// Пример из Hull, "Options, Futures, and Other Derivatives": c ≈ 4.76, p ≈ 0.81.
TEST(BlackScholesTest, HullTextbookExample) {
    const OptionParams p{42.0, 40.0, 0.10, 0.20, 0.5};
    EXPECT_NEAR(quant::callPrice(p), 4.7594, 1e-4);
    EXPECT_NEAR(quant::putPrice(p), 0.8086, 1e-4);
}

TEST(BlackScholesTest, AtTheMoneyExample) {
    const OptionParams p{100.0, 100.0, 0.05, 0.20, 1.0};
    EXPECT_NEAR(quant::callPrice(p), 10.4506, 1e-4);
    EXPECT_NEAR(quant::putPrice(p), 5.5735, 1e-4);
}

TEST(BlackScholesTest, PutCallParity) {
    const OptionParams p{120.0, 95.0, 0.03, 0.35, 2.5};
    const double lhs = quant::callPrice(p) - quant::putPrice(p);
    const double rhs = p.spot - p.strike * std::exp(-p.rate * p.maturity);
    EXPECT_NEAR(lhs, rhs, 1e-10);
}

TEST(BlackScholesTest, CallIncreasesWithVolatility) {
    OptionParams low{100.0, 110.0, 0.02, 0.10, 1.0};
    OptionParams high = low;
    high.volatility = 0.40;
    EXPECT_LT(quant::callPrice(low), quant::callPrice(high));
    EXPECT_LT(quant::putPrice(low), quant::putPrice(high));
}

TEST(BlackScholesTest, InvalidParamsThrow) {
    EXPECT_THROW(quant::callPrice({0.0, 100.0, 0.05, 0.2, 1.0}), std::invalid_argument);
    EXPECT_THROW(quant::callPrice({100.0, -1.0, 0.05, 0.2, 1.0}), std::invalid_argument);
    EXPECT_THROW(quant::putPrice({100.0, 100.0, 0.05, 0.0, 1.0}), std::invalid_argument);
    EXPECT_THROW(quant::putPrice({100.0, 100.0, 0.05, 0.2, 0.0}), std::invalid_argument);
}

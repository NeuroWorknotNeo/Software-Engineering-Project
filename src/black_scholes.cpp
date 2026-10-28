/**
 * @file black_scholes.cpp
 * @brief Реализация формулы Блэка–Шоулза.
 */
#include "quant/black_scholes.hpp"

#include <cmath>
#include <stdexcept>

namespace quant {

namespace {

struct D1D2 {
    double d1;
    double d2;
};

void validate(const OptionParams& p) {
    if (!(p.spot > 0.0) || !(p.strike > 0.0) || !(p.volatility > 0.0) || !(p.maturity > 0.0)) {
        throw std::invalid_argument("spot, strike, volatility and maturity must be positive");
    }
}

D1D2 computeD1D2(const OptionParams& p) {
    const double sigmaSqrtT = p.volatility * std::sqrt(p.maturity);
    const double d1 =
        (std::log(p.spot / p.strike) + (p.rate + 0.5 * p.volatility * p.volatility) * p.maturity) /
        sigmaSqrtT;
    return {d1, d1 - sigmaSqrtT};
}

}  // namespace

double normalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

double callPrice(const OptionParams& p) {
    validate(p);
    const auto [d1, d2] = computeD1D2(p);
    return p.spot * normalCdf(d1) - p.strike * std::exp(-p.rate * p.maturity) * normalCdf(d2);
}

double putPrice(const OptionParams& p) {
    validate(p);
    const auto [d1, d2] = computeD1D2(p);
    return p.strike * std::exp(-p.rate * p.maturity) * normalCdf(-d2) - p.spot * normalCdf(-d1);
}

}  // namespace quant

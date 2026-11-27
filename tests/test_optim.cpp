// Тесты модуля optim: FR-9…FR-11 (аналитические решения с короткими продажами),
// FR-12…FR-14 (без коротких продаж), NFR-2 (точность решателей).
#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>

#include "quant/optim/frontier.hpp"
#include "quant/optim/optimizer.hpp"
#include "quant/optim/portfolio.hpp"
#include "quant/optim/qp_solver.hpp"

namespace {

constexpr double kTolerance = 1e-8;
constexpr double kRiskFreeRate = 0.03;

// Три бумаги, у которых портфель минимальной дисперсии с короткими продажами
// уже имеет положительные веса: ограничение w ≥ 0 не активно.
quant::MarketModel MakeInteriorModel() {
    quant::MarketModel model;
    model.tickers = {"AAA", "BBB", "CCC"};
    model.mean_returns.resize(3);
    model.mean_returns << 0.08, 0.12, 0.15;
    model.covariance.resize(3, 3);
    model.covariance << 0.040, 0.006, 0.004,  //
        0.006, 0.090, 0.010,                  //
        0.004, 0.010, 0.160;
    return model;
}

// Четыре бумаги: DDD сильно коррелирует с AAA и хуже неё, поэтому без ограничений
// портфель минимальной дисперсии держит DDD в короткой позиции.
quant::MarketModel MakeShortingModel() {
    quant::MarketModel model;
    model.tickers = {"AAA", "BBB", "CCC", "DDD"};
    model.mean_returns.resize(4);
    model.mean_returns << 0.10, 0.07, 0.14, 0.06;
    model.covariance.resize(4, 4);
    model.covariance << 0.040, 0.010, 0.012, 0.046,  //
        0.010, 0.050, 0.008, 0.012,                  //
        0.012, 0.008, 0.120, 0.014,                  //
        0.046, 0.012, 0.014, 0.060;
    return model;
}

// Случайная модель: Σ = GGᵀ/k + 0.01·I гарантированно положительно определена.
quant::MarketModel MakeRandomModel(int asset_count, unsigned seed) {
    std::mt19937 generator(seed);
    std::normal_distribution<double> normal(0.0, 0.2);
    std::uniform_real_distribution<double> uniform_return(0.0, 0.25);
    const int factor_count = 2 * asset_count;
    Eigen::MatrixXd loadings(asset_count, factor_count);
    for (int row = 0; row < asset_count; ++row) {
        for (int column = 0; column < factor_count; ++column) {
            loadings(row, column) = normal(generator);
        }
    }
    quant::MarketModel model;
    model.covariance = loadings * loadings.transpose() / factor_count +
                       0.01 * Eigen::MatrixXd::Identity(asset_count, asset_count);
    model.mean_returns.resize(asset_count);
    for (int asset = 0; asset < asset_count; ++asset) {
        model.mean_returns(asset) = uniform_return(generator);
    }
    return model;
}

// Условия ККТ для min ½wᵀΣw при 1ᵀw = 1, w ≥ 0: (Σw)_i = λ для w_i > 0, (Σw)_i ≥ λ для w_i = 0.
void ExpectLongOnlyMinimumVarianceOptimal(const quant::MarketModel& model,
                                          const Eigen::VectorXd& weights) {
    EXPECT_NEAR(weights.sum(), 1.0, kTolerance);
    EXPECT_GE(weights.minCoeff(), 0.0);
    const Eigen::VectorXd marginal_risk = model.covariance * weights;
    const double multiplier = weights.dot(marginal_risk);
    for (Eigen::Index asset = 0; asset < weights.size(); ++asset) {
        if (weights(asset) > kTolerance) {
            EXPECT_NEAR(marginal_risk(asset), multiplier, kTolerance) << "asset " << asset;
        } else {
            EXPECT_GE(marginal_risk(asset), multiplier - kTolerance) << "asset " << asset;
        }
    }
}

double SharpeOf(const quant::MarketModel& model, const Eigen::VectorXd& weights) {
    return quant::ComputeSharpeRatio(quant::EvaluatePortfolio(model, weights), kRiskFreeRate);
}

}  // namespace

TEST(Portfolio, EvaluatesReturnAndVolatility) {
    const auto model = MakeInteriorModel();
    const Eigen::Vector3d weights(0.5, 0.5, 0.0);
    const auto portfolio = quant::EvaluatePortfolio(model, weights);
    EXPECT_NEAR(portfolio.expected_return, 0.10, 1e-15);
    EXPECT_NEAR(portfolio.volatility, std::sqrt(0.25 * 0.04 + 0.25 * 0.09 + 0.5 * 0.006), 1e-15);
    EXPECT_NEAR(quant::ComputeSharpeRatio(portfolio, 0.02), 0.08 / portfolio.volatility, 1e-15);
    EXPECT_THROW(quant::EvaluatePortfolio(model, Eigen::Vector2d(0.5, 0.5)), std::invalid_argument);
}

TEST(ShortSelling, TwoAssetMinimumVarianceMatchesFormula) {
    // Некоррелированные бумаги: w₁ = σ₂² / (σ₁² + σ₂²).
    quant::MarketModel model;
    model.mean_returns = Eigen::Vector2d(0.05, 0.10);
    model.covariance = Eigen::Vector2d(0.04, 0.09).asDiagonal();
    const auto weights = quant::ShortSellingOptimizer{}.FindMinimumVariance(model);
    EXPECT_NEAR(weights(0), 0.09 / 0.13, 1e-14);
    EXPECT_NEAR(weights(1), 0.04 / 0.13, 1e-14);
}

TEST(ShortSelling, MinimumVarianceSatisfiesOptimalityConditions) {
    const auto model = MakeShortingModel();
    const auto weights = quant::ShortSellingOptimizer{}.FindMinimumVariance(model);
    EXPECT_NEAR(weights.sum(), 1.0, 1e-14);
    EXPECT_LT(weights.minCoeff(), 0.0);
    // Σw ∝ 1: все предельные вклады в риск равны.
    const Eigen::VectorXd marginal_risk = model.covariance * weights;
    EXPECT_NEAR(marginal_risk.maxCoeff() - marginal_risk.minCoeff(), 0.0, 1e-14);
}

TEST(ShortSelling, MaxSharpeSatisfiesOptimalityConditions) {
    const auto model = MakeShortingModel();
    const auto weights = quant::ShortSellingOptimizer{}.FindMaxSharpe(model, kRiskFreeRate);
    EXPECT_NEAR(weights.sum(), 1.0, 1e-14);
    // Σw ∝ μ − r_f·1.
    const Eigen::VectorXd marginal_risk = model.covariance * weights;
    const Eigen::VectorXd excess = model.mean_returns.array() - kRiskFreeRate;
    const Eigen::VectorXd ratio = marginal_risk.cwiseQuotient(excess);
    EXPECT_NEAR(ratio.maxCoeff() - ratio.minCoeff(), 0.0, 1e-12);
    // Шарп выше, чем у любого портфеля границы.
    const double best_sharpe = SharpeOf(model, weights);
    for (const auto& point :
         quant::BuildEfficientFrontier(model, quant::ShortSellingOptimizer{}, 50)) {
        EXPECT_LE(quant::ComputeSharpeRatio(point, kRiskFreeRate), best_sharpe + 1e-12);
    }
}

TEST(ShortSelling, MaxSharpeDoesNotExistWhenRiskFreeRateIsTooHigh) {
    const auto model = MakeShortingModel();
    const auto minimum_variance =
        quant::EvaluatePortfolio(model, quant::ShortSellingOptimizer{}.FindMinimumVariance(model));
    EXPECT_THROW(quant::ShortSellingOptimizer{}.FindMaxSharpe(
                     model, minimum_variance.expected_return + 0.01),
                 std::domain_error);
}

TEST(ShortSelling, FrontierPointHasTargetReturnAndMinimalVariance) {
    const auto model = MakeShortingModel();
    const quant::ShortSellingOptimizer optimizer;
    const auto weights = optimizer.FindMinimumVarianceForReturn(model, 0.11);
    EXPECT_NEAR(weights.sum(), 1.0, 1e-13);
    EXPECT_NEAR(model.mean_returns.dot(weights), 0.11, 1e-13);
    // Любое допустимое отклонение d (1ᵀd = 0, μᵀd = 0) не уменьшает дисперсию.
    const Eigen::Vector4d deviation(1.0, -4.0, 0.0, 3.0);
    ASSERT_NEAR(model.mean_returns.dot(deviation), 0.0, 1e-15);
    const double variance = weights.dot(model.covariance * weights);
    for (const double scale : {-0.01, 0.01}) {
        const Eigen::VectorXd moved = weights + scale * deviation;
        EXPECT_GT(moved.dot(model.covariance * moved), variance);
    }
}

TEST(ShortSelling, RejectsNotPositiveDefiniteCovariance) {
    quant::MarketModel model;
    model.mean_returns = Eigen::Vector2d(0.05, 0.10);
    model.covariance.resize(2, 2);
    model.covariance << 0.04, 0.06, 0.06, 0.09;  // корреляция 1 — матрица вырождена
    EXPECT_THROW(quant::ShortSellingOptimizer{}.FindMinimumVariance(model), std::invalid_argument);
}

TEST(SimplexProjection, ProjectsOntoSimplex) {
    const Eigen::Vector3d inside(0.2, 0.3, 0.5);
    EXPECT_TRUE(quant::ProjectOntoSimplex(inside, 1.0).isApprox(inside));
    const auto projected = quant::ProjectOntoSimplex(Eigen::Vector3d(2.0, 0.0, -1.0), 1.0);
    EXPECT_TRUE(projected.isApprox(Eigen::Vector3d(1.0, 0.0, 0.0)));
    const auto shifted = quant::ProjectOntoSimplex(Eigen::Vector3d(0.5, 0.5, 0.5), 1.0);
    EXPECT_NEAR(shifted.sum(), 1.0, 1e-15);
    EXPECT_NEAR(shifted(0), 1.0 / 3.0, 1e-15);
    EXPECT_THROW(quant::ProjectOntoSimplex(inside, 0.0), std::invalid_argument);
}

TEST(LongOnly, MatchesAnalyticSolutionWhenConstraintIsInactive) {
    const auto model = MakeInteriorModel();
    const auto analytic = quant::ShortSellingOptimizer{}.FindMinimumVariance(model);
    ASSERT_GT(analytic.minCoeff(), 0.0);

    const quant::ActiveSetSolver active_set;
    const quant::ProjectedGradientSolver projected_gradient;
    for (const quant::QpSolver* solver :
         std::vector<const quant::QpSolver*>{&active_set, &projected_gradient}) {
        const auto weights = quant::LongOnlyOptimizer(*solver).FindMinimumVariance(model);
        EXPECT_NEAR((weights - analytic).lpNorm<Eigen::Infinity>(), 0.0, kTolerance)
            << solver->Name();
    }
}

TEST(LongOnly, MinimumVarianceDropsShortedAsset) {
    const auto model = MakeShortingModel();
    const quant::ActiveSetSolver solver;
    const auto weights = quant::LongOnlyOptimizer(solver).FindMinimumVariance(model);
    ExpectLongOnlyMinimumVarianceOptimal(model, weights);
    EXPECT_EQ(weights(3), 0.0);
}

TEST(LongOnly, ActiveSetAgreesWithProjectedGradientOnRandomModels) {
    const quant::ActiveSetSolver active_set;
    const quant::ProjectedGradientSolver projected_gradient;
    for (unsigned seed = 1; seed <= 20; ++seed) {
        const auto model = MakeRandomModel(15, seed);
        const auto exact = quant::LongOnlyOptimizer(active_set).FindMinimumVariance(model);
        const auto reference =
            quant::LongOnlyOptimizer(projected_gradient).FindMinimumVariance(model);
        ExpectLongOnlyMinimumVarianceOptimal(model, exact);
        EXPECT_NEAR((exact - reference).lpNorm<Eigen::Infinity>(), 0.0, 1e-7) << "seed " << seed;
    }
}

TEST(LongOnly, MeanVarianceTradeOffAgreesWithReturnTarget) {
    // Задача min ½wᵀΣw − τ·μᵀw на симплексе даёт точку той же границы, что и
    // min ½wᵀΣw при μᵀw = r для r = μᵀw_τ: сверяем два решателя на двух формулировках.
    const quant::ActiveSetSolver active_set;
    const quant::ProjectedGradientSolver projected_gradient;
    for (unsigned seed = 1; seed <= 10; ++seed) {
        const auto model = MakeRandomModel(12, seed);
        const auto asset_count = model.mean_returns.size();
        for (const double risk_tolerance : {0.05, 0.2, 0.5}) {
            quant::QpProblem problem;
            problem.quadratic = model.covariance;
            problem.linear = -risk_tolerance * model.mean_returns;
            problem.equality_matrix = Eigen::MatrixXd::Ones(1, asset_count);
            problem.equality_rhs = Eigen::VectorXd::Ones(1);
            problem.start = Eigen::VectorXd::Constant(asset_count, 1.0 / asset_count);
            const auto trade_off = projected_gradient.Solve(problem).solution;
            EXPECT_NEAR((active_set.Solve(problem).solution - trade_off).lpNorm<Eigen::Infinity>(),
                        0.0, 1e-7);

            const double target_return = model.mean_returns.dot(trade_off);
            const auto targeted = quant::LongOnlyOptimizer(active_set)
                                      .FindMinimumVarianceForReturn(model, target_return);
            EXPECT_NEAR((targeted - trade_off).lpNorm<Eigen::Infinity>(), 0.0, 1e-6)
                << "seed " << seed << ", tau " << risk_tolerance;
        }
    }
}

TEST(LongOnly, FrontierPointsAreFeasibleAndRiskier) {
    const auto model = MakeShortingModel();
    const quant::ActiveSetSolver solver;
    const quant::LongOnlyOptimizer long_only(solver);
    const auto frontier = quant::BuildEfficientFrontier(model, long_only, 20);
    const auto unconstrained =
        quant::BuildEfficientFrontier(model, quant::ShortSellingOptimizer{}, 20);
    ASSERT_EQ(frontier.size(), 20U);
    EXPECT_NEAR(frontier.back().expected_return, model.mean_returns.maxCoeff(), 1e-12);
    for (std::size_t point = 0; point < frontier.size(); ++point) {
        EXPECT_GE(frontier[point].weights.minCoeff(), 0.0);
        EXPECT_NEAR(frontier[point].weights.sum(), 1.0, kTolerance);
        if (point > 0) {
            EXPECT_GT(frontier[point].expected_return, frontier[point - 1].expected_return);
            EXPECT_GE(frontier[point].volatility, frontier[point - 1].volatility - kTolerance);
        }
        // Запрет коротких продаж не может уменьшить риск при той же доходности.
        const auto shorting_allowed = quant::EvaluatePortfolio(
            model, quant::ShortSellingOptimizer{}.FindMinimumVarianceForReturn(
                       model, frontier[point].expected_return));
        EXPECT_GE(frontier[point].volatility, shorting_allowed.volatility - kTolerance);
    }
    EXPECT_LE(unconstrained.front().volatility, frontier.front().volatility);
}

TEST(LongOnly, MaxSharpeBeatsFrontierAndIsBelowShortSelling) {
    const auto model = MakeShortingModel();
    const quant::ActiveSetSolver solver;
    const quant::LongOnlyOptimizer long_only(solver);
    const auto weights = long_only.FindMaxSharpe(model, kRiskFreeRate);
    EXPECT_NEAR(weights.sum(), 1.0, kTolerance);
    EXPECT_GE(weights.minCoeff(), 0.0);
    const double best_sharpe = SharpeOf(model, weights);
    for (const auto& point : quant::BuildEfficientFrontier(model, long_only, 200)) {
        EXPECT_LE(quant::ComputeSharpeRatio(point, kRiskFreeRate), best_sharpe + 1e-9);
    }
    const auto unconstrained = quant::ShortSellingOptimizer{}.FindMaxSharpe(model, kRiskFreeRate);
    EXPECT_LE(best_sharpe, SharpeOf(model, unconstrained) + 1e-12);
}

TEST(LongOnly, RejectsUnreachableTargetsAndRates) {
    const auto model = MakeInteriorModel();
    const quant::ActiveSetSolver solver;
    const quant::LongOnlyOptimizer long_only(solver);
    EXPECT_THROW(long_only.FindMinimumVarianceForReturn(model, 0.20), std::invalid_argument);
    EXPECT_THROW(long_only.FindMinimumVarianceForReturn(model, 0.05), std::invalid_argument);
    EXPECT_THROW(long_only.FindMaxSharpe(model, 0.16), std::domain_error);
}

TEST(QpSolvers, RejectInvalidProblems) {
    const auto model = MakeInteriorModel();
    quant::QpProblem problem;
    problem.quadratic = model.covariance;
    problem.linear = Eigen::VectorXd::Zero(3);
    problem.equality_matrix = Eigen::MatrixXd::Ones(1, 3);
    problem.equality_rhs = Eigen::VectorXd::Ones(1);
    problem.start = Eigen::Vector3d(0.5, 0.5, 0.5);
    EXPECT_THROW(quant::ActiveSetSolver{}.Solve(problem), std::invalid_argument);

    problem.equality_matrix = model.mean_returns.transpose();
    problem.equality_rhs = Eigen::VectorXd::Constant(1, 0.1);
    problem.start = Eigen::Vector3d(0.5, 0.5, 0.0);
    EXPECT_NO_THROW(quant::ActiveSetSolver{}.Solve(problem));
    EXPECT_THROW(quant::ProjectedGradientSolver{}.Solve(problem), std::invalid_argument);
}

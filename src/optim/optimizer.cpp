/**
 * @file optimizer.cpp
 * @brief Реализация оптимизаторов портфеля.
 */
#include "quant/optim/optimizer.hpp"

#include <sstream>
#include <stdexcept>

namespace quant {

namespace {

// Относительная точность, ниже которой величины считаются равными нулю.
constexpr double kRelativeTolerance = 1e-12;

// Допуск при сравнении целевой доходности с доходностями бумаг.
constexpr double kReturnTolerance = 1e-12;

constexpr double kPercent = 100.0;

Eigen::LLT<Eigen::MatrixXd> FactorizeCovariance(const MarketModel& model) {
    ValidateMarketModel(model);
    Eigen::LLT<Eigen::MatrixXd> factorization(model.covariance);
    if (factorization.info() != Eigen::Success) {
        throw std::invalid_argument(
            "covariance matrix is not positive definite; try the Ledoit-Wolf estimator");
    }
    return factorization;
}

std::string FormatPercent(double value) {
    std::ostringstream stream;
    stream.precision(2);
    stream << std::fixed << value * kPercent << '%';
    return stream.str();
}

QpProblem MakeQpProblem(const MarketModel& model, Eigen::MatrixXd equality_matrix,
                        Eigen::VectorXd equality_rhs, Eigen::VectorXd start) {
    QpProblem problem;
    problem.quadratic = model.covariance;
    problem.linear = Eigen::VectorXd::Zero(model.mean_returns.size());
    problem.equality_matrix = std::move(equality_matrix);
    problem.equality_rhs = std::move(equality_rhs);
    problem.start = std::move(start);
    return problem;
}

Eigen::VectorXd MakeUnitVector(Eigen::Index size, Eigen::Index index, double value) {
    Eigen::VectorXd vector = Eigen::VectorXd::Zero(size);
    vector(index) = value;
    return vector;
}

}  // namespace

Eigen::VectorXd ShortSellingOptimizer::FindMinimumVariance(const MarketModel& model) const {
    const auto factorization = FactorizeCovariance(model);
    const Eigen::VectorXd direction =
        factorization.solve(Eigen::VectorXd::Ones(model.mean_returns.size()));
    return direction / direction.sum();
}

Eigen::VectorXd ShortSellingOptimizer::FindMinimumVarianceForReturn(const MarketModel& model,
                                                                    double target_return) const {
    const auto factorization = FactorizeCovariance(model);
    const Eigen::VectorXd inverse_times_ones =
        factorization.solve(Eigen::VectorXd::Ones(model.mean_returns.size()));
    const Eigen::VectorXd inverse_times_means = factorization.solve(model.mean_returns);

    const double coefficient_a = inverse_times_ones.sum();
    const double coefficient_b = inverse_times_means.sum();
    const double coefficient_c = model.mean_returns.dot(inverse_times_means);
    const double determinant = coefficient_a * coefficient_c - coefficient_b * coefficient_b;
    if (!(determinant > kRelativeTolerance * coefficient_a * coefficient_c)) {
        throw std::invalid_argument(
            "expected returns of all assets are equal; a target return cannot be set");
    }

    const double ones_multiplier = (coefficient_c - coefficient_b * target_return) / determinant;
    const double means_multiplier = (coefficient_a * target_return - coefficient_b) / determinant;
    return ones_multiplier * inverse_times_ones + means_multiplier * inverse_times_means;
}

Eigen::VectorXd ShortSellingOptimizer::FindMaxSharpe(const MarketModel& model,
                                                     double risk_free_rate) const {
    const auto factorization = FactorizeCovariance(model);
    const auto asset_count = model.mean_returns.size();
    const Eigen::VectorXd excess_returns =
        model.mean_returns - risk_free_rate * Eigen::VectorXd::Ones(asset_count);
    const Eigen::VectorXd direction = factorization.solve(excess_returns);

    // Касательный портфель существует, только если доходность GMV выше безрисковой ставки,
    // что равносильно 1ᵀΣ⁻¹(μ − r_f·1) > 0.
    const double budget = direction.sum();
    if (!(budget > kRelativeTolerance * direction.cwiseAbs().sum())) {
        const Eigen::VectorXd minimum_variance = FindMinimumVariance(model);
        throw std::domain_error(
            "max-Sharpe portfolio with short selling does not exist: return of the "
            "minimum-variance portfolio (" +
            FormatPercent(model.mean_returns.dot(minimum_variance)) +
            ") does not exceed the risk-free rate (" + FormatPercent(risk_free_rate) + ")");
    }
    return direction / budget;
}

std::string ShortSellingOptimizer::Name() const {
    return "short";
}

LongOnlyOptimizer::LongOnlyOptimizer(const QpSolver& solver) : solver_(solver) {}

Eigen::VectorXd LongOnlyOptimizer::FindMinimumVariance(const MarketModel& model) const {
    ValidateMarketModel(model);
    const auto asset_count = model.mean_returns.size();
    const Eigen::VectorXd equal_weights =
        Eigen::VectorXd::Constant(asset_count, 1.0 / static_cast<double>(asset_count));
    const QpProblem problem = MakeQpProblem(model, Eigen::MatrixXd::Ones(1, asset_count),
                                            Eigen::VectorXd::Ones(1), equal_weights);
    return solver_.Solve(problem).solution;
}

Eigen::VectorXd LongOnlyOptimizer::FindMinimumVarianceForReturn(const MarketModel& model,
                                                                double target_return) const {
    ValidateMarketModel(model);
    const auto asset_count = model.mean_returns.size();
    Eigen::Index lowest_asset = 0;
    Eigen::Index highest_asset = 0;
    const double lowest_return = model.mean_returns.minCoeff(&lowest_asset);
    const double highest_return = model.mean_returns.maxCoeff(&highest_asset);

    if (target_return < lowest_return - kReturnTolerance ||
        target_return > highest_return + kReturnTolerance) {
        throw std::invalid_argument("target return " + FormatPercent(target_return) +
                                    " is not reachable without short selling: asset returns "
                                    "range from " +
                                    FormatPercent(lowest_return) + " to " +
                                    FormatPercent(highest_return));
    }
    if (highest_return - lowest_return <= kReturnTolerance) {
        return FindMinimumVariance(model);
    }
    if (target_return >= highest_return - kReturnTolerance) {
        return MakeUnitVector(asset_count, highest_asset, 1.0);
    }

    // Допустимая начальная точка: смесь бумаг с наименьшей и наибольшей доходностью.
    const double highest_share = (target_return - lowest_return) / (highest_return - lowest_return);
    Eigen::VectorXd start = MakeUnitVector(asset_count, highest_asset, highest_share);
    start(lowest_asset) += 1.0 - highest_share;

    Eigen::MatrixXd equality_matrix(2, asset_count);
    equality_matrix.row(0).setOnes();
    equality_matrix.row(1) = model.mean_returns.transpose();
    const Eigen::Vector2d equality_rhs(1.0, target_return);
    return solver_.Solve(MakeQpProblem(model, equality_matrix, equality_rhs, start)).solution;
}

Eigen::VectorXd LongOnlyOptimizer::FindMaxSharpe(const MarketModel& model,
                                                 double risk_free_rate) const {
    ValidateMarketModel(model);
    const auto asset_count = model.mean_returns.size();
    const Eigen::VectorXd excess_returns =
        model.mean_returns - risk_free_rate * Eigen::VectorXd::Ones(asset_count);
    Eigen::Index best_asset = 0;
    const double best_excess_return = excess_returns.maxCoeff(&best_asset);
    if (!(best_excess_return > 0.0)) {
        throw std::domain_error(
            "max-Sharpe portfolio without short selling does not exist: no asset has an "
            "expected return above the risk-free rate (" +
            FormatPercent(risk_free_rate) + ")");
    }

    const Eigen::VectorXd start = MakeUnitVector(asset_count, best_asset, 1.0 / best_excess_return);
    const QpProblem problem =
        MakeQpProblem(model, excess_returns.transpose(), Eigen::VectorXd::Ones(1), start);
    const Eigen::VectorXd scaled_weights = solver_.Solve(problem).solution;
    return scaled_weights / scaled_weights.sum();
}

std::string LongOnlyOptimizer::Name() const {
    return "long-only";
}

}  // namespace quant

/**
 * @file qp_solver.cpp
 * @brief Реализация решателей задачи квадратичного программирования.
 */
#include "quant/optim/qp_solver.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <vector>

namespace quant {

namespace {

// Шаг метода активного множества меньше этого значения считается нулевым.
constexpr double kZeroStepTolerance = 1e-13;

// Множитель Лагранжа больше −kMultiplierTolerance считается неотрицательным.
constexpr double kMultiplierTolerance = 1e-12;

// Допустимая погрешность начальной точки: |Ax − b| и отрицательные координаты.
constexpr double kFeasibilityTolerance = 1e-9;

// Предел итераций метода активного множества: kIterationsPerAsset · N + kExtraIterations.
constexpr int kIterationsPerAsset = 10;
constexpr int kExtraIterations = 100;

void ValidateProblem(const QpProblem& problem) {
    const auto size = problem.quadratic.rows();
    if (size == 0 || problem.quadratic.cols() != size || problem.linear.size() != size ||
        problem.start.size() != size || problem.equality_matrix.cols() != size ||
        problem.equality_matrix.rows() != problem.equality_rhs.size()) {
        throw std::invalid_argument("QP problem has inconsistent dimensions");
    }
    const double scale = 1.0 + problem.equality_rhs.cwiseAbs().maxCoeff();
    const double residual =
        (problem.equality_matrix * problem.start - problem.equality_rhs).cwiseAbs().maxCoeff();
    if (problem.start.minCoeff() < -kFeasibilityTolerance ||
        residual > kFeasibilityTolerance * scale) {
        throw std::invalid_argument("QP start point is not feasible");
    }
}

std::vector<Eigen::Index> CollectFreeIndices(const std::vector<bool>& at_bound) {
    std::vector<Eigen::Index> free_indices;
    for (std::size_t index = 0; index < at_bound.size(); ++index) {
        if (!at_bound[index]) {
            free_indices.push_back(static_cast<Eigen::Index>(index));
        }
    }
    return free_indices;
}

/// Решение задачи с равенствами на свободных координатах: шаг p и множители y.
struct EqualityStep {
    Eigen::VectorXd step;
    Eigen::VectorXd multipliers;
};

// Решает систему ККТ [Q_FF A_Fᵀ; A_F 0]·[p_F; y] = [−g_F; 0] для свободных координат F.
EqualityStep SolveEqualityStep(const QpProblem& problem, const Eigen::VectorXd& gradient,
                               const std::vector<Eigen::Index>& free_indices) {
    const auto free_count = static_cast<Eigen::Index>(free_indices.size());
    const auto constraint_count = problem.equality_matrix.rows();
    const auto all_rows = Eigen::all;

    Eigen::MatrixXd kkt =
        Eigen::MatrixXd::Zero(free_count + constraint_count, free_count + constraint_count);
    kkt.topLeftCorner(free_count, free_count) = problem.quadratic(free_indices, free_indices);
    kkt.topRightCorner(free_count, constraint_count) =
        problem.equality_matrix(all_rows, free_indices).transpose();
    kkt.bottomLeftCorner(constraint_count, free_count) =
        problem.equality_matrix(all_rows, free_indices);

    Eigen::VectorXd rhs = Eigen::VectorXd::Zero(free_count + constraint_count);
    rhs.head(free_count) = -gradient(free_indices);

    // Разложение с выбором ранга: при линейно зависимых ограничениях система остаётся
    // совместной, а множители определены неоднозначно — берётся решение с минимальной нормой.
    const Eigen::VectorXd solution = kkt.completeOrthogonalDecomposition().solve(rhs);

    EqualityStep result;
    result.step = Eigen::VectorXd::Zero(problem.quadratic.rows());
    result.step(free_indices) = solution.head(free_count);
    result.multipliers = solution.tail(constraint_count);
    return result;
}

}  // namespace

QpResult ActiveSetSolver::Solve(const QpProblem& problem) const {
    ValidateProblem(problem);
    const auto size = problem.quadratic.rows();

    Eigen::VectorXd point = problem.start.cwiseMax(0.0);
    std::vector<bool> at_bound(static_cast<std::size_t>(size));
    for (Eigen::Index index = 0; index < size; ++index) {
        at_bound[static_cast<std::size_t>(index)] = point(index) <= 0.0;
    }

    const int max_iterations = kIterationsPerAsset * static_cast<int>(size) + kExtraIterations;
    for (int iteration = 1; iteration <= max_iterations; ++iteration) {
        const Eigen::VectorXd gradient = problem.quadratic * point + problem.linear;
        const auto free_indices = CollectFreeIndices(at_bound);
        const EqualityStep equality_step = SolveEqualityStep(problem, gradient, free_indices);
        const Eigen::VectorXd& step = equality_step.step;

        if (step.lpNorm<Eigen::Infinity>() <= kZeroStepTolerance) {
            // Точка оптимальна на текущем активном множестве. Множители границ x_i ≥ 0:
            // ν = g + Aᵀy; если все ν_i ≥ 0 — выполнены условия ККТ, иначе освобождаем
            // координату с наименьшим множителем.
            const Eigen::VectorXd bound_multipliers =
                gradient + problem.equality_matrix.transpose() * equality_step.multipliers;
            Eigen::Index released = -1;
            double most_negative = -kMultiplierTolerance;
            for (Eigen::Index index = 0; index < size; ++index) {
                if (at_bound[static_cast<std::size_t>(index)] &&
                    bound_multipliers(index) < most_negative) {
                    most_negative = bound_multipliers(index);
                    released = index;
                }
            }
            if (released < 0) {
                return {point, iteration};
            }
            at_bound[static_cast<std::size_t>(released)] = false;
            continue;
        }

        // Самый длинный шаг вдоль p, при котором координаты остаются неотрицательными.
        double step_length = 1.0;
        Eigen::Index blocking = -1;
        for (const auto index : free_indices) {
            if (step(index) < 0.0) {
                const double ratio = -point(index) / step(index);
                if (ratio < step_length) {
                    step_length = ratio;
                    blocking = index;
                }
            }
        }
        point += step_length * step;
        if (blocking >= 0) {
            point(blocking) = 0.0;
            at_bound[static_cast<std::size_t>(blocking)] = true;
        }
    }
    throw std::runtime_error("active-set solver did not converge");
}

std::string ActiveSetSolver::Name() const {
    return "active-set";
}

ProjectedGradientSolver::ProjectedGradientSolver(int max_iterations, double tolerance)
    : max_iterations_(max_iterations), tolerance_(tolerance) {}

QpResult ProjectedGradientSolver::Solve(const QpProblem& problem) const {
    ValidateProblem(problem);
    const auto size = problem.quadratic.rows();
    const bool is_budget_constraint =
        problem.equality_matrix.rows() == 1 &&
        (problem.equality_matrix.row(0).transpose() - Eigen::VectorXd::Ones(size))
                .cwiseAbs()
                .maxCoeff() == 0.0;
    if (!is_budget_constraint) {
        throw std::invalid_argument(
            "projected gradient supports only the single constraint sum(x) = b");
    }
    const double total = problem.equality_rhs(0);

    const Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eigen_solver(problem.quadratic,
                                                                      Eigen::EigenvaluesOnly);
    const double lipschitz_constant = eigen_solver.eigenvalues().maxCoeff();
    if (!(lipschitz_constant > 0.0)) {
        throw std::invalid_argument("QP quadratic matrix must be positive definite");
    }
    const double step_size = 1.0 / lipschitz_constant;

    Eigen::VectorXd point = ProjectOntoSimplex(problem.start, total);
    for (int iteration = 1; iteration <= max_iterations_; ++iteration) {
        const Eigen::VectorXd gradient = problem.quadratic * point + problem.linear;
        const Eigen::VectorXd next = ProjectOntoSimplex(point - step_size * gradient, total);
        const double change = (next - point).lpNorm<Eigen::Infinity>();
        point = next;
        if (change <= tolerance_) {
            return {point, iteration};
        }
    }
    throw std::runtime_error("projected gradient did not converge");
}

std::string ProjectedGradientSolver::Name() const {
    return "projected-gradient";
}

Eigen::VectorXd ProjectOntoSimplex(const Eigen::VectorXd& point, double total) {
    if (point.size() == 0 || !(total > 0.0)) {
        throw std::invalid_argument("simplex projection needs a non-empty point and total > 0");
    }
    std::vector<double> sorted(point.data(), point.data() + point.size());
    std::sort(sorted.begin(), sorted.end(), std::greater<>());

    // Порог θ: x_i = max(v_i − θ, 0), где θ подбирается так, чтобы сумма x равнялась total.
    double prefix_sum = 0.0;
    double threshold = 0.0;
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        prefix_sum += sorted[index];
        const double candidate = (prefix_sum - total) / static_cast<double>(index + 1);
        if (sorted[index] > candidate) {
            threshold = candidate;
        }
    }
    return (point.array() - threshold).cwiseMax(0.0).matrix();
}

}  // namespace quant

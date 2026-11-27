/**
 * @file qp_solver.hpp
 * @brief Решатели задачи квадратичного программирования с ограничениями x ≥ 0.
 */
#pragma once

#include <Eigen/Dense>
#include <string>

namespace quant {

/**
 * @brief Задача min ½·xᵀQx + cᵀx при Ax = b, x ≥ 0.
 *
 * Q — симметричная положительно определённая матрица N × N, A — M × N.
 * Решатели начинают с допустимой точки @ref start (Ax = b, x ≥ 0).
 */
struct QpProblem {
    /// Матрица квадратичной части Q (N × N).
    Eigen::MatrixXd quadratic;

    /// Вектор линейной части c (длина N).
    Eigen::VectorXd linear;

    /// Матрица ограничений-равенств A (M × N).
    Eigen::MatrixXd equality_matrix;

    /// Правая часть ограничений-равенств b (длина M).
    Eigen::VectorXd equality_rhs;

    /// Допустимая начальная точка (длина N).
    Eigen::VectorXd start;
};

/**
 * @brief Решение задачи квадратичного программирования.
 */
struct QpResult {
    /// Оптимальная точка x*.
    Eigen::VectorXd solution;

    /// Число выполненных итераций.
    int iterations = 0;
};

/**
 * @brief Интерфейс решателя задачи квадратичного программирования (паттерн Strategy).
 */
class QpSolver {
public:
    virtual ~QpSolver() = default;

    /**
     * @brief Решает задачу.
     * @param problem Задача с допустимой начальной точкой.
     * @return Решение.
     * @throws std::invalid_argument если задача некорректна или начальная точка недопустима.
     * @throws std::runtime_error если решатель не сошёлся за отведённое число итераций.
     */
    [[nodiscard]] virtual QpResult Solve(const QpProblem& problem) const = 0;

    /// Короткое имя решателя для вывода.
    [[nodiscard]] virtual std::string Name() const = 0;
};

/**
 * @brief Прямой метод активного множества (Nocedal, Wright, «Numerical Optimization», алг. 16.3).
 *
 * Активное множество — бумаги с нулевым весом. На каждой итерации решается задача
 * только с равенствами для остальных бумаг (система ККТ); если шаг упирается в границу x_i = 0,
 * бумага добавляется в активное множество, если множитель Лагранжа у бумаги в активном
 * множестве отрицателен — удаляется. Даёт точное решение за конечное число шагов.
 */
class ActiveSetSolver final : public QpSolver {
public:
    [[nodiscard]] QpResult Solve(const QpProblem& problem) const override;

    [[nodiscard]] std::string Name() const override;
};

/**
 * @brief Проекционный градиентный спуск — простой эталон для проверки ActiveSetSolver.
 *
 * Шаг x ← P(x − (Qx + c) / L), где L — наибольшее собственное число Q, а P — проекция
 * на допустимое множество. Проекция реализована только для симплекса
 * {x ≥ 0, x₁ + … + x_N = b}, поэтому решатель принимает задачи с единственным
 * ограничением-равенством вида 1ᵀx = b.
 */
class ProjectedGradientSolver final : public QpSolver {
public:
    /**
     * @brief Создаёт решатель.
     * @param max_iterations Максимальное число итераций.
     * @param tolerance Остановка, когда max |x_{k+1} − x_k| не больше этого значения.
     */
    explicit ProjectedGradientSolver(int max_iterations = kDefaultMaxIterations,
                                     double tolerance = kDefaultTolerance);

    [[nodiscard]] QpResult Solve(const QpProblem& problem) const override;

    [[nodiscard]] std::string Name() const override;

private:
    static constexpr int kDefaultMaxIterations = 1'000'000;
    static constexpr double kDefaultTolerance = 1e-14;

    int max_iterations_;
    double tolerance_;
};

/**
 * @brief Евклидова проекция точки на симплекс {x ≥ 0, x₁ + … + x_N = total}.
 *
 * Алгоритм Duchi et al. (2008), O(N log N).
 *
 * @param point Проецируемая точка.
 * @param total Сумма координат на симплексе (> 0).
 * @return Ближайшая точка симплекса.
 * @throws std::invalid_argument если total ≤ 0 или точка пустая.
 */
Eigen::VectorXd ProjectOntoSimplex(const Eigen::VectorXd& point, double total);

}  // namespace quant

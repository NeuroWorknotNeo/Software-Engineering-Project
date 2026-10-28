/**
 * @file black_scholes.hpp
 * @brief Оценка европейских опционов Call и Put по формуле Блэка–Шоулза.
 */
#pragma once

namespace quant {

/**
 * @brief Параметры европейского опциона.
 */
struct OptionParams {
    double spot;        ///< Текущая цена базового актива S (> 0).
    double strike;      ///< Цена исполнения K (> 0).
    double rate;        ///< Безрисковая ставка r (годовая, непрерывное начисление).
    double volatility;  ///< Волатильность σ (годовая, > 0).
    double maturity;    ///< Время до экспирации T в годах (> 0).
};

/**
 * @brief Функция распределения стандартного нормального закона N(x).
 * @param x Аргумент.
 * @return Вероятность P(Z <= x), где Z ~ N(0, 1).
 */
double normalCdf(double x);

/**
 * @brief Цена европейского опциона Call.
 *
 * C = S·N(d1) − K·e^(−rT)·N(d2), где
 * d1 = (ln(S/K) + (r + σ²/2)·T) / (σ·√T), d2 = d1 − σ·√T.
 *
 * @param p Параметры опциона.
 * @return Справедливая цена Call.
 * @throws std::invalid_argument если S, K, σ или T не положительны.
 */
double callPrice(const OptionParams& p);

/**
 * @brief Цена европейского опциона Put.
 *
 * P = K·e^(−rT)·N(−d2) − S·N(−d1).
 *
 * @param p Параметры опциона.
 * @return Справедливая цена Put.
 * @throws std::invalid_argument если S, K, σ или T не положительны.
 */
double putPrice(const OptionParams& p);

}  // namespace quant

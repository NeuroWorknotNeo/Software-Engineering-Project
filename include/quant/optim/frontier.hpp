/**
 * @file frontier.hpp
 * @brief Построение эффективной границы.
 */
#pragma once

#include <vector>

#include "quant/optim/optimizer.hpp"
#include "quant/optim/portfolio.hpp"

namespace quant {

/**
 * @brief Строит эффективную границу из @p point_count портфелей.
 *
 * Целевые доходности равномерно расставлены от доходности портфеля минимальной
 * дисперсии до наибольшей ожидаемой доходности среди бумаг; для каждой находится
 * портфель минимальной дисперсии с этой доходностью.
 *
 * @param model Модель рынка.
 * @param optimizer Оптимизатор (с короткими продажами или без).
 * @param point_count Число точек, не меньше 2.
 * @return Портфели в порядке возрастания доходности.
 * @throws std::invalid_argument если point_count < 2.
 */
std::vector<Portfolio> BuildEfficientFrontier(const MarketModel& model,
                                              const PortfolioOptimizer& optimizer, int point_count);

}  // namespace quant

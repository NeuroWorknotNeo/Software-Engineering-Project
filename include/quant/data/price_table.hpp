/**
 * @file price_table.hpp
 * @brief Таблица дневных цен закрытия: чтение из CSV, заполнение пропусков, сплиты, проверки.
 */
#pragma once

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace quant {

/**
 * @brief Дневные цены закрытия нескольких бумаг на общем календаре.
 *
 * Строка матрицы @ref prices — торговый день, столбец — бумага.
 * Пропущенная цена хранится как NaN.
 */
struct PriceTable {
    std::vector<std::string> dates;    ///< Даты в формате YYYY-MM-DD, строго по возрастанию.
    std::vector<std::string> tickers;  ///< Тикеры в порядке столбцов.
    Eigen::MatrixXd prices;            ///< Цены: dates.size() × tickers.size().
};

/**
 * @brief Сплит бумаги: с даты @p date одна старая акция превращается в @p ratio новых.
 *
 * Для обратного сплита (консолидации) ratio < 1.
 */
struct Split {
    std::string date;    ///< Первый торговый день по новым ценам, YYYY-MM-DD.
    std::string ticker;  ///< Тикер бумаги.
    double ratio;        ///< Число новых акций на одну старую (> 0).
};

/**
 * @brief Подозрительное изменение цены за один торговый день.
 */
struct PriceAnomaly {
    std::string date;     ///< Дата, на которую пришлось изменение.
    std::string ticker;   ///< Тикер бумаги.
    double dailyReturn;   ///< Простая доходность за день.
};

/**
 * @brief Читает цены из CSV в широком формате `date,TICKER1,TICKER2,...`.
 *
 * Пустая ячейка означает отсутствие цены (NaN).
 *
 * @param path Путь к файлу.
 * @return Таблица цен (пропуски не заполняются).
 * @throws std::runtime_error если файл не открывается или формат нарушен: неверное число
 *         столбцов, некорректная дата, даты не по возрастанию, цена не число или ≤ 0.
 */
PriceTable readPriceCsv(const std::string& path);

/**
 * @brief Заполняет пропуски последней известной ценой (forward fill).
 * @param table Таблица цен, изменяется на месте.
 * @throws std::runtime_error если у бумаги нет цены в первый день таблицы.
 */
void forwardFill(PriceTable& table);

/**
 * @brief Читает список сплитов из CSV `date,ticker,ratio`.
 * @param path Путь к файлу.
 * @return Список сплитов.
 * @throws std::runtime_error если файл не открывается или формат нарушен.
 */
std::vector<Split> readSplitsCsv(const std::string& path);

/**
 * @brief Пересчитывает цены до даты сплита: делит их на ratio.
 *
 * Сплиты по тикерам, которых нет в таблице, пропускаются.
 *
 * @param table Таблица цен, изменяется на месте.
 * @param splits Список сплитов.
 * @throws std::invalid_argument если ratio ≤ 0.
 */
void applySplits(PriceTable& table, const std::vector<Split>& splits);

/**
 * @brief Находит дневные изменения цены, по модулю превышающие порог.
 *
 * Типичная причина — неучтённый сплит или ошибка в данных.
 *
 * @param table Таблица цен без пропусков.
 * @param threshold Порог для |доходности|, например 0.5 (50%).
 * @return Список аномалий в порядке дат.
 */
std::vector<PriceAnomaly> findAnomalies(const PriceTable& table, double threshold);

}  // namespace quant

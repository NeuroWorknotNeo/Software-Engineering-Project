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
    /// Даты в формате YYYY-MM-DD, строго по возрастанию.
    std::vector<std::string> dates;

    /// Тикеры в порядке столбцов.
    std::vector<std::string> tickers;

    /// Цены: dates.size() × tickers.size().
    Eigen::MatrixXd prices;
};

/**
 * @brief Сплит бумаги: с даты @p date одна старая акция превращается в @p ratio новых.
 *
 * Для обратного сплита (консолидации) ratio < 1.
 */
struct Split {
    /// Первый торговый день по новым ценам, YYYY-MM-DD.
    std::string date;

    /// Тикер бумаги.
    std::string ticker;

    /// Число новых акций на одну старую (> 0).
    double ratio = 1.0;
};

/**
 * @brief Подозрительное изменение цены за один торговый день.
 */
struct PriceAnomaly {
    /// Дата, на которую пришлось изменение.
    std::string date;

    /// Тикер бумаги.
    std::string ticker;

    /// Простая доходность за день.
    double daily_return = 0.0;
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
PriceTable ReadPriceCsv(const std::string& path);

/**
 * @brief Заполняет пропуски последней известной ценой (forward fill).
 * @param table Таблица цен.
 * @return Таблица без пропусков.
 * @throws std::runtime_error если у бумаги нет цены в первый день таблицы.
 */
PriceTable FillMissingPrices(PriceTable table);

/**
 * @brief Читает список сплитов из CSV `date,ticker,ratio`.
 * @param path Путь к файлу.
 * @return Список сплитов.
 * @throws std::runtime_error если файл не открывается или формат нарушен.
 */
std::vector<Split> ReadSplitsCsv(const std::string& path);

/**
 * @brief Пересчитывает цены до даты сплита: делит их на ratio.
 *
 * Сплиты по тикерам, которых нет в таблице, пропускаются.
 *
 * @param table Таблица цен.
 * @param splits Список сплитов.
 * @return Таблица с ценами, приведёнными к последнему количеству акций.
 * @throws std::invalid_argument если ratio ≤ 0.
 */
PriceTable ApplySplits(PriceTable table, const std::vector<Split>& splits);

/**
 * @brief Находит дневные изменения цены, по модулю превышающие порог.
 *
 * Типичная причина — неучтённый сплит или ошибка в данных.
 *
 * @param table Таблица цен без пропусков.
 * @param threshold Порог для |доходности|, например 0.5 (50%).
 * @return Список аномалий в порядке дат.
 */
std::vector<PriceAnomaly> FindPriceAnomalies(const PriceTable& table, double threshold);

}  // namespace quant

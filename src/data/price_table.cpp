/**
 * @file price_table.cpp
 * @brief Реализация чтения и подготовки таблицы цен.
 */
#include "quant/data/price_table.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace quant {

namespace {

constexpr std::size_t kIsoDateLength = 10;
constexpr std::size_t kYearMonthSeparator = 4;
constexpr std::size_t kMonthDaySeparator = 7;
constexpr int kMaxMonth = 12;
constexpr int kMaxDay = 31;
constexpr int kDecimalBase = 10;

std::string_view TrimSpaces(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::vector<std::string_view> SplitCsvLine(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t field_start = 0;
    while (true) {
        const auto comma = line.find(',', field_start);
        fields.push_back(TrimSpaces(line.substr(field_start, comma - field_start)));
        if (comma == std::string_view::npos) {
            break;
        }
        field_start = comma + 1;
    }
    return fields;
}

int ParseTwoDigits(std::string_view text, std::size_t position) {
    return (text[position] - '0') * kDecimalBase + (text[position + 1] - '0');
}

bool IsIsoDate(std::string_view text) {
    if (text.size() != kIsoDateLength || text[kYearMonthSeparator] != '-' ||
        text[kMonthDaySeparator] != '-') {
        return false;
    }
    for (std::size_t position = 0; position < text.size(); ++position) {
        const bool is_separator = position == kYearMonthSeparator || position == kMonthDaySeparator;
        if (!is_separator && (text[position] < '0' || text[position] > '9')) {
            return false;
        }
    }
    const int month = ParseTwoDigits(text, kYearMonthSeparator + 1);
    const int day = ParseTwoDigits(text, kMonthDaySeparator + 1);
    return month >= 1 && month <= kMaxMonth && day >= 1 && day <= kMaxDay;
}

double ParseNumber(std::string_view text, const std::string& location) {
    double value = 0.0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size() || !std::isfinite(value)) {
        throw std::runtime_error(location + ": not a number: '" + std::string(text) + "'");
    }
    return value;
}

std::string FormatLocation(const std::string& path, std::size_t line_number) {
    return path + ":" + std::to_string(line_number);
}

std::ifstream OpenFileOrThrow(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path);
    }
    return input;
}

std::vector<std::string> ParsePriceHeader(const std::string& line, const std::string& location) {
    const auto header = SplitCsvLine(line);
    if (header.size() < 2 || header[0] != "date") {
        throw std::runtime_error(location + ": header must be 'date,TICKER1,TICKER2,...'");
    }
    std::vector<std::string> tickers;
    for (std::size_t column = 1; column < header.size(); ++column) {
        if (header[column].empty()) {
            throw std::runtime_error(location + ": empty ticker name");
        }
        tickers.emplace_back(header[column]);
    }
    return tickers;
}

double ParsePrice(std::string_view text, const std::string& location) {
    if (text.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double price = ParseNumber(text, location);
    if (price <= 0.0) {
        throw std::runtime_error(location + ": price must be positive");
    }
    return price;
}

}  // namespace

PriceTable ReadPriceCsv(const std::string& path) {
    auto input = OpenFileOrThrow(path);
    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error(path + ": empty file");
    }
    std::size_t line_number = 1;

    PriceTable table;
    table.tickers = ParsePriceHeader(line, FormatLocation(path, line_number));
    const std::size_t ticker_count = table.tickers.size();

    std::vector<double> prices_row_major;
    while (std::getline(input, line)) {
        ++line_number;
        if (TrimSpaces(line).empty()) {
            continue;
        }
        const auto fields = SplitCsvLine(line);
        const auto location = FormatLocation(path, line_number);
        if (fields.size() != ticker_count + 1) {
            throw std::runtime_error(location + ": expected " + std::to_string(ticker_count + 1) +
                                     " fields, got " + std::to_string(fields.size()));
        }
        if (!IsIsoDate(fields[0])) {
            throw std::runtime_error(location + ": bad date '" + std::string(fields[0]) +
                                     "', expected YYYY-MM-DD");
        }
        if (!table.dates.empty() && fields[0] <= table.dates.back()) {
            throw std::runtime_error(location + ": dates must be strictly increasing");
        }
        table.dates.emplace_back(fields[0]);
        for (std::size_t column = 1; column <= ticker_count; ++column) {
            prices_row_major.push_back(ParsePrice(fields[column], location));
        }
    }

    using RowMajorMatrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    table.prices = Eigen::Map<RowMajorMatrix>(prices_row_major.data(),
                                              static_cast<Eigen::Index>(table.dates.size()),
                                              static_cast<Eigen::Index>(ticker_count));
    return table;
}

PriceTable FillMissingPrices(PriceTable table) {
    auto& prices = table.prices;
    for (Eigen::Index column = 0; column < prices.cols(); ++column) {
        for (Eigen::Index row = 0; row < prices.rows(); ++row) {
            if (!std::isnan(prices(row, column))) {
                continue;
            }
            if (row == 0) {
                throw std::runtime_error("no price for " +
                                         table.tickers[static_cast<std::size_t>(column)] +
                                         " on the first date " + table.dates.front());
            }
            prices(row, column) = prices(row - 1, column);
        }
    }
    return table;
}

std::vector<Split> ReadSplitsCsv(const std::string& path) {
    auto input = OpenFileOrThrow(path);
    std::string line;
    const std::vector<std::string_view> expected_header = {"date", "ticker", "ratio"};
    if (!std::getline(input, line) || SplitCsvLine(line) != expected_header) {
        throw std::runtime_error(path + ": header must be 'date,ticker,ratio'");
    }
    std::size_t line_number = 1;

    std::vector<Split> splits;
    while (std::getline(input, line)) {
        ++line_number;
        if (TrimSpaces(line).empty()) {
            continue;
        }
        const auto fields = SplitCsvLine(line);
        const auto location = FormatLocation(path, line_number);
        if (fields.size() != expected_header.size() || !IsIsoDate(fields[0]) || fields[1].empty()) {
            throw std::runtime_error(location + ": expected 'YYYY-MM-DD,TICKER,ratio'");
        }
        splits.push_back(
            {std::string(fields[0]), std::string(fields[1]), ParseNumber(fields[2], location)});
    }
    return splits;
}

PriceTable ApplySplits(PriceTable table, const std::vector<Split>& splits) {
    for (const auto& split : splits) {
        if (!(split.ratio > 0.0)) {
            throw std::invalid_argument("split ratio must be positive: " + split.ticker + " " +
                                        split.date);
        }
        for (std::size_t column = 0; column < table.tickers.size(); ++column) {
            if (table.tickers[column] != split.ticker) {
                continue;
            }
            for (std::size_t row = 0; row < table.dates.size() && table.dates[row] < split.date;
                 ++row) {
                table.prices(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(column)) /=
                    split.ratio;
            }
        }
    }
    return table;
}

std::vector<PriceAnomaly> FindPriceAnomalies(const PriceTable& table, double threshold) {
    std::vector<PriceAnomaly> anomalies;
    const auto& prices = table.prices;
    for (Eigen::Index row = 1; row < prices.rows(); ++row) {
        for (Eigen::Index column = 0; column < prices.cols(); ++column) {
            const double daily_return = prices(row, column) / prices(row - 1, column) - 1.0;
            if (std::fabs(daily_return) > threshold) {
                anomalies.push_back({table.dates[static_cast<std::size_t>(row)],
                                     table.tickers[static_cast<std::size_t>(column)],
                                     daily_return});
            }
        }
    }
    return anomalies;
}

}  // namespace quant

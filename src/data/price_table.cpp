/**
 * @file price_table.cpp
 * @brief Реализация чтения и подготовки таблицы цен.
 */
#include "quant/data/price_table.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace quant {

namespace {

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

std::vector<std::string_view> splitLine(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const auto comma = line.find(',', start);
        fields.push_back(trim(line.substr(start, comma - start)));
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return fields;
}

bool isIsoDate(std::string_view s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) {
            return false;
        }
    }
    const int month = (s[5] - '0') * 10 + (s[6] - '0');
    const int day = (s[8] - '0') * 10 + (s[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

double parseNumber(std::string_view s, const std::string& where) {
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size() || !std::isfinite(value)) {
        throw std::runtime_error(where + ": not a number: '" + std::string(s) + "'");
    }
    return value;
}

std::string location(const std::string& path, std::size_t lineNo) {
    return path + ":" + std::to_string(lineNo);
}

std::ifstream openOrThrow(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open file: " + path);
    }
    return in;
}

}  // namespace

PriceTable readPriceCsv(const std::string& path) {
    auto in = openOrThrow(path);
    std::string line;
    std::size_t lineNo = 0;

    if (!std::getline(in, line)) {
        throw std::runtime_error(path + ": empty file");
    }
    ++lineNo;
    const auto header = splitLine(line);
    if (header.size() < 2 || header[0] != "date") {
        throw std::runtime_error(location(path, lineNo) +
                                 ": header must be 'date,TICKER1,TICKER2,...'");
    }

    PriceTable table;
    for (std::size_t j = 1; j < header.size(); ++j) {
        if (header[j].empty()) {
            throw std::runtime_error(location(path, lineNo) + ": empty ticker name");
        }
        table.tickers.emplace_back(header[j]);
    }

    const std::size_t n = table.tickers.size();
    std::vector<double> values;
    while (std::getline(in, line)) {
        ++lineNo;
        if (trim(line).empty()) {
            continue;
        }
        const auto fields = splitLine(line);
        const auto where = location(path, lineNo);
        if (fields.size() != n + 1) {
            throw std::runtime_error(where + ": expected " + std::to_string(n + 1) +
                                     " fields, got " + std::to_string(fields.size()));
        }
        if (!isIsoDate(fields[0])) {
            throw std::runtime_error(where + ": bad date '" + std::string(fields[0]) +
                                     "', expected YYYY-MM-DD");
        }
        if (!table.dates.empty() && fields[0] <= table.dates.back()) {
            throw std::runtime_error(where + ": dates must be strictly increasing");
        }
        table.dates.emplace_back(fields[0]);
        for (std::size_t j = 1; j <= n; ++j) {
            if (fields[j].empty()) {
                values.push_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }
            const double price = parseNumber(fields[j], where);
            if (price <= 0.0) {
                throw std::runtime_error(where + ": price must be positive");
            }
            values.push_back(price);
        }
    }

    const auto t = static_cast<Eigen::Index>(table.dates.size());
    table.prices = Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic,
                                            Eigen::RowMajor>>(values.data(), t,
                                                              static_cast<Eigen::Index>(n));
    return table;
}

void forwardFill(PriceTable& table) {
    auto& p = table.prices;
    for (Eigen::Index j = 0; j < p.cols(); ++j) {
        for (Eigen::Index i = 0; i < p.rows(); ++i) {
            if (!std::isnan(p(i, j))) {
                continue;
            }
            if (i == 0) {
                throw std::runtime_error("no price for " +
                                         table.tickers[static_cast<std::size_t>(j)] +
                                         " on the first date " + table.dates.front());
            }
            p(i, j) = p(i - 1, j);
        }
    }
}

std::vector<Split> readSplitsCsv(const std::string& path) {
    auto in = openOrThrow(path);
    std::string line;
    std::size_t lineNo = 0;
    if (!std::getline(in, line) || splitLine(line) != std::vector<std::string_view>{
                                                         "date", "ticker", "ratio"}) {
        throw std::runtime_error(path + ": header must be 'date,ticker,ratio'");
    }
    ++lineNo;

    std::vector<Split> splits;
    while (std::getline(in, line)) {
        ++lineNo;
        if (trim(line).empty()) {
            continue;
        }
        const auto fields = splitLine(line);
        const auto where = location(path, lineNo);
        if (fields.size() != 3 || !isIsoDate(fields[0]) || fields[1].empty()) {
            throw std::runtime_error(where + ": expected 'YYYY-MM-DD,TICKER,ratio'");
        }
        splits.push_back({std::string(fields[0]), std::string(fields[1]),
                          parseNumber(fields[2], where)});
    }
    return splits;
}

void applySplits(PriceTable& table, const std::vector<Split>& splits) {
    for (const auto& split : splits) {
        if (!(split.ratio > 0.0)) {
            throw std::invalid_argument("split ratio must be positive: " + split.ticker + " " +
                                        split.date);
        }
        for (std::size_t j = 0; j < table.tickers.size(); ++j) {
            if (table.tickers[j] != split.ticker) {
                continue;
            }
            for (std::size_t i = 0; i < table.dates.size() && table.dates[i] < split.date;
                 ++i) {
                table.prices(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) /=
                    split.ratio;
            }
        }
    }
}

std::vector<PriceAnomaly> findAnomalies(const PriceTable& table, double threshold) {
    std::vector<PriceAnomaly> anomalies;
    const auto& p = table.prices;
    for (Eigen::Index i = 1; i < p.rows(); ++i) {
        for (Eigen::Index j = 0; j < p.cols(); ++j) {
            const double r = p(i, j) / p(i - 1, j) - 1.0;
            if (std::abs(r) > threshold) {
                anomalies.push_back({table.dates[static_cast<std::size_t>(i)],
                                     table.tickers[static_cast<std::size_t>(j)], r});
            }
        }
    }
    return anomalies;
}

}  // namespace quant

// Тесты модуля data: FR-2 (чтение CSV), FR-3 (пропуски), FR-4 (сплиты), FR-5 (аномалии).
#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "quant/data/price_table.hpp"

namespace {

std::string dataFile(const std::string& name) { return std::string(MPT_TEST_DATA_DIR) + "/" + name; }

quant::PriceTable makeTable() {
    quant::PriceTable t;
    t.dates = {"2024-01-03", "2024-01-04", "2024-01-05", "2024-01-08"};
    t.tickers = {"AAA", "BBB"};
    t.prices.resize(4, 2);
    t.prices << 10000, 50,  //
        10100, 51,          //
        102, 52,            // AAA: сплит 1:100 с 2024-01-05
        103, 53;
    return t;
}

}  // namespace

TEST(ReadPriceCsv, ParsesWideFormatWithGaps) {
    const auto t = quant::readPriceCsv(dataFile("prices_ok.csv"));
    ASSERT_EQ(t.tickers, (std::vector<std::string>{"AAA", "BBB"}));
    ASSERT_EQ(t.dates, (std::vector<std::string>{"2024-01-03", "2024-01-04", "2024-01-05"}));
    EXPECT_DOUBLE_EQ(t.prices(1, 0), 110.0);
    EXPECT_TRUE(std::isnan(t.prices(1, 1)));
    EXPECT_DOUBLE_EQ(t.prices(2, 1), 55.0);
}

TEST(ReadPriceCsv, HandlesCrlfAndSpaces) {
    const auto t = quant::readPriceCsv(dataFile("prices_crlf.csv"));
    ASSERT_EQ(t.prices.rows(), 2);
    EXPECT_DOUBLE_EQ(t.prices(1, 0), 101.0);
    EXPECT_DOUBLE_EQ(t.prices(1, 1), 51.0);
}

TEST(ReadPriceCsv, RejectsMalformedInput) {
    for (const char* name : {"prices_bad_fields.csv", "prices_bad_date.csv",
                             "prices_unsorted.csv", "prices_bad_number.csv",
                             "prices_negative.csv", "no_such_file.csv"}) {
        EXPECT_THROW(quant::readPriceCsv(dataFile(name)), std::runtime_error) << name;
    }
}

TEST(ForwardFill, FillsGapsWithLastPrice) {
    auto t = quant::readPriceCsv(dataFile("prices_ok.csv"));
    quant::forwardFill(t);
    EXPECT_FALSE(t.prices.hasNaN());
    EXPECT_DOUBLE_EQ(t.prices(1, 1), 50.0);
}

TEST(ForwardFill, RejectsMissingFirstPrice) {
    auto t = quant::readPriceCsv(dataFile("prices_leading_gap.csv"));
    EXPECT_THROW(quant::forwardFill(t), std::runtime_error);
}

TEST(Splits, ReadsCsv) {
    const auto splits = quant::readSplitsCsv(dataFile("splits_ok.csv"));
    ASSERT_EQ(splits.size(), 2u);
    EXPECT_EQ(splits[0].ticker, "AAA");
    EXPECT_EQ(splits[0].date, "2024-01-05");
    EXPECT_DOUBLE_EQ(splits[0].ratio, 100.0);
    EXPECT_THROW(quant::readSplitsCsv(dataFile("splits_bad.csv")), std::runtime_error);
}

TEST(Splits, AdjustsPricesBeforeSplitDateOnly) {
    auto t = makeTable();
    quant::applySplits(t, quant::readSplitsCsv(dataFile("splits_ok.csv")));
    EXPECT_DOUBLE_EQ(t.prices(0, 0), 100.0);
    EXPECT_DOUBLE_EQ(t.prices(1, 0), 101.0);
    EXPECT_DOUBLE_EQ(t.prices(2, 0), 102.0);
    EXPECT_DOUBLE_EQ(t.prices(0, 1), 50.0);  // другой тикер не тронут
}

TEST(Splits, RejectsNonPositiveRatio) {
    auto t = makeTable();
    EXPECT_THROW(quant::applySplits(t, {{"2024-01-05", "AAA", 0.0}}), std::invalid_argument);
}

TEST(Anomalies, DetectsUnadjustedSplit) {
    const auto t = makeTable();
    const auto anomalies = quant::findAnomalies(t, 0.5);
    ASSERT_EQ(anomalies.size(), 1u);
    EXPECT_EQ(anomalies[0].date, "2024-01-05");
    EXPECT_EQ(anomalies[0].ticker, "AAA");
    EXPECT_NEAR(anomalies[0].dailyReturn, 102.0 / 10100.0 - 1.0, 1e-12);

    auto adjusted = t;
    quant::applySplits(adjusted, {{"2024-01-05", "AAA", 100.0}});
    EXPECT_TRUE(quant::findAnomalies(adjusted, 0.5).empty());
}

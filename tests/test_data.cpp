// Тесты модуля data: FR-2 (чтение CSV), FR-3 (пропуски), FR-4 (сплиты), FR-5 (аномалии).
#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "quant/data/price_table.hpp"

namespace {

std::string GetTestDataPath(const std::string& file_name) {
    return std::string(MPT_TEST_DATA_DIR) + "/" + file_name;
}

// AAA: сплит 1:100 с 2024-01-05, цены до этой даты не скорректированы.
quant::PriceTable MakeTableWithUnadjustedSplit() {
    quant::PriceTable table;
    table.dates = {"2024-01-03", "2024-01-04", "2024-01-05", "2024-01-08"};
    table.tickers = {"AAA", "BBB"};
    table.prices.resize(4, 2);
    table.prices << 10000, 50,  //
        10100, 51,              //
        102, 52,                //
        103, 53;
    return table;
}

}  // namespace

TEST(ReadPriceCsv, ParsesWideFormatWithGaps) {
    const auto table = quant::ReadPriceCsv(GetTestDataPath("prices_ok.csv"));
    ASSERT_EQ(table.tickers, (std::vector<std::string>{"AAA", "BBB"}));
    ASSERT_EQ(table.dates, (std::vector<std::string>{"2024-01-03", "2024-01-04", "2024-01-05"}));
    EXPECT_DOUBLE_EQ(table.prices(1, 0), 110.0);
    EXPECT_TRUE(std::isnan(table.prices(1, 1)));
    EXPECT_DOUBLE_EQ(table.prices(2, 1), 55.0);
}

TEST(ReadPriceCsv, HandlesCrlfAndSpaces) {
    const auto table = quant::ReadPriceCsv(GetTestDataPath("prices_crlf.csv"));
    ASSERT_EQ(table.prices.rows(), 2);
    EXPECT_DOUBLE_EQ(table.prices(1, 0), 101.0);
    EXPECT_DOUBLE_EQ(table.prices(1, 1), 51.0);
}

TEST(ReadPriceCsv, RejectsMalformedInput) {
    for (const char* file_name :
         {"prices_bad_fields.csv", "prices_bad_date.csv", "prices_unsorted.csv",
          "prices_bad_number.csv", "prices_negative.csv", "no_such_file.csv"}) {
        EXPECT_THROW(quant::ReadPriceCsv(GetTestDataPath(file_name)), std::runtime_error)
            << file_name;
    }
}

TEST(FillMissingPrices, FillsGapsWithLastPrice) {
    const auto table =
        quant::FillMissingPrices(quant::ReadPriceCsv(GetTestDataPath("prices_ok.csv")));
    EXPECT_FALSE(table.prices.hasNaN());
    EXPECT_DOUBLE_EQ(table.prices(1, 1), 50.0);
}

TEST(FillMissingPrices, RejectsMissingFirstPrice) {
    auto table = quant::ReadPriceCsv(GetTestDataPath("prices_leading_gap.csv"));
    EXPECT_THROW(quant::FillMissingPrices(std::move(table)), std::runtime_error);
}

TEST(Splits, ReadsCsv) {
    const auto splits = quant::ReadSplitsCsv(GetTestDataPath("splits_ok.csv"));
    ASSERT_EQ(splits.size(), 2u);
    EXPECT_EQ(splits[0].ticker, "AAA");
    EXPECT_EQ(splits[0].date, "2024-01-05");
    EXPECT_DOUBLE_EQ(splits[0].ratio, 100.0);
    EXPECT_THROW(quant::ReadSplitsCsv(GetTestDataPath("splits_bad.csv")), std::runtime_error);
}

TEST(Splits, AdjustsPricesBeforeSplitDateOnly) {
    const auto table = quant::ApplySplits(MakeTableWithUnadjustedSplit(),
                                          quant::ReadSplitsCsv(GetTestDataPath("splits_ok.csv")));
    EXPECT_DOUBLE_EQ(table.prices(0, 0), 100.0);
    EXPECT_DOUBLE_EQ(table.prices(1, 0), 101.0);
    EXPECT_DOUBLE_EQ(table.prices(2, 0), 102.0);

    // Цены другого тикера не меняются.
    EXPECT_DOUBLE_EQ(table.prices(0, 1), 50.0);
}

TEST(Splits, RejectsNonPositiveRatio) {
    const quant::Split zero_ratio_split = {"2024-01-05", "AAA", 0.0};
    EXPECT_THROW(quant::ApplySplits(MakeTableWithUnadjustedSplit(), {zero_ratio_split}),
                 std::invalid_argument);
}

TEST(Anomalies, DetectsUnadjustedSplit) {
    const auto table = MakeTableWithUnadjustedSplit();
    const auto anomalies = quant::FindPriceAnomalies(table, 0.5);
    ASSERT_EQ(anomalies.size(), 1u);
    EXPECT_EQ(anomalies[0].date, "2024-01-05");
    EXPECT_EQ(anomalies[0].ticker, "AAA");
    EXPECT_NEAR(anomalies[0].daily_return, 102.0 / 10100.0 - 1.0, 1e-12);

    const quant::Split split = {"2024-01-05", "AAA", 100.0};
    const auto adjusted = quant::ApplySplits(table, {split});
    EXPECT_TRUE(quant::FindPriceAnomalies(adjusted, 0.5).empty());
}

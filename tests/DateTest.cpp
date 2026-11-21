#include "skarbonka/Date.hpp"

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using skarbonka::ErrorCode;

TEST(DateTest, ParsesValidDate)
{
    const auto date = skarbonka::parseDate("2026-10-30");
    ASSERT_TRUE(date.has_value());
    EXPECT_EQ(*date, 2026y / std::chrono::October / 30d);
}

TEST(DateTest, AcceptsLeapDay)
{
    EXPECT_TRUE(skarbonka::parseDate("2024-02-29").has_value());
}

TEST(DateTest, RejectsNonExistingDays)
{
    for (const auto* text : {"2026-02-29", "2026-04-31", "2026-13-01", "2026-00-10", "2026-01-00"}) {
        const auto date = skarbonka::parseDate(text);
        ASSERT_FALSE(date.has_value()) << text;
        EXPECT_EQ(date.error().code, ErrorCode::InvalidDate);
    }
}

TEST(DateTest, RejectsWrongFormat)
{
    for (const auto* text : {"", "2026-1-5", "30-10-2026", "2026/10/30", "2026-10-3x", " 2026-10-30"}) {
        EXPECT_FALSE(skarbonka::parseDate(text).has_value()) << text;
    }
}

TEST(DateTest, ParsesMonth)
{
    const auto month = skarbonka::parseMonth("2026-03");
    ASSERT_TRUE(month.has_value());
    EXPECT_EQ(*month, 2026y / std::chrono::March);
    EXPECT_FALSE(skarbonka::parseMonth("2026-13").has_value());
    EXPECT_FALSE(skarbonka::parseMonth("2026-3").has_value());
}

TEST(DateTest, FormatsDateAndMonth)
{
    EXPECT_EQ(skarbonka::formatDate(2026y / std::chrono::January / 5d), "2026-01-05");
    EXPECT_EQ(skarbonka::formatMonth(2026y / std::chrono::January), "2026-01");
}

TEST(DateTest, MonthOfDropsDay)
{
    EXPECT_EQ(skarbonka::monthOf(2026y / std::chrono::May / 17d), 2026y / std::chrono::May);
}

TEST(DateTest, TodayIsValidDate)
{
    EXPECT_TRUE(skarbonka::today().ok());
}

TEST(DateTest, ParsesAndFormatsYear)
{
    const auto year = skarbonka::parseYear("2026");
    ASSERT_TRUE(year.has_value());
    EXPECT_EQ(*year, 2026y);
    EXPECT_EQ(skarbonka::formatYear(2026y), "2026");
    for (const auto* text : {"", "26", "20266", "2026-10", "abcd", " 2026"}) {
        const auto invalid = skarbonka::parseYear(text);
        ASSERT_FALSE(invalid.has_value()) << text;
        EXPECT_EQ(invalid.error().code, ErrorCode::InvalidDate);
    }
}

#include "skarbonka/Money.hpp"

#include <gtest/gtest.h>

using skarbonka::ErrorCode;
using skarbonka::Money;

TEST(MoneyTest, ParsesWholeAmounts)
{
    const auto money = Money::parse("12");
    ASSERT_TRUE(money.has_value());
    EXPECT_EQ(money->cents(), 1200);
}

TEST(MoneyTest, ParsesFractionWithDotOrComma)
{
    EXPECT_EQ(Money::parse("12.50")->cents(), 1250);
    EXPECT_EQ(Money::parse("12,05")->cents(), 1205);
    EXPECT_EQ(Money::parse("0.5")->cents(), 50);
}

TEST(MoneyTest, ParsesNegativeAmounts)
{
    EXPECT_EQ(Money::parse("-3.99")->cents(), -399);
}

TEST(MoneyTest, RejectsMalformedInput)
{
    for (const auto* text : {"", "-", "abc", "12.", ".5", "1.234", "12.5x", "1 000", "+5", "1e3"}) {
        const auto money = Money::parse(text);
        ASSERT_FALSE(money.has_value()) << "zaakceptowano: '" << text << "'";
        EXPECT_EQ(money.error().code, ErrorCode::InvalidAmount);
    }
}

TEST(MoneyTest, RejectsAmountsThatWouldOverflow)
{
    EXPECT_FALSE(Money::parse("99999999999999999999").has_value());
}

TEST(MoneyTest, FormatsWithTwoDecimalPlaces)
{
    EXPECT_EQ(Money::fromCents(1250).toString(), "12.50");
    EXPECT_EQ(Money::fromCents(7).toString(), "0.07");
    EXPECT_EQ(Money::fromCents(-399).toString(), "-3.99");
    EXPECT_EQ(std::format("[{:>8}]", Money::fromCents(100)), "[    1.00]");
}

TEST(MoneyTest, SupportsArithmeticAndComparison)
{
    const auto a = Money::fromCents(500);
    const auto b = Money::fromCents(250);
    EXPECT_EQ(a + b, Money::fromCents(750));
    EXPECT_EQ(a - b, Money::fromCents(250));
    EXPECT_EQ(-a, Money::fromCents(-500));
    EXPECT_LT(b, a);
    EXPECT_GE(a, a);
    EXPECT_NE(a, b);
}

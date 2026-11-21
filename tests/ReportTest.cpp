#include "TestData.hpp"

#include "skarbonka/Filter.hpp"
#include "skarbonka/Report.hpp"

#include <gtest/gtest.h>

#include <list>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using skarbonka::Money;
using skarbonka::Transaction;
using skarbonka::TransactionType;

namespace {

std::vector<Transaction> sampleTransactions()
{
    return {
        makeTransaction(TransactionType::Income, 500000, "pensja", 2026y / std::chrono::September / 1d, 1),
        makeTransaction(TransactionType::Expense, 120000, "mieszkanie", 2026y / std::chrono::September / 5d, 2),
        makeTransaction(TransactionType::Expense, 30000, "jedzenie", 2026y / std::chrono::September / 9d, 3),
        makeTransaction(TransactionType::Income, 500000, "pensja", 2026y / std::chrono::October / 1d, 4),
        makeTransaction(TransactionType::Expense, 45000, "jedzenie", 2026y / std::chrono::October / 3d, 5),
        makeTransaction(TransactionType::Expense, 120000, "mieszkanie", 2026y / std::chrono::October / 5d, 6),
        makeTransaction(TransactionType::Expense, 5000, "transport", 2026y / std::chrono::October / 7d, 7),
    };
}

struct Item {
    std::string name;
    Money price;
};

}

static_assert(skarbonka::TransactionRange<std::vector<Transaction>>);
static_assert(skarbonka::TransactionRange<std::list<Transaction>&>);
static_assert(!skarbonka::TransactionRange<std::vector<int>>);
static_assert(skarbonka::MoneyProjection<decltype(&Transaction::amount), Transaction>);
static_assert(!skarbonka::MoneyProjection<decltype(&Transaction::category), Transaction>);
static_assert(skarbonka::GroupingKey<decltype(&Transaction::category), Transaction>);

TEST(ReportTest, SumOfWorksForAnyRangeAndProjection)
{
    const std::vector<Item> items{{"a", Money::fromCents(150)}, {"b", Money::fromCents(250)}};
    EXPECT_EQ(skarbonka::sumOf(items, &Item::price), Money::fromCents(400));
    EXPECT_EQ(skarbonka::sumOf(std::vector<Item>{}, &Item::price), Money{});
}

TEST(ReportTest, TotalsByGroupsByKey)
{
    const auto totals = skarbonka::totalsBy(sampleTransactions(), &Transaction::category, &Transaction::amount);
    ASSERT_EQ(totals.size(), 4U);
    EXPECT_EQ(totals.at("jedzenie"), Money::fromCents(75000));
    EXPECT_EQ(totals.at("pensja"), Money::fromCents(1000000));
}

TEST(ReportTest, SummarizeWholePeriod)
{
    const auto summary = skarbonka::summarize(sampleTransactions());

    EXPECT_EQ(summary.count, 7U);
    EXPECT_EQ(summary.income, Money::fromCents(1000000));
    EXPECT_EQ(summary.expenses, Money::fromCents(320000));
    EXPECT_EQ(summary.balance(), Money::fromCents(680000));

    const std::vector<skarbonka::CategoryTotal> expected{
        {"mieszkanie", Money::fromCents(240000)},
        {"jedzenie", Money::fromCents(75000)},
        {"transport", Money::fromCents(5000)},
    };
    EXPECT_EQ(summary.expensesByCategory, expected);

    ASSERT_EQ(summary.balanceByMonth.size(), 2U);
    EXPECT_EQ(summary.balanceByMonth.at(2026y / std::chrono::September), Money::fromCents(350000));
    EXPECT_EQ(summary.balanceByMonth.at(2026y / std::chrono::October), Money::fromCents(330000));
}

TEST(ReportTest, SummarizeFilteredView)
{
    const auto transactions = sampleTransactions();
    const auto summary =
        skarbonka::summarize(skarbonka::filtered(transactions, {.month = 2026y / std::chrono::September}));

    EXPECT_EQ(summary.count, 3U);
    EXPECT_EQ(summary.income, Money::fromCents(500000));
    EXPECT_EQ(summary.expenses, Money::fromCents(150000));
    EXPECT_EQ(summary.expensesByCategory.front().category, "mieszkanie");
}

TEST(ReportTest, CategoriesWithEqualTotalsAreSortedAlphabetically)
{
    const auto date = 2026y / std::chrono::October / 1d;
    const std::vector<Transaction> transactions{
        makeTransaction(TransactionType::Expense, 100, "zabawki", date, 1),
        makeTransaction(TransactionType::Expense, 100, "apteka", date, 2),
    };
    const auto summary = skarbonka::summarize(transactions);
    ASSERT_EQ(summary.expensesByCategory.size(), 2U);
    EXPECT_EQ(summary.expensesByCategory[0].category, "apteka");
    EXPECT_EQ(summary.expensesByCategory[1].category, "zabawki");
}

TEST(ReportTest, SummarizeEmptyRange)
{
    const auto summary = skarbonka::summarize(std::vector<Transaction>{});
    EXPECT_EQ(summary.count, 0U);
    EXPECT_EQ(summary.balance(), Money{});
    EXPECT_TRUE(summary.expensesByCategory.empty());
}

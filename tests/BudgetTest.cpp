#include "TestData.hpp"

#include "skarbonka/Budget.hpp"
#include "skarbonka/Filter.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <ranges>
#include <vector>

using namespace std::chrono_literals;
using skarbonka::Budget;
using skarbonka::ErrorCode;
using skarbonka::TransactionType;

namespace {

constexpr skarbonka::Date october = 2026y / std::chrono::October / 10d;
constexpr skarbonka::Date november = 2026y / std::chrono::November / 2d;

}

TEST(BudgetTest, AddAssignsConsecutiveIds)
{
    Budget budget;
    const auto first = budget.add(makeTransaction(TransactionType::Expense, 1000, "jedzenie", october));
    const auto second = budget.add(makeTransaction(TransactionType::Income, 5000, "pensja", october));

    ASSERT_TRUE(first && second);
    EXPECT_EQ(first->id, 1U);
    EXPECT_EQ(second->id, 2U);
    EXPECT_EQ(budget.transactions().size(), 2U);
}

TEST(BudgetTest, AddNormalizesCategory)
{
    Budget budget;
    const auto added = budget.add(makeTransaction(TransactionType::Expense, 100, "  Jedzenie ", october));
    ASSERT_TRUE(added);
    EXPECT_EQ(added->category, "jedzenie");
}

TEST(BudgetTest, AddRejectsInvalidTransactions)
{
    Budget budget;
    EXPECT_EQ(budget.add(makeTransaction(TransactionType::Expense, 0, "x", october)).error().code,
              ErrorCode::InvalidAmount);
    EXPECT_EQ(budget.add(makeTransaction(TransactionType::Expense, -5, "x", october)).error().code,
              ErrorCode::InvalidAmount);
    EXPECT_EQ(budget.add(makeTransaction(TransactionType::Expense, 100, "   ", october)).error().code,
              ErrorCode::InvalidArgument);
    EXPECT_TRUE(budget.transactions().empty());
}

TEST(BudgetTest, AmountHasUpperLimit)
{
    Budget budget;
    const auto limit = skarbonka::maxTransactionAmount.cents();
    EXPECT_TRUE(budget.add(makeTransaction(TransactionType::Income, limit, "x", october)));
    EXPECT_EQ(budget.add(makeTransaction(TransactionType::Income, limit + 1, "x", october)).error().code,
              ErrorCode::InvalidAmount);
}

TEST(BudgetTest, RemoveReturnsRemovedTransaction)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "a", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 200, "b", october)));

    const auto removed = budget.remove(1);
    ASSERT_TRUE(removed);
    EXPECT_EQ(removed->category, "a");
    ASSERT_EQ(budget.transactions().size(), 1U);
    EXPECT_EQ(budget.transactions().front().id, 2U);
}

TEST(BudgetTest, RemoveUnknownIdFails)
{
    Budget budget;
    const auto removed = budget.remove(42);
    ASSERT_FALSE(removed);
    EXPECT_EQ(removed.error().code, ErrorCode::NotFound);
}

TEST(BudgetTest, IdsAreNotReusedAfterRemoval)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "a", october)));
    ASSERT_TRUE(budget.remove(1));
    EXPECT_EQ(budget.add(makeTransaction(TransactionType::Expense, 100, "a", october))->id, 2U);
}

TEST(BudgetTest, FromTransactionsContinuesNumbering)
{
    const auto budget = Budget::fromTransactions({
        makeTransaction(TransactionType::Expense, 100, "a", october, 3),
        makeTransaction(TransactionType::Expense, 100, "b", october, 7),
    });
    ASSERT_TRUE(budget);
    EXPECT_EQ(budget->nextId(), 8U);
}

TEST(BudgetTest, FromTransactionsRejectsDuplicateIds)
{
    const auto budget = Budget::fromTransactions({
        makeTransaction(TransactionType::Expense, 100, "a", october, 3),
        makeTransaction(TransactionType::Expense, 100, "b", october, 3),
    });
    ASSERT_FALSE(budget);
    EXPECT_EQ(budget.error().code, ErrorCode::ParseError);
}

TEST(FilterTest, CombinesAllCriteria)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "jedzenie", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 200, "jedzenie", november)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 300, "transport", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Income, 400, "jedzenie", october)));

    const skarbonka::Filter filter{
        .type = TransactionType::Expense,
        .category = "jedzenie",
        .month = 2026y / std::chrono::October,
    };
    const auto ids = skarbonka::filtered(budget.transactions(), filter)
                   | std::views::transform(&skarbonka::Transaction::id)
                   | std::ranges::to<std::vector>();
    EXPECT_EQ(ids, std::vector<std::uint32_t>{1});
}

TEST(FilterTest, EmptyFilterMatchesEverything)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "a", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Income, 100, "b", november)));
    EXPECT_EQ(std::ranges::distance(skarbonka::filtered(budget.transactions(), {})), 2);
}

TEST(BudgetTest, EditChangesOnlyGivenFields)
{
    Budget budget;
    auto original = makeTransaction(TransactionType::Expense, 1000, "jedzenie", october);
    original.note = "zakupy";
    ASSERT_TRUE(budget.add(original));

    const auto edited = budget.edit(1, {.amount = skarbonka::Money::fromCents(1250), .category = " Restauracja "});
    ASSERT_TRUE(edited) << edited.error().message;
    EXPECT_EQ(edited->id, 1U);
    EXPECT_EQ(edited->amount, skarbonka::Money::fromCents(1250));
    EXPECT_EQ(edited->category, "restauracja");
    EXPECT_EQ(edited->type, TransactionType::Expense);
    EXPECT_EQ(edited->date, october);
    EXPECT_EQ(edited->note, "zakupy");
    EXPECT_EQ(budget.transactions().front(), *edited);
}

TEST(BudgetTest, EditCanClearNoteAndChangeTypeAndDate)
{
    Budget budget;
    auto original = makeTransaction(TransactionType::Expense, 1000, "zwrot", october);
    original.note = "do usunięcia";
    ASSERT_TRUE(budget.add(original));

    const auto edited = budget.edit(1, {.type = TransactionType::Income, .date = november, .note = ""});
    ASSERT_TRUE(edited);
    EXPECT_EQ(edited->type, TransactionType::Income);
    EXPECT_EQ(edited->date, november);
    EXPECT_TRUE(edited->note.empty());
}

TEST(BudgetTest, EditRejectsInvalidChangesAndKeepsTransaction)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 1000, "jedzenie", october)));
    const auto before = budget.transactions().front();

    EXPECT_EQ(budget.edit(1, {.amount = skarbonka::Money{}}).error().code, ErrorCode::InvalidAmount);
    EXPECT_EQ(budget.edit(1, {.category = "   "}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(budget.transactions().front(), before);
}

TEST(BudgetTest, EditUnknownIdFails)
{
    Budget budget;
    EXPECT_EQ(budget.edit(7, {.note = "x"}).error().code, ErrorCode::NotFound);
}

TEST(BudgetTest, RemoveIfRemovesMatchingAndKeepsOrder)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "a", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 200, "b", november)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 300, "c", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 400, "d", november)));

    const skarbonka::Filter octoberOnly{.month = 2026y / std::chrono::October};
    const auto removed = budget.removeIf([&](const skarbonka::Transaction& t) { return octoberOnly.matches(t); });

    const auto ids = [](auto&& transactions) {
        return transactions | std::views::transform(&skarbonka::Transaction::id) | std::ranges::to<std::vector>();
    };
    EXPECT_EQ(ids(removed), (std::vector<std::uint32_t>{1, 3}));
    EXPECT_EQ(ids(budget.transactions()), (std::vector<std::uint32_t>{2, 4}));
    EXPECT_EQ(budget.add(makeTransaction(TransactionType::Expense, 100, "e", october))->id, 5U);
}

TEST(BudgetTest, RemoveIfWithoutMatchesChangesNothing)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "a", october)));
    EXPECT_TRUE(budget.removeIf([](const skarbonka::Transaction&) { return false; }).empty());
    EXPECT_EQ(budget.transactions().size(), 1U);
}

TEST(FilterTest, FiltersByYear)
{
    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "a", 2025y / std::chrono::December / 31d)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "b", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "c", 2027y / std::chrono::January / 1d)));

    const auto ids = skarbonka::filtered(budget.transactions(), {.year = 2026y})
                   | std::views::transform(&skarbonka::Transaction::id)
                   | std::ranges::to<std::vector>();
    EXPECT_EQ(ids, std::vector<std::uint32_t>{2});
}

TEST(BudgetTest, NormalizesPolishUppercaseLettersInCategory)
{
    EXPECT_EQ(skarbonka::normalizeCategory(" ŻYWNOŚĆ "), "żywność");
    EXPECT_EQ(skarbonka::normalizeCategory("ĄĆĘŁŃÓŚŹŻ"), "ąćęłńóśźż");
    EXPECT_EQ(skarbonka::normalizeCategory("ąćęłńóśźż"), "ąćęłńóśźż");

    Budget budget;
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "Żywność", october)));
    ASSERT_TRUE(budget.add(makeTransaction(TransactionType::Expense, 100, "żywność", october)));
    EXPECT_EQ(budget.transactions()[0].category, budget.transactions()[1].category);
}

TEST(BudgetTest, FromTransactionsRejectsTooLargeId)
{
    const auto budget = Budget::fromTransactions({
        makeTransaction(TransactionType::Expense, 100, "a", october, std::numeric_limits<std::uint32_t>::max()),
        makeTransaction(TransactionType::Expense, 100, "b", october, 1),
    });
    ASSERT_FALSE(budget);
    EXPECT_EQ(budget.error().code, ErrorCode::ParseError);
}

TEST(BudgetTest, AddFailsInsteadOfReusingIdWhenIdsAreExhausted)
{
    auto budget = Budget::fromTransactions({makeTransaction(TransactionType::Expense, 100, "a", october, Budget::maxId)});
    ASSERT_TRUE(budget) << budget.error().message;
    EXPECT_FALSE(budget->add(makeTransaction(TransactionType::Expense, 100, "b", october)));
    EXPECT_EQ(budget->transactions().size(), 1U);
}

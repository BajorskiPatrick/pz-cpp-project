#include "skarbonka/App.hpp"
#include "skarbonka/Storage.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace {

class AppTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        file_ = std::filesystem::temp_directory_path() / ("skarbonka_app_test_" + testName + ".json");
        std::filesystem::remove(file_);
    }

    void TearDown() override { std::filesystem::remove(file_); }

    std::string execute(std::vector<std::string> arguments)
    {
        arguments.push_back("--file");
        arguments.push_back(file_.string());

        std::ostringstream out;
        const auto result = skarbonka::parseArguments(arguments).and_then(
            [&out](const skarbonka::Options& options) { return skarbonka::run(options, out); });
        if (!result) {
            lastError_ = result.error().message;
            return {};
        }
        return out.str();
    }

    std::filesystem::path file_;
    std::string lastError_;
};

bool contains(const std::string& text, const std::string& fragment)
{
    return text.find(fragment) != std::string::npos;
}

}

TEST_F(AppTest, AddSavesTransactionToFile)
{
    const auto output = execute({"add", "-a", "45.90", "-c", "jedzenie", "-d", "2026-10-03"});
    EXPECT_TRUE(contains(output, "Dodano transakcję #1")) << output;

    const auto budget = skarbonka::loadBudget(file_);
    ASSERT_TRUE(budget);
    ASSERT_EQ(budget->transactions().size(), 1U);
    EXPECT_EQ(budget->transactions().front().amount, skarbonka::Money::fromCents(4590));
}

TEST_F(AppTest, ListShowsTransactionsChronologically)
{
    execute({"add", "-a", "10", "-c", "b", "-d", "2026-10-20"});
    execute({"add", "-a", "20", "-c", "a", "-d", "2026-10-05"});

    const auto output = execute({"list"});
    const auto earlier = output.find("2026-10-05");
    const auto later = output.find("2026-10-20");
    ASSERT_NE(earlier, std::string::npos) << output;
    ASSERT_NE(later, std::string::npos) << output;
    EXPECT_LT(earlier, later);
    EXPECT_TRUE(contains(output, "saldo: -30.00")) << output;
}

TEST_F(AppTest, ListWithFilterThatMatchesNothing)
{
    execute({"add", "-a", "10", "-c", "kawa", "-d", "2026-10-20"});
    EXPECT_TRUE(contains(execute({"list", "--month", "2025-01"}), "Brak transakcji"));
}

TEST_F(AppTest, RemoveDeletesTransaction)
{
    execute({"add", "-a", "10", "-c", "kawa"});
    EXPECT_TRUE(contains(execute({"remove", "--id", "1"}), "Usunięto transakcję #1"));
    EXPECT_TRUE(skarbonka::loadBudget(file_)->transactions().empty());
}

TEST_F(AppTest, IdsAreNotReusedBetweenRuns)
{
    execute({"add", "-a", "10", "-c", "kawa"});
    execute({"add", "-a", "20", "-c", "kawa"});
    execute({"remove", "--id", "2"});
    EXPECT_TRUE(contains(execute({"add", "-a", "30", "-c", "kawa"}), "#3")) << lastError_;
}

TEST_F(AppTest, RemoveUnknownIdReportsError)
{
    execute({"add", "-a", "10", "-c", "kawa"});
    EXPECT_TRUE(execute({"remove", "--id", "5"}).empty());
    EXPECT_TRUE(contains(lastError_, "#5"));
}

TEST_F(AppTest, ReadingMissingFileReportsError)
{
    for (const auto& command : std::vector<std::vector<std::string>>{{"list"}, {"summary"}, {"remove", "--id", "1"}}) {
        lastError_.clear();
        EXPECT_TRUE(execute(command).empty()) << command.front();
        EXPECT_TRUE(contains(lastError_, "nie istnieje")) << command.front() << ": " << lastError_;
    }
    EXPECT_FALSE(std::filesystem::exists(file_));
}

TEST_F(AppTest, AddCreatesMissingFile)
{
    EXPECT_FALSE(std::filesystem::exists(file_));
    execute({"add", "-a", "10", "-c", "kawa"});
    EXPECT_TRUE(std::filesystem::exists(file_)) << lastError_;
}

TEST_F(AppTest, SummaryShowsTotalsAndCategories)
{
    execute({"add", "-t", "income", "-a", "1000", "-c", "pensja", "-d", "2026-10-01"});
    execute({"add", "-a", "300", "-c", "jedzenie", "-d", "2026-10-02"});
    execute({"add", "-a", "100", "-c", "transport", "-d", "2026-10-03"});
    execute({"add", "-a", "999", "-c", "wakacje", "-d", "2026-09-03"});

    const auto output = execute({"summary", "--month", "2026-10"});
    EXPECT_TRUE(contains(output, "1000.00")) << output;
    EXPECT_TRUE(contains(output, "400.00")) << output;
    EXPECT_TRUE(contains(output, "600.00")) << output;
    EXPECT_TRUE(contains(output, "75.0%")) << output;
    EXPECT_FALSE(contains(output, "wakacje")) << output;
}

TEST_F(AppTest, HelpDoesNotTouchFile)
{
    EXPECT_TRUE(contains(execute({"--help"}), "Użycie"));
    EXPECT_FALSE(std::filesystem::exists(file_));
}

TEST_F(AppTest, EditChangesTransactionInFile)
{
    execute({"add", "-a", "10", "-c", "kawa", "-d", "2026-10-03", "-n", "latte"});
    const auto output = execute({"edit", "--id", "1", "-a", "12.50", "-c", "Kawiarnia"});
    EXPECT_TRUE(contains(output, "Zmieniono transakcję #1")) << lastError_;
    EXPECT_TRUE(contains(output, "12.50")) << output;

    const auto budget = skarbonka::loadBudget(file_);
    ASSERT_TRUE(budget);
    const auto& edited = budget->transactions().front();
    EXPECT_EQ(edited.amount, skarbonka::Money::fromCents(1250));
    EXPECT_EQ(edited.category, "kawiarnia");
    EXPECT_EQ(edited.note, "latte");
    EXPECT_EQ(edited.date, skarbonka::parseDate("2026-10-03"));
    EXPECT_EQ(budget->nextId(), 2U);
}

TEST_F(AppTest, EditWithInvalidValueKeepsFileUnchanged)
{
    execute({"add", "-a", "10", "-c", "kawa"});
    const auto before = skarbonka::loadBudget(file_)->transactions().front();

    EXPECT_TRUE(execute({"edit", "--id", "1", "-c", "  "}).empty());
    EXPECT_TRUE(contains(lastError_, "kategoria")) << lastError_;
    EXPECT_TRUE(execute({"edit", "--id", "9", "-a", "5"}).empty());
    EXPECT_TRUE(contains(lastError_, "#9")) << lastError_;
    EXPECT_EQ(skarbonka::loadBudget(file_)->transactions().front(), before);
}

TEST_F(AppTest, RemoveByMonthDeletesOnlyThatMonth)
{
    execute({"add", "-a", "10", "-c", "a", "-d", "2026-09-30"});
    execute({"add", "-a", "20", "-c", "b", "-d", "2026-10-01"});
    execute({"add", "-a", "30", "-c", "c", "-d", "2026-10-31"});
    execute({"add", "-a", "40", "-c", "d", "-d", "2025-10-15"});

    const auto output = execute({"remove", "--month", "2026-10"});
    EXPECT_TRUE(contains(output, "miesiąca 2026-10")) << lastError_;
    EXPECT_TRUE(contains(output, "Liczba transakcji: 2")) << output;

    const auto budget = skarbonka::loadBudget(file_);
    ASSERT_TRUE(budget);
    const auto categories = budget->transactions()
                          | std::views::transform(&skarbonka::Transaction::category)
                          | std::ranges::to<std::vector>();
    EXPECT_EQ(categories, (std::vector<std::string>{"a", "d"}));
}

TEST_F(AppTest, RemoveByYearDeletesWholeYear)
{
    execute({"add", "-a", "10", "-c", "a", "-d", "2025-01-01"});
    execute({"add", "-a", "20", "-c", "b", "-d", "2025-12-31"});
    execute({"add", "-a", "30", "-c", "c", "-d", "2026-01-01"});

    EXPECT_TRUE(contains(execute({"remove", "--year", "2025"}), "roku 2025")) << lastError_;
    const auto budget = skarbonka::loadBudget(file_);
    ASSERT_TRUE(budget);
    ASSERT_EQ(budget->transactions().size(), 1U);
    EXPECT_EQ(budget->transactions().front().category, "c");
    EXPECT_TRUE(contains(execute({"add", "-a", "5", "-c", "e"}), "#4")) << lastError_;
}

TEST_F(AppTest, RemoveEmptyPeriodIsNotAnError)
{
    execute({"add", "-a", "10", "-c", "a", "-d", "2026-10-01"});
    lastError_.clear();
    const auto output = execute({"remove", "--year", "2024"});
    EXPECT_TRUE(lastError_.empty()) << lastError_;
    EXPECT_TRUE(contains(output, "Brak transakcji z roku 2024 - nic nie usunięto.")) << output;
    EXPECT_EQ(skarbonka::loadBudget(file_)->transactions().size(), 1U);
}

TEST_F(AppTest, ListFiltersByYear)
{
    execute({"add", "-a", "10", "-c", "stary", "-d", "2025-06-01"});
    execute({"add", "-a", "20", "-c", "nowy", "-d", "2026-06-01"});
    const auto output = execute({"list", "--year", "2026"});
    EXPECT_TRUE(contains(output, "nowy")) << output;
    EXPECT_FALSE(contains(output, "stary")) << output;
}

TEST_F(AppTest, SummaryForYearShowsMonthlyBalanceOfThatYearOnly)
{
    execute({"add", "-t", "income", "-a", "1000", "-c", "pensja", "-d", "2026-01-10"});
    execute({"add", "-a", "200", "-c", "jedzenie", "-d", "2026-02-10"});
    execute({"add", "-a", "999", "-c", "wakacje", "-d", "2025-07-01"});

    const auto output = execute({"summary", "--year", "2026"});
    EXPECT_TRUE(contains(output, "Podsumowanie: rok 2026")) << lastError_;
    EXPECT_TRUE(contains(output, "800.00")) << output;
    EXPECT_TRUE(contains(output, "Saldo w kolejnych miesiącach")) << output;
    EXPECT_TRUE(contains(output, "2026-02")) << output;
    EXPECT_FALSE(contains(output, "wakacje")) << output;
    EXPECT_FALSE(contains(output, "2025-07")) << output;
}

TEST_F(AppTest, CategoriesDifferingOnlyInPolishLetterCaseAreMerged)
{
    execute({"add", "-a", "10", "-c", "ŻYWNOŚĆ", "-d", "2026-10-01"});
    execute({"add", "-a", "5", "-c", "żywność", "-d", "2026-10-02"});
    const auto output = execute({"summary"});
    EXPECT_TRUE(contains(output, "żywność")) << lastError_;
    EXPECT_TRUE(contains(output, "100.0%")) << output;
}

#include "skarbonka/Cli.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

using namespace std::chrono_literals;
using skarbonka::ErrorCode;
using skarbonka::parseArguments;

TEST(CliTest, NoArgumentsShowsHelp)
{
    const auto options = parseArguments({});
    ASSERT_TRUE(options);
    EXPECT_TRUE(std::holds_alternative<skarbonka::HelpCommand>(options->command));
    EXPECT_EQ(options->dataFile, skarbonka::defaultDataFile);
}

TEST(CliTest, OptionsWithoutCommandAreAnError)
{
    for (const auto& arguments : std::vector<std::vector<std::string>>{{"--file", "dane.json"}, {"-m", "2026-10"}}) {
        const auto options = parseArguments(arguments);
        ASSERT_FALSE(options) << arguments.front();
        EXPECT_EQ(options.error().code, ErrorCode::InvalidArgument);
        EXPECT_NE(options.error().message.find("nie podano polecenia"), std::string::npos);
    }
    EXPECT_TRUE(parseArguments({"--file", "dane.json", "--help"}));
}

TEST(CliTest, HelpFlagsAndCommand)
{
    for (const auto& arguments : std::vector<std::vector<std::string>>{{"-h"}, {"--help"}, {"help"}, {"add", "--help"}}) {
        const auto options = parseArguments(arguments);
        ASSERT_TRUE(options) << options.error().message;
        EXPECT_TRUE(std::holds_alternative<skarbonka::HelpCommand>(options->command));
    }
}

TEST(CliTest, ParsesAddCommand)
{
    const auto options = parseArguments({"add", "--type", "income", "-a", "5200,50", "-c", "Pensja", "-d",
                                         "2026-10-01", "-n", "wypłata", "--file", "dom.json"});
    ASSERT_TRUE(options) << options.error().message;
    EXPECT_EQ(options->dataFile, "dom.json");

    const auto* add = std::get_if<skarbonka::AddCommand>(&options->command);
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->transaction.type, skarbonka::TransactionType::Income);
    EXPECT_EQ(add->transaction.amount, skarbonka::Money::fromCents(520050));
    EXPECT_EQ(add->transaction.category, "Pensja");
    EXPECT_EQ(add->transaction.date, 2026y / std::chrono::October / 1d);
    EXPECT_EQ(add->transaction.note, "wypłata");
}

TEST(CliTest, DataFileNameIsUtf8)
{
    const auto options = parseArguments({"list", "--file", "budżet.json"});
    ASSERT_TRUE(options);
    EXPECT_EQ(options->dataFile, std::filesystem::path{u8"budżet.json"});
}

TEST(CliTest, AddUsesDefaults)
{
    const auto options = parseArguments({"add", "--amount", "10", "--category", "kawa"});
    ASSERT_TRUE(options);
    const auto& add = std::get<skarbonka::AddCommand>(options->command);
    EXPECT_EQ(add.transaction.type, skarbonka::TransactionType::Expense);
    EXPECT_EQ(add.transaction.date, skarbonka::today());
    EXPECT_TRUE(add.transaction.note.empty());
}

TEST(CliTest, AddRequiresAmountAndCategory)
{
    const auto noAmount = parseArguments({"add", "--category", "kawa"});
    ASSERT_FALSE(noAmount);
    EXPECT_NE(noAmount.error().message.find("--amount"), std::string::npos);

    const auto noCategory = parseArguments({"add", "--amount", "10"});
    ASSERT_FALSE(noCategory);
    EXPECT_NE(noCategory.error().message.find("--category"), std::string::npos);
}

TEST(CliTest, AddPropagatesValueErrors)
{
    EXPECT_EQ(parseArguments({"add", "-a", "dziesięć", "-c", "x"}).error().code, ErrorCode::InvalidAmount);
    EXPECT_EQ(parseArguments({"add", "-a", "10", "-c", "x", "-d", "2026-02-30"}).error().code, ErrorCode::InvalidDate);
    EXPECT_EQ(parseArguments({"add", "-a", "10", "-c", "x", "-t", "loan"}).error().code, ErrorCode::InvalidArgument);
}

TEST(CliTest, ParsesListFilters)
{
    const auto options = parseArguments({"list", "--type", "expense", "--category", " Jedzenie", "--month", "2026-10"});
    ASSERT_TRUE(options) << options.error().message;
    const auto& filter = std::get<skarbonka::ListCommand>(options->command).filter;
    EXPECT_EQ(filter.type, skarbonka::TransactionType::Expense);
    EXPECT_EQ(filter.category, "jedzenie");
    EXPECT_EQ(filter.month, 2026y / std::chrono::October);
}

TEST(CliTest, ParsesRemoveAndSummary)
{
    const auto remove = parseArguments({"remove", "--id", "12"});
    ASSERT_TRUE(remove);
    EXPECT_EQ(std::get<skarbonka::RemoveCommand>(remove->command).id, 12U);

    const auto summary = parseArguments({"summary"});
    ASSERT_TRUE(summary);
    EXPECT_FALSE(std::get<skarbonka::SummaryCommand>(summary->command).period.has_value());
}

TEST(CliTest, RejectsInvalidId)
{
    for (const auto* id : {"0", "abc", "12x", "99999999999"}) {
        EXPECT_FALSE(parseArguments({"remove", "--id", id})) << id;
    }
}

TEST(CliTest, RejectsUnknownCommandAndOptions)
{
    EXPECT_FALSE(parseArguments({"delete"}));
    EXPECT_FALSE(parseArguments({"list", "--colour", "red"}));
    EXPECT_FALSE(parseArguments({"list", "extra"}));
}

TEST(CliTest, RejectsOptionNotMeantForCommand)
{
    const auto options = parseArguments({"summary", "--amount", "5"});
    ASSERT_FALSE(options);
    EXPECT_NE(options.error().message.find("--amount"), std::string::npos);
}

TEST(CliTest, HelpTextDescribesCommands)
{
    const auto text = skarbonka::helpText();
    for (const auto* word : {"add", "list", "remove", "summary", "--file", "--month"}) {
        EXPECT_NE(text.find(word), std::string::npos) << word;
    }
}

TEST(CliTest, ParsesEditCommand)
{
    const auto options = parseArguments({"edit", "--id", "4", "-a", "12,30", "-c", "Kawa", "-n", ""});
    ASSERT_TRUE(options) << options.error().message;
    const auto& edit = std::get<skarbonka::EditCommand>(options->command);
    EXPECT_EQ(edit.id, 4U);
    EXPECT_EQ(edit.changes.amount, skarbonka::Money::fromCents(1230));
    EXPECT_EQ(edit.changes.category, "Kawa");
    EXPECT_EQ(edit.changes.note, "");
    EXPECT_FALSE(edit.changes.type.has_value());
    EXPECT_FALSE(edit.changes.date.has_value());
}

TEST(CliTest, EditRequiresIdAndAtLeastOneChange)
{
    const auto noId = parseArguments({"edit", "--amount", "5"});
    ASSERT_FALSE(noId);
    EXPECT_NE(noId.error().message.find("--id"), std::string::npos);

    const auto noChanges = parseArguments({"edit", "--id", "1"});
    ASSERT_FALSE(noChanges);
    EXPECT_NE(noChanges.error().message.find("co najmniej jednej zmiany"), std::string::npos);

    EXPECT_FALSE(parseArguments({"edit", "--id", "1", "--month", "2026-10"}));
}

TEST(CliTest, EditPropagatesValueErrors)
{
    EXPECT_EQ(parseArguments({"edit", "--id", "1", "-a", "abc"}).error().code, ErrorCode::InvalidAmount);
    EXPECT_EQ(parseArguments({"edit", "--id", "1", "-d", "2026-02-30"}).error().code, ErrorCode::InvalidDate);
    EXPECT_EQ(parseArguments({"edit", "--id", "1", "-t", "loan"}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(parseArguments({"edit", "--id", "0", "-a", "5"}).error().code, ErrorCode::InvalidArgument);
}

TEST(CliTest, ParsesRemoveByMonthAndYear)
{
    const auto byMonth = parseArguments({"remove", "--month", "2026-10"});
    ASSERT_TRUE(byMonth) << byMonth.error().message;
    EXPECT_EQ(std::get<skarbonka::Month>(std::get<skarbonka::RemovePeriodCommand>(byMonth->command).period),
              2026y / std::chrono::October);

    const auto byYear = parseArguments({"remove", "-y", "2025"});
    ASSERT_TRUE(byYear) << byYear.error().message;
    EXPECT_EQ(std::get<skarbonka::Year>(std::get<skarbonka::RemovePeriodCommand>(byYear->command).period), 2025y);
}

TEST(CliTest, RemoveRequiresExactlyOneSelector)
{
    for (const auto& arguments : std::vector<std::vector<std::string>>{
             {"remove"},
             {"remove", "--id", "1", "--month", "2026-10"},
             {"remove", "--month", "2026-10", "--year", "2026"},
         }) {
        const auto options = parseArguments(arguments);
        ASSERT_FALSE(options);
        EXPECT_NE(options.error().message.find("dokładnie jednej"), std::string::npos) << options.error().message;
    }
    EXPECT_EQ(parseArguments({"remove", "--year", "26"}).error().code, ErrorCode::InvalidDate);
    EXPECT_EQ(parseArguments({"remove", "--month", "2026-13"}).error().code, ErrorCode::InvalidDate);
    EXPECT_FALSE(parseArguments({"remove", "--year", "2026", "--type", "expense"}));
}

TEST(CliTest, ListAcceptsYearFilter)
{
    const auto options = parseArguments({"list", "--year", "2026"});
    ASSERT_TRUE(options) << options.error().message;
    EXPECT_EQ(std::get<skarbonka::ListCommand>(options->command).filter.year, 2026y);
}

TEST(CliTest, SummaryAcceptsMonthOrYear)
{
    const auto byMonth = parseArguments({"summary", "--month", "2026-10"});
    ASSERT_TRUE(byMonth) << byMonth.error().message;
    const auto& monthPeriod = std::get<skarbonka::SummaryCommand>(byMonth->command).period;
    ASSERT_TRUE(monthPeriod.has_value());
    EXPECT_EQ(std::get<skarbonka::Month>(*monthPeriod), 2026y / std::chrono::October);

    const auto byYear = parseArguments({"summary", "-y", "2026"});
    ASSERT_TRUE(byYear) << byYear.error().message;
    const auto& yearPeriod = std::get<skarbonka::SummaryCommand>(byYear->command).period;
    ASSERT_TRUE(yearPeriod.has_value());
    EXPECT_EQ(std::get<skarbonka::Year>(*yearPeriod), 2026y);

    const auto both = parseArguments({"summary", "--month", "2026-10", "--year", "2026"});
    ASSERT_FALSE(both);
    EXPECT_NE(both.error().message.find("tylko jedną"), std::string::npos) << both.error().message;
    EXPECT_EQ(parseArguments({"summary", "--year", "2026x"}).error().code, ErrorCode::InvalidDate);
}

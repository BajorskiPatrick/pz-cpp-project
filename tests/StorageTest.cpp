#include "TestData.hpp"

#include "skarbonka/Storage.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <format>
#include <fstream>

using namespace std::chrono_literals;
using skarbonka::Budget;
using skarbonka::ErrorCode;
using skarbonka::TransactionType;

namespace {

Budget sampleBudget()
{
    Budget budget;
    auto expense = makeTransaction(TransactionType::Expense, 4590, "jedzenie", 2026y / std::chrono::October / 3d);
    expense.note = "zakupy \"na tydzień\"";
    EXPECT_TRUE(budget.add(expense));
    EXPECT_TRUE(budget.add(makeTransaction(TransactionType::Income, 520000, "pensja", 2026y / std::chrono::October / 1d)));
    return budget;
}

class StorageFileTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        directory_ = std::filesystem::temp_directory_path() /
                     ("skarbonka_test_" + std::string{::testing::UnitTest::GetInstance()->current_test_info()->name()});
        std::filesystem::remove_all(directory_);
    }

    void TearDown() override { std::filesystem::remove_all(directory_); }

    std::filesystem::path directory_;
};

}

TEST(StorageTest, SerializeAndDeserializeRoundTrip)
{
    const auto original = sampleBudget();
    const auto restored = skarbonka::deserialize(skarbonka::serialize(original));

    ASSERT_TRUE(restored) << restored.error().message;
    ASSERT_EQ(restored->transactions().size(), original.transactions().size());
    EXPECT_TRUE(std::ranges::equal(restored->transactions(), original.transactions()));
    EXPECT_EQ(restored->nextId(), original.nextId());
}

TEST(StorageTest, NextIdSurvivesRemovalOfLastTransaction)
{
    auto budget = sampleBudget();
    ASSERT_TRUE(budget.remove(2));

    const auto restored = skarbonka::deserialize(skarbonka::serialize(budget));
    ASSERT_TRUE(restored) << restored.error().message;
    EXPECT_EQ(restored->nextId(), 3U);
}

TEST(StorageTest, FileWithoutNextIdIsAccepted)
{
    const auto budget = skarbonka::deserialize(R"({"version": 1, "transactions": [
        {"id": 4, "type": "expense", "amount": "10.00", "category": "a", "date": "2026-10-01"}]})");
    ASSERT_TRUE(budget) << budget.error().message;
    EXPECT_EQ(budget->nextId(), 5U);
}

TEST(StorageTest, SerializedFormatIsReadable)
{
    const auto json = skarbonka::serialize(sampleBudget());
    EXPECT_NE(json.find("\"amount\": \"45.90\""), std::string::npos);
    EXPECT_NE(json.find("\"date\": \"2026-10-03\""), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"income\""), std::string::npos);
}

TEST(StorageTest, RejectsInvalidJson)
{
    const auto budget = skarbonka::deserialize("{ to nie jest json");
    ASSERT_FALSE(budget);
    EXPECT_EQ(budget.error().code, ErrorCode::ParseError);
}

TEST(StorageTest, RejectsMissingFields)
{
    const auto budget = skarbonka::deserialize(R"({"version": 1, "transactions": [{"id": 1}]})");
    ASSERT_FALSE(budget);
    EXPECT_EQ(budget.error().code, ErrorCode::ParseError);
}

TEST(StorageTest, RejectsInvalidValues)
{
    const auto budget = skarbonka::deserialize(R"({"version": 1, "transactions": [
        {"id": 1, "type": "expense", "amount": "10.00", "category": "a", "date": "2026-02-30"}]})");
    ASSERT_FALSE(budget);
    EXPECT_EQ(budget.error().code, ErrorCode::InvalidDate);
    EXPECT_NE(budget.error().message.find("transakcja #1"), std::string::npos);
}

TEST(StorageTest, RejectsIdsThatAreNotUnsignedIntegers)
{
    for (const auto* id : {"-2", "1.7", "4294967296", "\"3\""}) {
        const auto budget = skarbonka::deserialize(std::format(
            R"({{"version": 1, "transactions": [
                {{"id": {}, "type": "expense", "amount": "1.00", "category": "a", "date": "2026-10-01"}}]}})",
            id));
        ASSERT_FALSE(budget) << id;
        EXPECT_EQ(budget.error().code, ErrorCode::ParseError) << id;
        EXPECT_NE(budget.error().message.find(std::format("transakcja #{}", id)), std::string::npos)
            << budget.error().message;
    }
    for (const auto* nextId : {"-1", "2.5"}) {
        const auto budget =
            skarbonka::deserialize(std::format(R"({{"version": 1, "nextId": {}, "transactions": []}})", nextId));
        ASSERT_FALSE(budget) << nextId;
        EXPECT_EQ(budget.error().code, ErrorCode::ParseError) << nextId;
    }
}

TEST(StorageTest, RejectsUnknownVersion)
{
    EXPECT_FALSE(skarbonka::deserialize(R"({"version": 99, "transactions": []})"));
}

TEST_F(StorageFileTest, MissingFileMeansEmptyBudget)
{
    const auto budget = skarbonka::loadBudget(directory_ / "nie_istnieje.json");
    ASSERT_TRUE(budget);
    EXPECT_TRUE(budget->transactions().empty());
}

TEST_F(StorageFileTest, MissingFileCanBeAnError)
{
    const auto budget = skarbonka::loadBudget(directory_ / "nie_istnieje.json", skarbonka::IfMissing::Fail);
    ASSERT_FALSE(budget);
    EXPECT_EQ(budget.error().code, ErrorCode::NotFound);
    EXPECT_NE(budget.error().message.find("nie_istnieje.json"), std::string::npos);
}

TEST_F(StorageFileTest, SaveAndLoadFile)
{
    const auto path = directory_ / "dane" / "budzet.json";
    ASSERT_TRUE(skarbonka::saveBudget(sampleBudget(), path));
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));

    const auto loaded = skarbonka::loadBudget(path);
    ASSERT_TRUE(loaded) << loaded.error().message;
    EXPECT_EQ(loaded->transactions().size(), 2U);
}

TEST_F(StorageFileTest, PathWithPolishCharacters)
{
    const auto path = directory_ / std::filesystem::path{u8"budżet domowy.json"};
    ASSERT_TRUE(skarbonka::saveBudget(sampleBudget(), path));
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_TRUE(skarbonka::loadBudget(path));
}

TEST_F(StorageFileTest, CorruptedFileReportsPath)
{
    std::filesystem::create_directories(directory_);
    const auto path = directory_ / "zepsuty.json";
    std::ofstream{path} << "[1, 2,";

    const auto loaded = skarbonka::loadBudget(path);
    ASSERT_FALSE(loaded);
    EXPECT_NE(loaded.error().message.find("zepsuty.json"), std::string::npos);
}

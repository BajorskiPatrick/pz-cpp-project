#pragma once

#include "skarbonka/Date.hpp"
#include "skarbonka/Error.hpp"
#include "skarbonka/Money.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace skarbonka {

enum class TransactionType { Expense, Income };

[[nodiscard]] std::string_view toString(TransactionType type) noexcept;
[[nodiscard]] std::string_view displayName(TransactionType type) noexcept;
[[nodiscard]] Result<TransactionType> parseTransactionType(std::string_view text);

[[nodiscard]] std::string normalizeCategory(std::string_view category);

struct Transaction {
    std::uint32_t id{0};
    TransactionType type{TransactionType::Expense};
    Money amount;
    std::string category;
    Date date;
    std::string note;

    [[nodiscard]] Money signedAmount() const noexcept
    {
        return type == TransactionType::Income ? amount : -amount;
    }

    bool operator==(const Transaction&) const = default;
};

struct TransactionChanges {
    std::optional<TransactionType> type{};
    std::optional<Money> amount{};
    std::optional<std::string> category{};
    std::optional<Date> date{};
    std::optional<std::string> note{};

    [[nodiscard]] bool empty() const noexcept
    {
        return !type && !amount && !category && !date && !note;
    }

    [[nodiscard]] Transaction applyTo(Transaction transaction) const;
};

inline constexpr Money maxTransactionAmount = Money::fromCents(1'000'000'000'00);

[[nodiscard]] Result<Transaction> validate(Transaction transaction);

}

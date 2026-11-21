#include "skarbonka/Transaction.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <array>
#include <format>
#include <utility>

namespace skarbonka {

std::string_view toString(TransactionType type) noexcept
{
    return type == TransactionType::Income ? "income" : "expense";
}

std::string_view displayName(TransactionType type) noexcept
{
    return type == TransactionType::Income ? "przychód" : "wydatek";
}

Result<TransactionType> parseTransactionType(std::string_view text)
{
    const std::string normalized = boost::algorithm::to_lower_copy(std::string{text});
    if (normalized == "expense") {
        return TransactionType::Expense;
    }
    if (normalized == "income") {
        return TransactionType::Income;
    }
    return makeError(ErrorCode::InvalidArgument,
                     std::format("nieznany typ transakcji '{}' (dozwolone: expense, income)", text));
}

namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 9> polishLetters{{
    {"Ą", "ą"}, {"Ć", "ć"}, {"Ę", "ę"}, {"Ł", "ł"}, {"Ń", "ń"},
    {"Ó", "ó"}, {"Ś", "ś"}, {"Ź", "ź"}, {"Ż", "ż"},
}};

}

std::string normalizeCategory(std::string_view category)
{
    auto normalized = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(std::string{category}));
    for (const auto& [upper, lower] : polishLetters) {
        boost::algorithm::replace_all(normalized, upper, lower);
    }
    return normalized;
}

Transaction TransactionChanges::applyTo(Transaction transaction) const
{
    transaction.type = type.value_or(transaction.type);
    transaction.amount = amount.value_or(transaction.amount);
    transaction.category = category.value_or(transaction.category);
    transaction.date = date.value_or(transaction.date);
    transaction.note = note.value_or(transaction.note);
    return transaction;
}

Result<Transaction> validate(Transaction transaction)
{
    if (transaction.amount <= Money{}) {
        return makeError(ErrorCode::InvalidAmount,
                         std::format("kwota musi być dodatnia (podano {})", transaction.amount));
    }
    if (transaction.amount > maxTransactionAmount) {
        return makeError(ErrorCode::InvalidAmount,
                         std::format("kwota nie może przekraczać {} (podano {})", maxTransactionAmount,
                                     transaction.amount));
    }
    transaction.category = normalizeCategory(transaction.category);
    if (transaction.category.empty()) {
        return makeError(ErrorCode::InvalidArgument, "kategoria nie może być pusta");
    }
    if (!transaction.date.ok()) {
        return makeError(ErrorCode::InvalidDate, "niepoprawna data transakcji");
    }
    return transaction;
}

}

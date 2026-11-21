#include "skarbonka/Budget.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace skarbonka {

Result<Budget> Budget::fromTransactions(std::vector<Transaction> transactions, std::uint32_t nextId)
{
    Budget budget;
    for (auto& transaction : transactions) {
        auto valid = validate(std::move(transaction));
        if (!valid) {
            return std::unexpected(valid.error());
        }
        transaction = std::move(*valid);
    }

    auto ids = transactions | std::views::transform(&Transaction::id) | std::ranges::to<std::vector>();
    std::ranges::sort(ids);
    if (const auto duplicate = std::ranges::adjacent_find(ids); duplicate != ids.end()) {
        return makeError(ErrorCode::ParseError, std::format("powtórzony identyfikator transakcji #{}", *duplicate));
    }
    if (std::ranges::find(ids, 0U) != ids.end()) {
        return makeError(ErrorCode::ParseError, "identyfikator transakcji musi być dodatni");
    }
    if (!ids.empty() && ids.back() > maxId) {
        return makeError(ErrorCode::ParseError,
                         std::format("identyfikator transakcji #{} jest za duży (największy dozwolony: {})",
                                     ids.back(), maxId));
    }

    budget.nextId_ = std::max(nextId, ids.empty() ? 1U : ids.back() + 1);
    budget.transactions_ = std::move(transactions);
    return budget;
}

Result<Transaction> Budget::add(Transaction transaction)
{
    if (nextId_ > maxId) {
        return makeError(ErrorCode::InvalidArgument, "wyczerpano pulę identyfikatorów transakcji");
    }
    return validate(std::move(transaction)).transform([this](Transaction valid) {
        valid.id = nextId_++;
        transactions_.push_back(valid);
        return valid;
    });
}

Result<Transaction> Budget::remove(std::uint32_t id)
{
    const auto it = std::ranges::find(transactions_, id, &Transaction::id);
    if (it == transactions_.end()) {
        return makeError(ErrorCode::NotFound, std::format("nie ma transakcji o identyfikatorze #{}", id));
    }
    Transaction removed = std::move(*it);
    transactions_.erase(it);
    return removed;
}

Result<Transaction> Budget::edit(std::uint32_t id, const TransactionChanges& changes)
{
    const auto it = std::ranges::find(transactions_, id, &Transaction::id);
    if (it == transactions_.end()) {
        return makeError(ErrorCode::NotFound, std::format("nie ma transakcji o identyfikatorze #{}", id));
    }
    return validate(changes.applyTo(*it)).transform([&it](Transaction edited) {
        *it = edited;
        return edited;
    });
}

}

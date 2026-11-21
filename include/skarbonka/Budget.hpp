#pragma once

#include "skarbonka/Error.hpp"
#include "skarbonka/Transaction.hpp"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

namespace skarbonka {

class Budget {
public:
    static constexpr std::uint32_t maxId = std::numeric_limits<std::uint32_t>::max() - 1;

    Budget() = default;

    [[nodiscard]] static Result<Budget> fromTransactions(std::vector<Transaction> transactions,
                                                         std::uint32_t nextId = 1);

    [[nodiscard]] Result<Transaction> add(Transaction transaction);

    [[nodiscard]] Result<Transaction> remove(std::uint32_t id);

    template <std::predicate<const Transaction&> Predicate>
    std::vector<Transaction> removeIf(Predicate predicate)
    {
        const auto toRemove = std::ranges::stable_partition(transactions_, std::not_fn(std::move(predicate)));
        std::vector<Transaction> removed{std::make_move_iterator(toRemove.begin()),
                                         std::make_move_iterator(toRemove.end())};
        transactions_.erase(toRemove.begin(), toRemove.end());
        return removed;
    }

    [[nodiscard]] Result<Transaction> edit(std::uint32_t id, const TransactionChanges& changes);

    [[nodiscard]] std::span<const Transaction> transactions() const noexcept { return transactions_; }
    [[nodiscard]] std::uint32_t nextId() const noexcept { return nextId_; }

private:
    std::vector<Transaction> transactions_;
    std::uint32_t nextId_{1};
};

}

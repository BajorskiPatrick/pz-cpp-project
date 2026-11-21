#pragma once

#include "skarbonka/Date.hpp"
#include "skarbonka/Transaction.hpp"

#include <optional>
#include <ranges>
#include <span>
#include <string>

namespace skarbonka {

struct Filter {
    std::optional<TransactionType> type{};
    std::optional<std::string> category{};
    std::optional<Month> month{};
    std::optional<Year> year{};

    [[nodiscard]] bool matches(const Transaction& transaction) const;
};

[[nodiscard]] inline auto filtered(std::span<const Transaction> transactions, Filter filter)
{
    return transactions | std::views::filter([filter = std::move(filter)](const Transaction& transaction) {
               return filter.matches(transaction);
           });
}

}

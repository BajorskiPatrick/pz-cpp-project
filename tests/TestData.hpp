#pragma once

#include "skarbonka/Transaction.hpp"

#include <chrono>
#include <string>

inline skarbonka::Transaction makeTransaction(skarbonka::TransactionType type,
                                              std::int64_t cents,
                                              std::string category,
                                              skarbonka::Date date,
                                              std::uint32_t id = 0)
{
    skarbonka::Transaction transaction;
    transaction.id = id;
    transaction.type = type;
    transaction.amount = skarbonka::Money::fromCents(cents);
    transaction.category = std::move(category);
    transaction.date = date;
    return transaction;
}

#pragma once

#include "skarbonka/Date.hpp"
#include "skarbonka/Money.hpp"
#include "skarbonka/Transaction.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

namespace skarbonka {

template <typename R>
concept TransactionRange =
    std::ranges::forward_range<R> &&
    std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, Transaction>;

template <typename F, typename T>
concept MoneyProjection =
    std::regular_invocable<F&, const T&> && std::convertible_to<std::invoke_result_t<F&, const T&>, Money>;

template <typename F, typename T>
concept GroupingKey =
    std::regular_invocable<F&, const T&> &&
    std::totally_ordered<std::remove_cvref_t<std::invoke_result_t<F&, const T&>>>;

template <std::ranges::input_range R, MoneyProjection<std::ranges::range_value_t<R>> Proj>
[[nodiscard]] Money sumOf(R&& range, Proj projection)
{
    return std::ranges::fold_left(std::forward<R>(range) | std::views::transform(projection), Money{}, std::plus<>{});
}

template <std::ranges::input_range R,
          GroupingKey<std::ranges::range_value_t<R>> KeyFn,
          MoneyProjection<std::ranges::range_value_t<R>> ValueFn>
[[nodiscard]] auto totalsBy(R&& range, KeyFn key, ValueFn value)
{
    using Key = std::remove_cvref_t<std::invoke_result_t<KeyFn&, const std::ranges::range_value_t<R>&>>;
    std::map<Key, Money> totals;
    for (const auto& item : range) {
        totals[std::invoke(key, item)] += std::invoke(value, item);
    }
    return totals;
}

struct CategoryTotal {
    std::string category;
    Money total;

    bool operator==(const CategoryTotal&) const = default;
};

struct Summary {
    std::size_t count{0};
    Money income;
    Money expenses;
    std::vector<CategoryTotal> expensesByCategory;
    std::map<Month, Money> balanceByMonth;

    [[nodiscard]] Money balance() const noexcept { return income - expenses; }
};

[[nodiscard]] inline auto ofType(TransactionType type)
{
    return std::views::filter([type](const Transaction& transaction) { return transaction.type == type; });
}

template <TransactionRange R>
[[nodiscard]] Summary summarize(R&& transactions)
{
    Summary summary;
    summary.count = static_cast<std::size_t>(std::ranges::distance(transactions));
    summary.income = sumOf(transactions | ofType(TransactionType::Income), &Transaction::amount);
    summary.expenses = sumOf(transactions | ofType(TransactionType::Expense), &Transaction::amount);

    const auto byCategory =
        totalsBy(transactions | ofType(TransactionType::Expense), &Transaction::category, &Transaction::amount);
    summary.expensesByCategory = byCategory
        | std::views::transform([](const auto& entry) { return CategoryTotal{entry.first, entry.second}; })
        | std::ranges::to<std::vector>();
    std::ranges::stable_sort(summary.expensesByCategory, std::ranges::greater{}, &CategoryTotal::total);

    summary.balanceByMonth =
        totalsBy(transactions, [](const Transaction& t) { return monthOf(t.date); }, &Transaction::signedAmount);
    return summary;
}

}

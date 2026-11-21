#include "skarbonka/App.hpp"

#include "skarbonka/Budget.hpp"
#include "skarbonka/Filter.hpp"
#include "skarbonka/Report.hpp"
#include "skarbonka/Storage.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace skarbonka {

namespace {

template <typename... Lambdas>
struct Overloaded : Lambdas... {
    using Lambdas::operator()...;
};

constexpr std::size_t barWidth = 30;

void printTransactions(std::ostream& out, const std::vector<Transaction>& transactions)
{
    if (transactions.empty()) {
        out << "Brak transakcji spełniających kryteria.\n";
        return;
    }

    out << std::format("{:>4}  {:<10}  {:<8}  {:>11}  {:<14}  {}\n", "ID", "Data", "Typ", "Kwota", "Kategoria", "Opis");
    out << std::string(70, '-') << '\n';
    for (const auto& t : transactions) {
        const auto amount = std::format("{}{}", t.type == TransactionType::Income ? "+" : "-", t.amount);
        out << std::format("{:>4}  {:<10}  {:<8}  {:>11}  {:<14}  {}\n", t.id, formatDate(t.date),
                           displayName(t.type), amount, t.category, t.note);
    }
    out << std::string(70, '-') << '\n';
    out << std::format("Liczba transakcji: {}, saldo: {}\n", transactions.size(),
                       sumOf(transactions, &Transaction::signedAmount));
}

std::string periodLabel(const std::optional<Period>& period)
{
    if (!period) {
        return "cały okres";
    }
    return std::visit(Overloaded{
                          [](const Month& month) { return formatMonth(month); },
                          [](const Year& year) { return std::format("rok {}", formatYear(year)); },
                      },
                      *period);
}

void printSummary(std::ostream& out, const Summary& summary, const std::optional<Period>& period)
{
    out << "Podsumowanie: " << periodLabel(period) << "\n\n";
    if (summary.count == 0) {
        out << "Brak transakcji w wybranym okresie.\n";
        return;
    }

    out << std::format("  Przychody: {:>12}\n", summary.income);
    out << std::format("  Wydatki:   {:>12}\n", summary.expenses);
    out << std::format("  Saldo:     {:>12}\n", summary.balance());

    if (!summary.expensesByCategory.empty()) {
        out << "\nWydatki według kategorii:\n";
        const auto total = summary.expenses.cents();
        for (const auto& [category, amount] : summary.expensesByCategory) {
            const double share = total > 0 ? static_cast<double>(amount.cents()) / static_cast<double>(total) : 0.0;
            const auto bar = std::string(static_cast<std::size_t>(share * barWidth + 0.5), '#');
            out << std::format("  {:<14} {:>12} {:>6.1f}%  {}\n", category, amount, share * 100.0, bar);
        }
    }

    const bool singleMonth = period && std::holds_alternative<Month>(*period);
    if (!singleMonth && summary.balanceByMonth.size() > 1) {
        out << "\nSaldo w kolejnych miesiącach:\n";
        for (const auto& [monthKey, balance] : summary.balanceByMonth) {
            out << std::format("  {}  {:>12}\n", formatMonth(monthKey), balance);
        }
    }
}

void sortChronologically(std::vector<Transaction>& transactions)
{
    std::ranges::sort(transactions, {}, [](const Transaction& t) { return std::tie(t.date, t.id); });
}

std::string describePeriod(const Period& period)
{
    return std::visit(Overloaded{
                          [](const Month& month) { return std::format("miesiąca {}", formatMonth(month)); },
                          [](const Year& year) { return std::format("roku {}", formatYear(year)); },
                      },
                      period);
}

Filter periodFilter(const Period& period)
{
    return std::visit(Overloaded{
                          [](const Month& month) { return Filter{.month = month}; },
                          [](const Year& year) { return Filter{.year = year}; },
                      },
                      period);
}

template <typename Modify>
Result<void> modifyBudget(const std::filesystem::path& file, IfMissing ifMissing, Modify modify)
{
    auto budget = loadBudget(file, ifMissing);
    if (!budget) {
        return std::unexpected(budget.error());
    }
    return modify(*budget).and_then([&] { return saveBudget(*budget, file); });
}

}

Result<void> run(const Options& options, std::ostream& out)
{
    const auto& file = options.dataFile;

    return std::visit(
        Overloaded{
            [&](const HelpCommand&) -> Result<void> {
                out << helpText();
                return {};
            },
            [&](const AddCommand& command) -> Result<void> {
                return modifyBudget(file, IfMissing::CreateEmpty, [&](Budget& budget) {
                    return budget.add(command.transaction).transform([&](const Transaction& added) {
                        out << std::format("Dodano transakcję #{}: {} {} ({}, {})\n", added.id,
                                           displayName(added.type), added.amount, added.category,
                                           formatDate(added.date));
                    });
                });
            },
            [&](const RemoveCommand& command) -> Result<void> {
                return modifyBudget(file, IfMissing::Fail, [&](Budget& budget) {
                    return budget.remove(command.id).transform([&](const Transaction& removed) {
                        out << std::format("Usunięto transakcję #{} ({} {}, {})\n", removed.id,
                                           displayName(removed.type), removed.amount, removed.category);
                    });
                });
            },
            [&](const RemovePeriodCommand& command) -> Result<void> {
                return modifyBudget(file, IfMissing::Fail, [&](Budget& budget) -> Result<void> {
                    const auto filter = periodFilter(command.period);
                    auto removed = budget.removeIf([&filter](const Transaction& t) { return filter.matches(t); });
                    if (removed.empty()) {
                        out << std::format("Brak transakcji z {} - nic nie usunięto.\n",
                                           describePeriod(command.period));
                        return {};
                    }
                    sortChronologically(removed);
                    out << std::format("Usunięto wszystkie transakcje z {}:\n", describePeriod(command.period));
                    printTransactions(out, removed);
                    return {};
                });
            },
            [&](const EditCommand& command) -> Result<void> {
                return modifyBudget(file, IfMissing::Fail, [&](Budget& budget) {
                    return budget.edit(command.id, command.changes).transform([&](const Transaction& edited) {
                        out << std::format("Zmieniono transakcję #{}: {} {} ({}, {}){}\n", edited.id,
                                           displayName(edited.type), edited.amount, edited.category,
                                           formatDate(edited.date),
                                           edited.note.empty() ? std::string{} : std::format(" \"{}\"", edited.note));
                    });
                });
            },
            [&](const ListCommand& command) -> Result<void> {
                return loadBudget(file, IfMissing::Fail).transform([&](const Budget& budget) {
                    auto selected = filtered(budget.transactions(), command.filter) | std::ranges::to<std::vector>();
                    sortChronologically(selected);
                    printTransactions(out, selected);
                });
            },
            [&](const SummaryCommand& command) -> Result<void> {
                return loadBudget(file, IfMissing::Fail).transform([&](const Budget& budget) {
                    const auto filter = command.period ? periodFilter(*command.period) : Filter{};
                    const auto summary = summarize(filtered(budget.transactions(), filter));
                    printSummary(out, summary, command.period);
                });
            },
        },
        options.command);
}

}

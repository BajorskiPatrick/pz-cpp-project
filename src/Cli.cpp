#include "skarbonka/Cli.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <initializer_list>
#include <sstream>
#include <string_view>

namespace skarbonka {

namespace po = boost::program_options;

namespace {

void describeOptions(po::options_description& general, po::options_description& commands)
{
    general.add_options()
        ("help,h", "wyświetla tę pomoc")
        ("file,f", po::value<std::string>()->default_value(defaultDataFile), "plik z danymi (JSON), względem bieżącego katalogu; tworzy go polecenie add");

    commands.add_options()
        ("type,t", po::value<std::string>(), "typ transakcji: expense (domyślnie) | income")
        ("amount,a", po::value<std::string>(), "kwota, np. 12.50 lub 12,50")
        ("category,c", po::value<std::string>(), "kategoria, np. jedzenie")
        ("date,d", po::value<std::string>(), "data RRRR-MM-DD (domyślnie dzisiaj)")
        ("note,n", po::value<std::string>(), "krótki opis transakcji")
        ("month,m", po::value<std::string>(), "miesiąc RRRR-MM")
        ("year,y", po::value<std::string>(), "rok RRRR")
        ("id", po::value<std::string>(), "identyfikator transakcji (z polecenia list)");
}

std::optional<std::string> optionalValue(const po::variables_map& values, const std::string& name)
{
    if (!values.contains(name)) {
        return std::nullopt;
    }
    return values[name].as<std::string>();
}

Result<std::string> requiredValue(const po::variables_map& values, const std::string& name, std::string_view command)
{
    if (auto value = optionalValue(values, name)) {
        return std::move(*value);
    }
    return makeError(ErrorCode::InvalidArgument, std::format("polecenie '{}' wymaga opcji --{}", command, name));
}

Result<void> checkAllowedOptions(const po::variables_map& values,
                                 std::string_view command,
                                 std::initializer_list<std::string_view> allowed)
{
    for (const auto& [name, value] : values) {
        if (value.defaulted() || name == "command" || name == "file") {
            continue;
        }
        if (std::ranges::find(allowed, name) == allowed.end()) {
            return makeError(ErrorCode::InvalidArgument,
                             std::format("opcja --{} nie dotyczy polecenia '{}'", name, command));
        }
    }
    return {};
}

Result<std::uint32_t> parseId(std::string_view text)
{
    std::uint32_t id = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), id);
    if (ec != std::errc{} || ptr != text.data() + text.size() || id == 0) {
        return makeError(ErrorCode::InvalidArgument,
                         std::format("niepoprawny identyfikator '{}' (oczekiwano liczby dodatniej)", text));
    }
    return id;
}

template <typename T, typename Parser>
Result<std::optional<T>> parseOptional(const po::variables_map& values, const std::string& name, Parser parser)
{
    const auto text = optionalValue(values, name);
    if (!text) {
        return std::optional<T>{};
    }
    return parser(*text).transform([](T value) { return std::optional<T>{std::move(value)}; });
}

Result<Command> parseAdd(const po::variables_map& values)
{
    if (auto allowed = checkAllowedOptions(values, "add", {"type", "amount", "category", "date", "note"}); !allowed) {
        return std::unexpected(allowed.error());
    }

    const auto amount = requiredValue(values, "amount", "add").and_then(Money::parse);
    if (!amount) {
        return std::unexpected(amount.error());
    }
    const auto category = requiredValue(values, "category", "add");
    if (!category) {
        return std::unexpected(category.error());
    }
    const auto type = parseOptional<TransactionType>(values, "type", parseTransactionType);
    if (!type) {
        return std::unexpected(type.error());
    }
    const auto date = parseOptional<Date>(values, "date", parseDate);
    if (!date) {
        return std::unexpected(date.error());
    }

    Transaction transaction;
    transaction.type = type->value_or(TransactionType::Expense);
    transaction.amount = *amount;
    transaction.category = *category;
    transaction.date = date->value_or(today());
    transaction.note = optionalValue(values, "note").value_or("");
    return AddCommand{std::move(transaction)};
}

Result<Command> parseEdit(const po::variables_map& values)
{
    if (auto allowed = checkAllowedOptions(values, "edit", {"id", "type", "amount", "category", "date", "note"});
        !allowed) {
        return std::unexpected(allowed.error());
    }

    const auto id = requiredValue(values, "id", "edit").and_then(parseId);
    if (!id) {
        return std::unexpected(id.error());
    }
    const auto type = parseOptional<TransactionType>(values, "type", parseTransactionType);
    if (!type) {
        return std::unexpected(type.error());
    }
    const auto amount = parseOptional<Money>(values, "amount", Money::parse);
    if (!amount) {
        return std::unexpected(amount.error());
    }
    const auto date = parseOptional<Date>(values, "date", parseDate);
    if (!date) {
        return std::unexpected(date.error());
    }

    TransactionChanges changes;
    changes.type = *type;
    changes.amount = *amount;
    changes.date = *date;
    changes.category = optionalValue(values, "category");
    changes.note = optionalValue(values, "note");
    if (changes.empty()) {
        return makeError(ErrorCode::InvalidArgument,
                         "polecenie 'edit' wymaga co najmniej jednej zmiany "
                         "(--type, --amount, --category, --date, --note)");
    }
    return EditCommand{*id, std::move(changes)};
}

Result<Command> parseList(const po::variables_map& values)
{
    if (auto allowed = checkAllowedOptions(values, "list", {"type", "category", "month", "year"}); !allowed) {
        return std::unexpected(allowed.error());
    }

    const auto type = parseOptional<TransactionType>(values, "type", parseTransactionType);
    if (!type) {
        return std::unexpected(type.error());
    }
    const auto month = parseOptional<Month>(values, "month", parseMonth);
    if (!month) {
        return std::unexpected(month.error());
    }
    const auto year = parseOptional<Year>(values, "year", parseYear);
    if (!year) {
        return std::unexpected(year.error());
    }

    Filter filter;
    filter.type = *type;
    filter.month = *month;
    filter.year = *year;
    filter.category = optionalValue(values, "category").transform(normalizeCategory);
    return ListCommand{std::move(filter)};
}

Result<std::optional<Period>> parsePeriod(const po::variables_map& values)
{
    const auto toPeriod = [](auto value) { return std::optional<Period>{value}; };
    if (values.contains("month")) {
        return parseMonth(values["month"].as<std::string>()).transform(toPeriod);
    }
    if (values.contains("year")) {
        return parseYear(values["year"].as<std::string>()).transform(toPeriod);
    }
    return std::optional<Period>{};
}

Result<Command> parseRemove(const po::variables_map& values)
{
    if (auto allowed = checkAllowedOptions(values, "remove", {"id", "month", "year"}); !allowed) {
        return std::unexpected(allowed.error());
    }

    const auto given = std::ranges::count_if(std::initializer_list<std::string_view>{"id", "month", "year"},
                                             [&values](std::string_view name) {
                                                 return values.contains(std::string{name});
                                             });
    if (given != 1) {
        return makeError(ErrorCode::InvalidArgument,
                         "polecenie 'remove' wymaga dokładnie jednej z opcji --id, --month, --year");
    }

    if (values.contains("id")) {
        return parseId(values["id"].as<std::string>()).transform([](std::uint32_t id) -> Command {
            return RemoveCommand{id};
        });
    }
    return parsePeriod(values).transform([](std::optional<Period> period) -> Command {
        return RemovePeriodCommand{*period};
    });
}

Result<Command> parseSummary(const po::variables_map& values)
{
    if (auto allowed = checkAllowedOptions(values, "summary", {"month", "year"}); !allowed) {
        return std::unexpected(allowed.error());
    }
    if (values.contains("month") && values.contains("year")) {
        return makeError(ErrorCode::InvalidArgument,
                         "polecenie 'summary' przyjmuje tylko jedną z opcji --month, --year");
    }
    return parsePeriod(values).transform([](std::optional<Period> period) -> Command {
        return SummaryCommand{period};
    });
}

Result<Command> parseCommand(const std::string& name, const po::variables_map& values)
{
    if (name == "help") {
        return HelpCommand{};
    }
    if (name == "add") {
        return parseAdd(values);
    }
    if (name == "list") {
        return parseList(values);
    }
    if (name == "remove") {
        return parseRemove(values);
    }
    if (name == "edit") {
        return parseEdit(values);
    }
    if (name == "summary") {
        return parseSummary(values);
    }
    return makeError(ErrorCode::InvalidArgument, std::format("nieznane polecenie '{}'", name));
}

}

Result<Options> parseArguments(const std::vector<std::string>& arguments)
{
    po::options_description general{"Opcje ogólne"};
    po::options_description commands{"Opcje poleceń"};
    describeOptions(general, commands);

    po::options_description hidden;
    hidden.add_options()("command", po::value<std::string>());

    po::options_description all;
    all.add(general).add(commands).add(hidden);

    po::positional_options_description positional;
    positional.add("command", 1);

    po::variables_map values;
    try {
        po::store(po::command_line_parser(arguments).options(all).positional(positional).run(), values);
        po::notify(values);
    } catch (const po::error& e) {
        return makeError(ErrorCode::InvalidArgument, e.what());
    }

    const auto& file = values["file"].as<std::string>();
    Options options{.dataFile = std::filesystem::path{std::u8string(file.begin(), file.end())},
                    .command = HelpCommand{}};
    if (values.contains("help") || arguments.empty()) {
        return options;
    }
    if (!values.contains("command")) {
        return makeError(ErrorCode::InvalidArgument,
                         "nie podano polecenia (dostępne: add, edit, list, remove, summary, help), "
                         "np. 'skarbonka list --file dane.json'");
    }

    return parseCommand(values["command"].as<std::string>(), values).transform([&options](Command command) {
        options.command = std::move(command);
        return std::move(options);
    });
}

std::string helpText()
{
    po::options_description general{"Opcje ogólne"};
    po::options_description commands{"Opcje poleceń"};
    describeOptions(general, commands);

    std::ostringstream text;
    text << "Skarbonka - konsolowy menedżer budżetu domowego.\n"
            "Zapisuje wydatki i przychody w pliku JSON, wyświetla je z filtrami\n"
            "i przygotowuje podsumowania: miesięczne, roczne i za cały okres.\n\n"
            "Użycie:\n"
            "  skarbonka <polecenie> [opcje]\n\n"
            "Polecenia:\n"
            "  add       dodaje transakcję (wymagane: --amount, --category;\n"
            "            opcjonalne: --type, --date, --note)\n"
            "  edit      zmienia transakcję (wymagane: --id oraz co najmniej jedna\n"
            "            z opcji --type, --amount, --category, --date, --note)\n"
            "  list      wyświetla transakcje (filtry: --type, --category, --month, --year)\n"
            "  remove    usuwa transakcję (--id) albo wszystkie transakcje\n"
            "            z danego miesiąca (--month) lub roku (--year)\n"
            "  summary   podsumowanie przychodów i wydatków (opcjonalnie: --month\n"
            "            albo --year; bez nich - cały okres)\n"
            "  help      wyświetla tę pomoc\n\n"
         << general << '\n'
         << commands << '\n'
         << "Przykłady:\n"
            "  skarbonka add --amount 45.90 --category jedzenie --note \"zakupy\"\n"
            "  skarbonka add --type income --amount 5200 --category pensja --date 2026-10-01\n"
            "  skarbonka list --month 2026-10 --type expense\n"
            "  skarbonka summary --month 2026-10\n"
            "  skarbonka summary --year 2026\n"
            "  skarbonka edit --id 3 --amount 49.90 --note \"zakupy w markecie\"\n"
            "  skarbonka remove --id 3\n"
            "  skarbonka remove --month 2026-10\n"
            "  skarbonka remove --year 2025\n"
            "  skarbonka list --file examples/budget.json\n";
    return text.str();
}

}

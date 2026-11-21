#include "skarbonka/Storage.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <format>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <system_error>
#include <vector>

namespace skarbonka {

namespace {

using nlohmann::json;

constexpr int fileFormatVersion = 1;

json toJson(const Transaction& transaction)
{
    return json{
        {"id", transaction.id},
        {"type", toString(transaction.type)},
        {"amount", transaction.amount.toString()},
        {"category", transaction.category},
        {"date", formatDate(transaction.date)},
        {"note", transaction.note},
    };
}

std::optional<std::uint32_t> toUint32(const json& value)
{
    if (!value.is_number_unsigned() || value.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value.get<std::uint64_t>());
}

Result<Transaction> transactionFromJson(const json& object)
{
    const auto id = toUint32(object.at("id"));
    if (!id) {
        return makeError(ErrorCode::ParseError, "identyfikator transakcji musi być liczbą całkowitą nieujemną");
    }

    Transaction transaction;
    transaction.id = *id;
    transaction.category = object.at("category").get<std::string>();
    transaction.note = object.value("note", std::string{});

    auto type = parseTransactionType(object.at("type").get<std::string>());
    auto amount = Money::parse(object.at("amount").get<std::string>());
    auto date = parseDate(object.at("date").get<std::string>());
    if (!type) {
        return std::unexpected(type.error());
    }
    if (!amount) {
        return std::unexpected(amount.error());
    }
    if (!date) {
        return std::unexpected(date.error());
    }
    transaction.type = *type;
    transaction.amount = *amount;
    transaction.date = *date;
    return transaction;
}

std::string displayPath(const std::filesystem::path& path)
{
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

Error withContext(Error error, std::string_view context)
{
    error.message = std::format("{}: {}", context, error.message);
    return error;
}

}

std::string serialize(const Budget& budget)
{
    json transactions = json::array();
    for (const auto& transaction : budget.transactions()) {
        transactions.push_back(toJson(transaction));
    }
    const json document{
        {"version", fileFormatVersion},
        {"nextId", budget.nextId()},
        {"transactions", std::move(transactions)},
    };
    return document.dump(2) + '\n';
}

Result<Budget> deserialize(std::string_view text)
{
    try {
        const json document = json::parse(text);
        if (const int version = document.at("version").get<int>(); version != fileFormatVersion) {
            return makeError(ErrorCode::ParseError, std::format("nieobsługiwana wersja pliku: {}", version));
        }

        std::vector<Transaction> transactions;
        for (const auto& item : document.at("transactions")) {
            auto transaction = transactionFromJson(item);
            if (!transaction) {
                const auto id = item.contains("id") ? item.at("id").dump() : std::string{"?"};
                return std::unexpected(withContext(transaction.error(), std::format("transakcja #{}", id)));
            }
            transactions.push_back(std::move(*transaction));
        }

        std::uint32_t nextId = 1;
        if (document.contains("nextId")) {
            const auto value = toUint32(document.at("nextId"));
            if (!value) {
                return makeError(ErrorCode::ParseError, "pole nextId musi być liczbą całkowitą nieujemną");
            }
            nextId = *value;
        }
        return Budget::fromTransactions(std::move(transactions), nextId);
    } catch (const json::exception& e) {
        return makeError(ErrorCode::ParseError, std::format("niepoprawny format danych: {}", e.what()));
    }
}

Result<Budget> loadBudget(const std::filesystem::path& path, IfMissing ifMissing)
{
    std::error_code ec;
    const bool fileExists = std::filesystem::exists(path, ec);
    if (ec) {
        return makeError(ErrorCode::IoError,
                         std::format("nie można sprawdzić pliku '{}': {}", displayPath(path), ec.message()));
    }
    if (!fileExists) {
        if (ifMissing == IfMissing::CreateEmpty) {
            return Budget{};
        }
        return makeError(ErrorCode::NotFound,
                         std::format("plik '{}' nie istnieje (sprawdź ścieżkę; nowy plik tworzy polecenie add)",
                                     displayPath(path)));
    }

    std::ifstream file{path, std::ios::binary};
    if (!file) {
        return makeError(ErrorCode::IoError, std::format("nie można otworzyć pliku '{}'", displayPath(path)));
    }
    std::ostringstream content;
    content << file.rdbuf();

    return deserialize(content.str()).transform_error([&path](Error error) {
        return withContext(std::move(error), std::format("plik '{}'", displayPath(path)));
    });
}

Result<void> saveBudget(const Budget& budget, const std::filesystem::path& path)
{
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file{temporary, std::ios::binary | std::ios::trunc};
        file << serialize(budget);
        file.close();
        if (!file) {
            std::filesystem::remove(temporary, ec);
            return makeError(ErrorCode::IoError,
                             std::format("nie można zapisać pliku '{}'", displayPath(temporary)));
        }
    }

    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return makeError(ErrorCode::IoError,
                         std::format("nie można zapisać pliku '{}': {}", displayPath(path), ec.message()));
    }
    return {};
}

}

#pragma once

#include "skarbonka/Date.hpp"
#include "skarbonka/Error.hpp"
#include "skarbonka/Filter.hpp"
#include "skarbonka/Transaction.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace skarbonka {

struct HelpCommand {};

struct AddCommand {
    Transaction transaction;
};

struct ListCommand {
    Filter filter;
};

struct RemoveCommand {
    std::uint32_t id{0};
};

using Period = std::variant<Month, Year>;

struct RemovePeriodCommand {
    Period period;
};

struct EditCommand {
    std::uint32_t id{0};
    TransactionChanges changes;
};

struct SummaryCommand {
    std::optional<Period> period;
};

using Command = std::variant<HelpCommand, AddCommand, ListCommand, RemoveCommand, RemovePeriodCommand, EditCommand,
                             SummaryCommand>;

struct Options {
    std::filesystem::path dataFile;
    Command command;
};

inline constexpr auto defaultDataFile = "skarbonka.json";

[[nodiscard]] Result<Options> parseArguments(const std::vector<std::string>& arguments);

[[nodiscard]] std::string helpText();

}

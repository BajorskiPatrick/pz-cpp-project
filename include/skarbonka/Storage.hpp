#pragma once

#include "skarbonka/Budget.hpp"
#include "skarbonka/Error.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace skarbonka {

[[nodiscard]] std::string serialize(const Budget& budget);
[[nodiscard]] Result<Budget> deserialize(std::string_view json);

enum class IfMissing {
    CreateEmpty,
    Fail,
};

[[nodiscard]] Result<Budget> loadBudget(const std::filesystem::path& path,
                                        IfMissing ifMissing = IfMissing::CreateEmpty);

[[nodiscard]] Result<void> saveBudget(const Budget& budget, const std::filesystem::path& path);

}

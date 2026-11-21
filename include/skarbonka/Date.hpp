#pragma once

#include "skarbonka/Error.hpp"

#include <chrono>
#include <string>
#include <string_view>

namespace skarbonka {

using Date = std::chrono::year_month_day;
using Month = std::chrono::year_month;
using Year = std::chrono::year;

[[nodiscard]] Result<Date> parseDate(std::string_view text);

[[nodiscard]] Result<Month> parseMonth(std::string_view text);

[[nodiscard]] Result<Year> parseYear(std::string_view text);

[[nodiscard]] std::string formatDate(const Date& date);
[[nodiscard]] std::string formatMonth(const Month& month);
[[nodiscard]] std::string formatYear(const Year& year);

[[nodiscard]] constexpr Month monthOf(const Date& date) noexcept
{
    return Month{date.year(), date.month()};
}

[[nodiscard]] Date today();

}

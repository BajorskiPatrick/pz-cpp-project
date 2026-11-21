#include "skarbonka/Date.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>

namespace skarbonka {

namespace {

std::optional<int> parseFixedNumber(std::string_view text, std::size_t digits)
{
    if (text.size() != digits || !std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; })) {
        return std::nullopt;
    }
    int value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

}

Result<Date> parseDate(std::string_view text)
{
    const auto fail = [text](std::string_view reason) {
        return makeError(ErrorCode::InvalidDate, std::format("niepoprawna data '{}': {}", text, reason));
    };

    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return fail("oczekiwano formatu RRRR-MM-DD");
    }

    const auto y = parseFixedNumber(text.substr(0, 4), 4);
    const auto m = parseFixedNumber(text.substr(5, 2), 2);
    const auto d = parseFixedNumber(text.substr(8, 2), 2);
    if (!y || !m || !d) {
        return fail("oczekiwano formatu RRRR-MM-DD");
    }

    const Date date{std::chrono::year{*y}, std::chrono::month{static_cast<unsigned>(*m)},
                    std::chrono::day{static_cast<unsigned>(*d)}};
    if (!date.ok()) {
        return fail("taki dzień nie istnieje");
    }
    return date;
}

Result<Month> parseMonth(std::string_view text)
{
    const auto fail = [text] {
        return makeError(ErrorCode::InvalidDate, std::format("niepoprawny miesiąc '{}' (oczekiwano RRRR-MM)", text));
    };

    if (text.size() != 7 || text[4] != '-') {
        return fail();
    }
    const auto y = parseFixedNumber(text.substr(0, 4), 4);
    const auto m = parseFixedNumber(text.substr(5, 2), 2);
    if (!y || !m) {
        return fail();
    }

    const Month month{std::chrono::year{*y}, std::chrono::month{static_cast<unsigned>(*m)}};
    if (!month.ok()) {
        return fail();
    }
    return month;
}

Result<Year> parseYear(std::string_view text)
{
    const auto year = parseFixedNumber(text, 4);
    if (!year) {
        return makeError(ErrorCode::InvalidDate, std::format("niepoprawny rok '{}' (oczekiwano RRRR)", text));
    }
    return Year{*year};
}

std::string formatDate(const Date& date)
{
    return std::format("{:%Y-%m-%d}", date);
}

std::string formatMonth(const Month& month)
{
    return std::format("{:%Y-%m}", month);
}

std::string formatYear(const Year& year)
{
    return std::format("{:%Y}", year);
}

Date today()
{
    const auto now = std::chrono::system_clock::now();
    try {
        const std::chrono::zoned_time local{std::chrono::current_zone(), now};
        return Date{std::chrono::floor<std::chrono::days>(local.get_local_time())};
    } catch (const std::runtime_error&) {
        return Date{std::chrono::floor<std::chrono::days>(now)};
    }
}

}

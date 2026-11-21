#include "skarbonka/Money.hpp"

#include <algorithm>
#include <charconv>
#include <limits>

namespace skarbonka {

namespace {

bool isDigits(std::string_view text)
{
    return !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

bool toInteger(std::string_view text, std::int64_t& value)
{
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && ptr == text.data() + text.size();
}

}

Result<Money> Money::parse(std::string_view text)
{
    const auto fail = [text] {
        return makeError(ErrorCode::InvalidAmount,
                         std::format("niepoprawna kwota '{}' (oczekiwano np. 12.50)", text));
    };

    std::string_view rest = text;
    const bool negative = rest.starts_with('-');
    if (negative) {
        rest.remove_prefix(1);
    }

    const auto separator = rest.find_first_of(".,");
    const auto wholePart = rest.substr(0, separator);
    const auto fractionPart = separator == std::string_view::npos ? std::string_view{} : rest.substr(separator + 1);

    if (!isDigits(wholePart)) {
        return fail();
    }
    if (separator != std::string_view::npos && (!isDigits(fractionPart) || fractionPart.size() > 2)) {
        return fail();
    }

    constexpr std::int64_t maxWhole = std::numeric_limits<std::int64_t>::max() / 100 - 1;

    std::int64_t whole = 0;
    if (!toInteger(wholePart, whole) || whole > maxWhole) {
        return fail();
    }

    std::int64_t fraction = 0;
    if (!fractionPart.empty()) {
        if (!toInteger(fractionPart, fraction)) {
            return fail();
        }
        if (fractionPart.size() == 1) {
            fraction *= 10;
        }
    }

    const std::int64_t cents = whole * 100 + fraction;
    return Money{negative ? -cents : cents};
}

std::string Money::toString() const
{
    const std::int64_t absolute = cents_ < 0 ? -cents_ : cents_;
    return std::format("{}{}.{:02}", cents_ < 0 ? "-" : "", absolute / 100, absolute % 100);
}

}

#pragma once

#include "skarbonka/Error.hpp"

#include <compare>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace skarbonka {

class Money {
public:
    constexpr Money() noexcept = default;

    [[nodiscard]] static constexpr Money fromCents(std::int64_t cents) noexcept { return Money{cents}; }

    [[nodiscard]] static Result<Money> parse(std::string_view text);

    [[nodiscard]] constexpr std::int64_t cents() const noexcept { return cents_; }

    [[nodiscard]] std::string toString() const;

    constexpr Money& operator+=(Money other) noexcept
    {
        cents_ += other.cents_;
        return *this;
    }

    constexpr Money& operator-=(Money other) noexcept
    {
        cents_ -= other.cents_;
        return *this;
    }

    [[nodiscard]] friend constexpr Money operator+(Money lhs, Money rhs) noexcept { return lhs += rhs; }
    [[nodiscard]] friend constexpr Money operator-(Money lhs, Money rhs) noexcept { return lhs -= rhs; }
    [[nodiscard]] constexpr Money operator-() const noexcept { return Money{-cents_}; }

    constexpr auto operator<=>(const Money&) const noexcept = default;

private:
    explicit constexpr Money(std::int64_t cents) noexcept : cents_{cents} {}

    std::int64_t cents_{0};
};

}

template <>
struct std::formatter<skarbonka::Money> : std::formatter<std::string> {
    auto format(const skarbonka::Money& money, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(money.toString(), ctx);
    }
};

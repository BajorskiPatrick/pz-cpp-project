#pragma once

#include <expected>
#include <string>

namespace skarbonka {

enum class ErrorCode {
    InvalidAmount,
    InvalidDate,
    InvalidArgument,
    NotFound,
    IoError,
    ParseError,
};

struct Error {
    ErrorCode code;
    std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] inline std::unexpected<Error> makeError(ErrorCode code, std::string message)
{
    return std::unexpected(Error{code, std::move(message)});
}

}

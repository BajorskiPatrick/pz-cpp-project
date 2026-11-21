#include "skarbonka/App.hpp"
#include "skarbonka/Cli.hpp"

#include <exception>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <memory>
#include <string_view>
#endif

namespace {

#ifdef _WIN32

std::string toUtf8(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    const auto length = static_cast<int>(text.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), size, nullptr, nullptr);
    return result;
}

std::vector<std::string> commandLineArguments(int /*argc*/, char* /*argv*/[])
{
    struct LocalFreeDeleter {
        void operator()(LPWSTR* pointer) const noexcept { LocalFree(pointer); }
    };

    int count = 0;
    const std::unique_ptr<LPWSTR[], LocalFreeDeleter> wide{CommandLineToArgvW(GetCommandLineW(), &count)};
    if (!wide) {
        return {};
    }
    return std::span(wide.get(), static_cast<std::size_t>(count))
         | std::views::drop(1)
         | std::views::transform([](LPCWSTR argument) { return toUtf8(argument); })
         | std::ranges::to<std::vector>();
}

class Utf8ConsoleOutput {
public:
    Utf8ConsoleOutput() : previous_{GetConsoleOutputCP()} { SetConsoleOutputCP(CP_UTF8); }

    ~Utf8ConsoleOutput()
    {
        if (previous_ != 0) {
            std::cout.flush();
            std::fflush(stdout);
            SetConsoleOutputCP(previous_);
        }
    }

    Utf8ConsoleOutput(const Utf8ConsoleOutput&) = delete;
    Utf8ConsoleOutput& operator=(const Utf8ConsoleOutput&) = delete;

private:
    UINT previous_;
};

#else

std::vector<std::string> commandLineArguments(int argc, char* argv[])
{
    return std::span(argv, static_cast<std::size_t>(argc))
         | std::views::drop(1)
         | std::ranges::to<std::vector<std::string>>();
}

#endif

}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    const Utf8ConsoleOutput utf8Console;
#endif

    try {
        const auto arguments = commandLineArguments(argc, argv);

        const auto result = skarbonka::parseArguments(arguments).and_then(
            [](const skarbonka::Options& options) { return skarbonka::run(options, std::cout); });

        if (!result) {
            std::cerr << "Błąd: " << result.error().message << '\n'
                      << "Uruchom 'skarbonka --help', aby zobaczyć listę poleceń.\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nieoczekiwany błąd: " << e.what() << '\n';
        return 2;
    }
}

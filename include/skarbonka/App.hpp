#pragma once

#include "skarbonka/Cli.hpp"
#include "skarbonka/Error.hpp"

#include <ostream>

namespace skarbonka {

[[nodiscard]] Result<void> run(const Options& options, std::ostream& out);

}

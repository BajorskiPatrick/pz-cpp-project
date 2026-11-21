#include "skarbonka/Filter.hpp"

namespace skarbonka {

bool Filter::matches(const Transaction& transaction) const
{
    const bool typeMatches = !type || transaction.type == *type;
    const bool categoryMatches = !category || transaction.category == *category;
    const bool monthMatches = !month || monthOf(transaction.date) == *month;
    const bool yearMatches = !year || transaction.date.year() == *year;
    return typeMatches && categoryMatches && monthMatches && yearMatches;
}

}

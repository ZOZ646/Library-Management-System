#pragma once

#include <stdexcept>
#include <string>

namespace titans {

// Thrown on database errors (SQL syntax, constraints, missing records, corruption).
class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace titans

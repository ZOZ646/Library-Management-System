#pragma once

#include <stdexcept>
#include <string>

namespace titans {

// Thrown when a book or member is not found in the service.
class NotFoundError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown when a duplicate book ISBN or member id is added.
class DuplicateError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace titans

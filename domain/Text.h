#pragma once

#include <cctype>
#include <string>
#include <stdexcept>

namespace titans::text {

// Removes leading and trailing whitespace; keeps inner whitespace untouched.
inline std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = s.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(start, end);
}

// Returns trimmed value; throws std::invalid_argument if the trimmed string is empty.
inline std::string require_non_blank(const std::string& value, const char* field_name) {
    std::string trimmed = trim(value);
    if (trimmed.empty()) {
        throw std::invalid_argument(std::string(field_name) + " must not be empty");
    }
    return trimmed;
}

} // namespace titans::text

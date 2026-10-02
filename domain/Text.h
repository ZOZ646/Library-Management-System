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

// Case folding is ASCII-only: std::tolower(static_cast<unsigned char>(c)).
// Bytes >= 128 (e.g. UTF-8 multi-byte sequences such as Arabic) are left
// untouched and compare by raw byte value. The program never changes the locale.

// Returns a new string with every ASCII letter lowered.
inline std::string to_lower(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// Lexicographic comparison of the ASCII-folded bytes, without allocating.
// Returns a negative value, 0, or a positive value.
// If one string is a prefix of the other the shorter one is smaller.
inline int compare_ignore_case(const std::string& a, const std::string& b) noexcept {
    std::size_t len = (a.size() < b.size()) ? a.size() : b.size();
    for (std::size_t i = 0; i < len; ++i) {
        int ca = std::tolower(static_cast<unsigned char>(a[i]));
        int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) {
            return ca - cb;
        }
    }
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    return 0;
}

// Returns true if a and b are equal ignoring ASCII case.
inline bool equals_ignore_case(const std::string& a, const std::string& b) noexcept {
    return compare_ignore_case(a, b) == 0;
}

// Returns true if part occurs inside text ignoring ASCII case.
// An empty part is contained in everything.
inline bool contains_ignore_case(const std::string& text, const std::string& part) {
    if (part.empty()) {
        return true;
    }
    if (part.size() > text.size()) {
        return false;
    }
    for (std::size_t i = 0; i <= text.size() - part.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < part.size(); ++j) {
            int ct = std::tolower(static_cast<unsigned char>(text[i + j]));
            int cp = std::tolower(static_cast<unsigned char>(part[j]));
            if (ct != cp) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

} // namespace titans::text

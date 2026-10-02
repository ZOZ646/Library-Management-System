#pragma once

#include <cstddef>
#include <string>

namespace titans::utf8 {

// Limitation: The display width is calculated as the number of UTF-8 code points.
// Combining marks, double-width characters, and right-to-left shaping are not handled.

// Returns the number of UTF-8 code points in the string.
// Every code point has exactly one non-continuation byte ((b & 0xC0) != 0x80).
inline std::size_t display_width(const std::string& s) noexcept {
    std::size_t count = 0;
    for (unsigned char b : s) {
        if ((b & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

// Truncates string to max_width display width.
// If display_width(s) <= max_width, returns s unchanged.
// If max_width >= 4, returns the first (max_width - 3) code points followed by "...".
// If max_width < 4, returns the first max_width code points (no ellipsis).
// Cuts are always on code point boundaries.
inline std::string truncate_display(const std::string& s, std::size_t max_width) {
    if (display_width(s) <= max_width) {
        return s;
    }

    std::size_t target_code_points = (max_width >= 4) ? (max_width - 3) : max_width;
    if (target_code_points == 0) {
        return (max_width >= 4) ? "..." : "";
    }

    std::size_t cp_count = 0;
    std::size_t byte_pos = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        unsigned char b = static_cast<unsigned char>(s[i]);
        if ((b & 0xC0) != 0x80) {
            if (cp_count == target_code_points) {
                byte_pos = i;
                break;
            }
            ++cp_count;
        }
    }
    if (cp_count == target_code_points && byte_pos == 0) {
        byte_pos = s.size();
    }

    std::string result = s.substr(0, byte_pos);
    if (max_width >= 4) {
        result += "...";
    }
    return result;
}

// Appends spaces until display_width reaches width; returns s unchanged if already wide enough.
inline std::string pad_right(const std::string& s, std::size_t width) {
    std::size_t w = display_width(s);
    if (w >= width) {
        return s;
    }
    return s + std::string(width - w, ' ');
}

} // namespace titans::utf8

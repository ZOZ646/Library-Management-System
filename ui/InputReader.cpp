#include "ui/InputReader.h"
#include "domain/Book.h"
#include "domain/Text.h"

#include <charconv>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <system_error>

namespace titans {

InputReader::InputReader(std::istream& in, std::ostream& out)
    : in_(in), out_(out) {}

std::string InputReader::read_line(const std::string& prompt) {
    out_ << prompt;
    out_.flush();
    std::string line;
    if (!std::getline(in_, line)) {
        throw InputClosed("input closed");
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return text::trim(line);
}

std::string InputReader::read_text(const std::string& prompt, bool cancellable) {
    while (true) {
        std::string line = read_line(prompt);
        if (line.empty()) {
            out_ << "Value must not be empty.\n";
            continue;
        }
        if (cancellable && text::equals_ignore_case(line, "/cancel")) {
            throw CancelledByUser("cancelled");
        }
        return line;
    }
}

int InputReader::read_int(const std::string& prompt, int min, int max, bool cancellable) {
    if (min > max) {
        throw std::invalid_argument("min must be <= max");
    }
    while (true) {
        std::string line = read_line(prompt);
        if (cancellable && text::equals_ignore_case(line, "/cancel")) {
            throw CancelledByUser("cancelled");
        }
        if (!line.empty()) {
            int value = 0;
            const char* begin = line.data();
            const char* end = line.data() + line.size();
            auto [ptr, ec] = std::from_chars(begin, end, value);
            if (ec == std::errc() && ptr == end && value >= min && value <= max) {
                return value;
            }
        }
        out_ << "Please enter a whole number between " << min << " and " << max << ".\n";
    }
}

bool InputReader::confirm(const std::string& prompt) {
    while (true) {
        std::string line = read_line(prompt);
        if (text::equals_ignore_case(line, "/cancel")) {
            throw CancelledByUser("cancelled");
        }
        if (text::equals_ignore_case(line, "y") || text::equals_ignore_case(line, "yes")) {
            return true;
        }
        if (text::equals_ignore_case(line, "n") || text::equals_ignore_case(line, "no")) {
            return false;
        }
        out_ << "Please answer y or n.\n";
    }
}

std::string InputReader::read_isbn(const std::string& prompt) {
    while (true) {
        std::string line = read_text(prompt, true);
        if (Book::is_valid_isbn(line)) {
            return Book::normalize_isbn(line);
        }
        out_ << "Invalid ISBN: use 10 or 13 digits (hyphens and spaces are allowed).\n";
    }
}

} // namespace titans

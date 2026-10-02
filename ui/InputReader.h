#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>

namespace titans {

// Thrown when the input stream closes (EOF or error).
class InputClosed : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown when the user types "/cancel".
class CancelledByUser : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class InputReader {
public:
    InputReader(std::istream& in, std::ostream& out);

    InputReader(const InputReader&) = delete;
    InputReader& operator=(const InputReader&) = delete;
    InputReader(InputReader&&) = delete;
    InputReader& operator=(InputReader&&) = delete;

    // Reads non-empty trimmed text. Throws CancelledByUser if cancellable and "/cancel" entered.
    // Throws InputClosed if input stream closes.
    std::string read_text(const std::string& prompt, bool cancellable = true);

    // Reads an integer in [min, max]. Throws std::invalid_argument if min > max.
    // Throws CancelledByUser if cancellable and "/cancel" entered.
    // Throws InputClosed if input stream closes.
    int read_int(const std::string& prompt, int min, int max, bool cancellable = true);

    // Reads confirmation: accepts y/yes/n/no. Throws CancelledByUser on "/cancel".
    // Throws InputClosed if input stream closes.
    bool confirm(const std::string& prompt);

    // Reads a valid ISBN (10 or 13 digits). Re-prompts on invalid.
    // Returns the normalized ISBN.
    std::string read_isbn(const std::string& prompt);

private:
    std::istream& in_;
    std::ostream& out_;

    // Private helper: prints prompt, flushes out, reads line, strips '\r', trims.
    // Throws InputClosed if getline fails.
    std::string read_line(const std::string& prompt);
};

} // namespace titans

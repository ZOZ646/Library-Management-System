#include <doctest/doctest.h>
#include "ui/InputReader.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::size_t count_occurrences(const std::string& haystack, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

// ── 1. read_text ─────────────────────────────────────────────────

TEST_CASE("InputReader: read_text") {
    // Normal text trimmed
    {
        std::istringstream in("  hello world  \n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_text("Prompt: ") == "hello world");
        CHECK(out.str() == "Prompt: ");
    }

    // Skips blank lines and re-prompts
    {
        std::istringstream in("\n   \n\t\nvalid\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_text("P: ") == "valid");
        CHECK(count_occurrences(out.str(), "Value must not be empty.\n") == 3);
    }

    // Handles Windows CRLF
    {
        std::istringstream in("line1\r\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_text("P: ") == "line1");
    }

    // EOF -> InputClosed
    {
        std::istringstream in("");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_text("P: "), titans::InputClosed);
    }

    // EOF after blank lines -> InputClosed
    {
        std::istringstream in("   \n\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_text("P: "), titans::InputClosed);
    }

    // /cancel when cancellable
    {
        std::istringstream in("  /CANCEL  \n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_text("P: ", true), titans::CancelledByUser);
    }

    // /cancel when not cancellable
    {
        std::istringstream in("/cancel\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_text("P: ", false) == "/cancel");
    }

    // UTF-8 text passes through unchanged
    {
        std::string arabic = "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8";
        std::istringstream in(arabic + "\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_text("P: ") == arabic);
    }
}

// ── 2. read_int ──────────────────────────────────────────────────

TEST_CASE("InputReader: read_int") {
    // Valid values and bounds
    {
        std::istringstream in("5\n0\n10\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_int("P: ", 0, 10) == 5);
        CHECK(reader.read_int("P: ", 0, 10) == 0);
        CHECK(reader.read_int("P: ", 0, 10) == 10);
    }

    // Padded integer
    {
        std::istringstream in("  7  \n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_int("P: ", 0, 10) == 7);
    }

    // Negative range
    {
        std::istringstream in("-3\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_int("P: ", -5, -1) == -3);
    }

    // Rejections followed by valid input
    {
        std::vector<std::string> rejected = {
            "abc", "12abc", "3.5", "+5", "   ", "-1", "11", "99999999999999999999"
        };
        for (const auto& rej : rejected) {
            std::istringstream in(rej + "\n5\n");
            std::ostringstream out;
            titans::InputReader reader(in, out);
            int val = reader.read_int("P: ", 0, 10);
            CHECK(val == 5);
            CHECK(count_occurrences(out.str(), "Please enter a whole number between 0 and 10.\n") == 1);
        }
    }

    // EOF in the middle of re-prompting
    {
        std::istringstream in("abc\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_int("P: ", 0, 10), titans::InputClosed);
    }

    // /cancel cancels when cancellable
    {
        std::istringstream in("/cancel\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_int("P: ", 0, 10, true), titans::CancelledByUser);
    }

    // /cancel rejected when not cancellable
    {
        std::istringstream in("/cancel\n4\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_int("P: ", 0, 10, false) == 4);
        CHECK(count_occurrences(out.str(), "Please enter a whole number between 0 and 10.\n") == 1);
    }

    // min > max throws std::invalid_argument
    {
        std::istringstream in("5\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_int("P: ", 10, 5), std::invalid_argument);
    }
}

// ── 3. confirm ───────────────────────────────────────────────────

TEST_CASE("InputReader: confirm") {
    // Yes variants
    for (const auto& str : {"y", "Y", "yes", "YES", "  yes  "}) {
        std::istringstream in(std::string(str) + "\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.confirm("Confirm: ") == true);
    }

    // No variants
    for (const auto& str : {"n", "N", "no", "NO", "  no  "}) {
        std::istringstream in(std::string(str) + "\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.confirm("Confirm: ") == false);
    }

    // Re-prompt on invalid answer
    {
        std::istringstream in("maybe\ny\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.confirm("Confirm: ") == true);
        CHECK(count_occurrences(out.str(), "Please answer y or n.\n") == 1);
    }

    // Cancel
    {
        std::istringstream in("/cancel\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.confirm("Confirm: "), titans::CancelledByUser);
    }

    // EOF
    {
        std::istringstream in("");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.confirm("Confirm: "), titans::InputClosed);
    }
}

// ── 4. read_isbn ─────────────────────────────────────────────────

TEST_CASE("InputReader: read_isbn") {
    // Hyphenated ISBN returns normalized
    {
        std::istringstream in("978-0-13-468599-1\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_isbn("ISBN: ") == "9780134685991");
    }

    // Spaced ISBN
    {
        std::istringstream in("978 0 13 468599 1\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_isbn("ISBN: ") == "9780134685991");
    }

    // Lowercase x normalized to uppercase X
    {
        std::istringstream in("0-8044-2957-x\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_isbn("ISBN: ") == "080442957X");
    }

    // Invalid writes message and re-prompts
    {
        std::istringstream in("abc\n0-8044-2957-X\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK(reader.read_isbn("ISBN: ") == "080442957X");
        CHECK(count_occurrences(out.str(), "Invalid ISBN: use 10 or 13 digits (hyphens and spaces are allowed).\n") == 1);
    }

    // Cancel
    {
        std::istringstream in("/cancel\n");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_isbn("ISBN: "), titans::CancelledByUser);
    }

    // EOF
    {
        std::istringstream in("");
        std::ostringstream out;
        titans::InputReader reader(in, out);
        CHECK_THROWS_AS(reader.read_isbn("ISBN: "), titans::InputClosed);
    }
}

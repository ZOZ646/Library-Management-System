#include <doctest/doctest.h>
#include "ui/Utf8.h"
#include "ui/Table.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace titans;
using namespace titans::utf8;

// Hex-escaped Arabic word for "book": 4 letters (2 bytes each) = 8 bytes
const std::string kArabicBook = "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8";
// 4-byte emoji (grinning face U+1F600: F0 9F 98 80)
const std::string kEmoji = "\xF0\x9F\x98\x80";

// Simple UTF-8 validity checker
bool is_valid_utf8(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size()) {
        unsigned char b = static_cast<unsigned char>(s[i]);
        std::size_t remaining = 0;
        if (b <= 0x7F) {
            remaining = 0;
        } else if ((b & 0xE0) == 0xC0) {
            remaining = 1;
        } else if ((b & 0xF0) == 0xE0) {
            remaining = 2;
        } else if ((b & 0xF8) == 0xF0) {
            remaining = 3;
        } else {
            return false; // Invalid leader byte
        }
        if (i + remaining >= s.size()) {
            return false; // Truncated sequence
        }
        for (std::size_t r = 1; r <= remaining; ++r) {
            unsigned char cb = static_cast<unsigned char>(s[i + r]);
            if ((cb & 0xC0) != 0x80) {
                return false; // Invalid continuation byte
            }
        }
        i += 1 + remaining;
    }
    return true;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

} // namespace

// ── 1. display_width ─────────────────────────────────────────────

TEST_CASE("utf8: display_width") {
    CHECK(display_width("") == 0);
    CHECK(display_width("hello") == 5);
    CHECK(display_width("ASCII 123 !@#") == 13);
    // Arabic word has 4 code points
    CHECK(display_width(kArabicBook) == 4);
    // Mixed ASCII and Arabic
    CHECK(display_width("Title: " + kArabicBook) == 7 + 4);
    // 4-byte emoji counts as 1 code point
    CHECK(display_width(kEmoji) == 1);
    CHECK(display_width("Hi " + kEmoji + "!") == 5);
}

// ── 2. truncate_display ──────────────────────────────────────────

TEST_CASE("utf8: truncate_display") {
    // Shorter or equal unchanged
    CHECK(truncate_display("hello", 5) == "hello");
    CHECK(truncate_display("hello", 10) == "hello");

    // Longer ends with "..." and has display_width == max_width
    std::string trunc1 = truncate_display("hello world", 7);
    CHECK(trunc1 == "hell...");
    CHECK(display_width(trunc1) == 7);

    // Empty string stays empty
    CHECK(truncate_display("", 5) == "");
    CHECK(truncate_display("", 0) == "");

    // Arabic text cut on code point boundary and is valid UTF-8
    // kArabicBook has 4 code points
    std::string arabic_longer = kArabicBook + kArabicBook; // 8 code points
    std::string trunc_arabic = truncate_display(arabic_longer, 6);
    CHECK(display_width(trunc_arabic) == 6);
    CHECK(is_valid_utf8(trunc_arabic));
    CHECK(trunc_arabic.size() >= 3);
    CHECK(trunc_arabic.substr(trunc_arabic.size() - 3) == "...");

    // max_width < 4 gives first N code points with no ellipsis
    CHECK(truncate_display("hello", 3) == "hel");
    CHECK(truncate_display("hello", 2) == "he");
    CHECK(truncate_display("hello", 1) == "h");
    CHECK(truncate_display("hello", 0) == "");

    // Arabic with max_width < 4
    std::string trunc_ar2 = truncate_display(kArabicBook, 2);
    CHECK(display_width(trunc_ar2) == 2);
    CHECK(is_valid_utf8(trunc_ar2));
}

// ── 3. pad_right ─────────────────────────────────────────────────

TEST_CASE("utf8: pad_right") {
    CHECK(pad_right("abc", 5) == "abc  ");
    CHECK(pad_right("abc", 3) == "abc");
    CHECK(pad_right("abc", 2) == "abc");
    CHECK(pad_right("", 3) == "   ");

    // Arabic padding
    std::string padded_arabic = pad_right(kArabicBook, 7);
    CHECK(display_width(padded_arabic) == 7);
    CHECK(padded_arabic == kArabicBook + "   ");
}

// ── 4. Table ─────────────────────────────────────────────────────

TEST_CASE("Table: exact example from prompt") {
    Table table;
    table.add_column("ID", 4);
    table.add_column("Name", 10);
    table.add_row({"1", "Ali"});

    std::ostringstream out;
    table.print(out);

    std::string expected =
        "+----+------+\n"
        "| ID | Name |\n"
        "+----+------+\n"
        "| 1  | Ali  |\n"
        "+----+------+\n";

    CHECK(out.str() == expected);
}

TEST_CASE("Table: column growth and truncation") {
    Table table;
    table.add_column("Col", 10);
    table.add_row({"short"});
    table.add_row({"very long cell that exceeds maximum"});

    std::ostringstream out;
    table.print(out);

    // Column max width is 10, so cell is truncated to 10 ("very lo...")
    std::string res = out.str();
    CHECK(res.find("very lo...") != std::string::npos);
}

TEST_CASE("Table: uniform line display width for ASCII and Arabic") {
    Table table;
    table.add_column("ID", 6);
    table.add_column("Title", 15);
    table.add_column("Status", 10);

    table.add_row({"1", "English Book", "Available"});
    table.add_row({"2", kArabicBook, "Borrowed"});
    table.add_row({"3", "Long " + kArabicBook + " Title", "Available"});

    std::ostringstream out;
    table.print(out);

    auto lines = split_lines(out.str());
    CHECK(lines.size() == 7); // border, header, border, 3 rows, border

    std::size_t expected_width = display_width(lines[0]);
    for (const auto& line : lines) {
        CHECK(display_width(line) == expected_width);
    }
}

TEST_CASE("Table: zero columns and no rows") {
    // Zero columns prints nothing
    Table empty_table;
    std::ostringstream out1;
    empty_table.print(out1);
    CHECK(out1.str().empty());

    // Columns but no rows: border, header, border, border
    Table no_rows;
    no_rows.add_column("A", 5);
    no_rows.add_column("B", 5);
    std::ostringstream out2;
    no_rows.print(out2);

    std::string expected =
        "+---+---+\n"
        "| A | B |\n"
        "+---+---+\n"
        "+---+---+\n";
    CHECK(out2.str() == expected);
    CHECK(no_rows.row_count() == 0);
}

TEST_CASE("Table: error cases") {
    Table table;
    CHECK_THROWS_WITH_AS(table.add_column("Bad", 0),
                         "column max width must be at least 1", std::invalid_argument);

    table.add_column("Valid", 5);
    table.add_row({"one"});
    CHECK(table.row_count() == 1);

    // Cannot add column after rows
    CHECK_THROWS_WITH_AS(table.add_column("Late", 5),
                         "cannot add a column after rows were added", std::logic_error);

    // Wrong cell count
    CHECK_THROWS_WITH_AS(table.add_row({"too", "many"}),
                         "row has the wrong number of cells", std::invalid_argument);
    CHECK_THROWS_WITH_AS(table.add_row({}),
                         "row has the wrong number of cells", std::invalid_argument);
}

#include <doctest/doctest.h>
#include "domain/Text.h"

#include <string>
#include <vector>
#include <cmath>

namespace {

using titans::text::to_lower;
using titans::text::compare_ignore_case;
using titans::text::equals_ignore_case;
using titans::text::contains_ignore_case;

// Arabic UTF-8 bytes for "book": \xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8
const std::string kArabicBook = "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8";

} // namespace

TEST_CASE("text::to_lower") {
    CHECK(to_lower("Hello World") == "hello world");
    CHECK(to_lower("ABC123xyz") == "abc123xyz");
    CHECK(to_lower("") == "");
    CHECK(to_lower("already lower") == "already lower");
    CHECK(to_lower("!@#$%") == "!@#$%");
    // Bytes >= 128 left untouched
    CHECK(to_lower(kArabicBook) == kArabicBook);
    CHECK(to_lower("Test" + kArabicBook + "MIX") == "test" + kArabicBook + "mix");
}

TEST_CASE("text::compare_ignore_case") {
    CHECK(compare_ignore_case("abc", "ABC") == 0);
    CHECK(compare_ignore_case("abc", "abd") < 0);
    CHECK(compare_ignore_case("abd", "abc") > 0);
    CHECK(compare_ignore_case("abc", "abcd") < 0);
    CHECK(compare_ignore_case("abcd", "abc") > 0);
    CHECK(compare_ignore_case("", "") == 0);
    CHECK(compare_ignore_case("", "a") < 0);
    CHECK(compare_ignore_case("a", "") > 0);
    CHECK(compare_ignore_case("Hello", "hello") == 0);

    // Antisymmetry
    std::vector<std::string> samples = {"apple", "Banana", "cherry", "", "APPLE", "123"};
    for (const auto& a : samples) {
        for (const auto& b : samples) {
            int ab = compare_ignore_case(a, b);
            int ba = compare_ignore_case(b, a);
            if (ab > 0) CHECK(ba < 0);
            else if (ab < 0) CHECK(ba > 0);
            else CHECK(ba == 0);
        }
    }

    // UTF-8 bytes compare by raw value
    CHECK(compare_ignore_case(kArabicBook, kArabicBook) == 0);
    CHECK(compare_ignore_case("z", kArabicBook) < 0);  // 'z' (0x7A) < 0xD9
}

TEST_CASE("text::equals_ignore_case") {
    CHECK(equals_ignore_case("Hello", "hello"));
    CHECK(equals_ignore_case("", ""));
    CHECK(!equals_ignore_case("abc", "abcd"));
    CHECK(!equals_ignore_case("abc", "xyz"));
    CHECK(equals_ignore_case("Fiction", "FICTION"));
    CHECK(equals_ignore_case("Fiction", "fiction"));
    CHECK(!equals_ignore_case("Fiction", "Non-Fiction"));
}

TEST_CASE("text::contains_ignore_case") {
    // At start
    CHECK(contains_ignore_case("Hello World", "hello"));
    // At end
    CHECK(contains_ignore_case("Hello World", "WORLD"));
    // In middle
    CHECK(contains_ignore_case("Hello World", "LO WO"));
    // Whole string
    CHECK(contains_ignore_case("Hello", "HELLO"));
    // Different case
    CHECK(contains_ignore_case("BIG apple PIE", "Apple"));
    // Not contained
    CHECK(!contains_ignore_case("Hello World", "xyz"));
    // Empty part -> always true
    CHECK(contains_ignore_case("Hello", ""));
    CHECK(contains_ignore_case("", ""));
    // Part longer than text -> false
    CHECK(!contains_ignore_case("Hi", "Hello World"));
    // Arabic substring found
    std::string arabic_title = kArabicBook + " 1";
    CHECK(contains_ignore_case(arabic_title, kArabicBook));
    CHECK(contains_ignore_case(arabic_title, "1"));
}

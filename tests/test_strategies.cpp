#include <doctest/doctest.h>
#include "service/SortStrategy.h"
#include "service/SearchStrategy.h"
#include "domain/Book.h"
#include "domain/Text.h"
#include "tests/library_fixtures.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using namespace titans;
using titans::fixtures::make_book;
using titans::fixtures::make_sample_service;

// Arabic title for testing
const std::string kArabicBook = "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8" " 1";
const std::string kArabicAuthor = "\xD9\x85\xD8\xA4\xD9\x84\xD9\x81";

} // namespace

// ── Sort Strategy Tests ──────────────────────────────────────────

TEST_CASE("sort strategy: abstract and final") {
    static_assert(std::is_abstract_v<ISortStrategy>);
    static_assert(std::is_abstract_v<ISearchStrategy>);
}

TEST_CASE("sort strategy: TitleSort") {
    TitleSort ts;
    CHECK(std::string(ts.name()) == "title");

    auto a = make_book("apple", "Author", "0000000001", "Genre");
    auto b = make_book("Banana", "Author", "0000000002", "Genre");

    // Case-insensitive: "apple" before "Banana"
    CHECK(ts.before(a, b) == true);
    CHECK(ts.before(b, a) == false);

    // Equal ignoring case -> false both directions
    auto c = make_book("APPLE", "Author", "0000000003", "Genre");
    CHECK(ts.before(a, c) == false);
    CHECK(ts.before(c, a) == false);

    // Through base pointer
    const ISortStrategy& base = ts;
    CHECK(base.before(a, b) == true);
}

TEST_CASE("sort strategy: AuthorSort") {
    AuthorSort as;
    CHECK(std::string(as.name()) == "author");

    auto a = make_book("T", "alice", "0000000001", "G");
    auto b = make_book("T", "Bob", "0000000002", "G");

    CHECK(as.before(a, b) == true);
    CHECK(as.before(b, a) == false);

    auto c = make_book("T", "ALICE", "0000000003", "G");
    CHECK(as.before(a, c) == false);
    CHECK(as.before(c, a) == false);
}

TEST_CASE("sort strategy: GenreSort") {
    GenreSort gs;
    CHECK(std::string(gs.name()) == "genre");

    auto a = make_book("T", "A", "0000000001", "fiction");
    auto b = make_book("T", "A", "0000000002", "Science");

    CHECK(gs.before(a, b) == true);
    CHECK(gs.before(b, a) == false);

    auto c = make_book("T", "A", "0000000003", "FICTION");
    CHECK(gs.before(a, c) == false);
    CHECK(gs.before(c, a) == false);
}

TEST_CASE("sort strategy: strict weak ordering properties on sample books") {
    auto svc = make_sample_service();
    std::vector<Book> books;
    for (const auto& b : svc.books()) {
        books.push_back(b);
    }

    std::vector<std::unique_ptr<ISortStrategy>> strategies;
    strategies.push_back(std::make_unique<TitleSort>());
    strategies.push_back(std::make_unique<AuthorSort>());
    strategies.push_back(std::make_unique<GenreSort>());

    for (const auto& strat : strategies) {
        // Irreflexive
        for (const auto& b : books) {
            CHECK(strat->before(b, b) == false);
        }

        // Asymmetric: if before(a, b) then !before(b, a)
        for (std::size_t i = 0; i < books.size(); ++i) {
            for (std::size_t j = 0; j < books.size(); ++j) {
                if (strat->before(books[i], books[j])) {
                    CHECK(strat->before(books[j], books[i]) == false);
                }
            }
        }

        // Transitive: if before(a, b) and before(b, c), then before(a, c)
        for (std::size_t i = 0; i < books.size(); ++i) {
            for (std::size_t j = 0; j < books.size(); ++j) {
                for (std::size_t k = 0; k < books.size(); ++k) {
                    if (strat->before(books[i], books[j]) && strat->before(books[j], books[k])) {
                        CHECK(strat->before(books[i], books[k]) == true);
                    }
                }
            }
        }
    }
}

// ── SortStrategyFactory Tests ────────────────────────────────────

TEST_CASE("SortStrategyFactory: valid keys") {
    auto ts = SortStrategyFactory::create("title");
    CHECK(std::string(ts->name()) == "title");

    auto as = SortStrategyFactory::create("Author");
    CHECK(std::string(as->name()) == "author");

    auto gs = SortStrategyFactory::create("  GENRE  ");
    CHECK(std::string(gs->name()) == "genre");

    // Verify behavior matches concrete class
    auto a = make_book("apple", "A", "0000000001", "G");
    auto b = make_book("Banana", "A", "0000000002", "G");
    CHECK(ts->before(a, b) == true);
}

TEST_CASE("SortStrategyFactory: invalid keys") {
    CHECK_THROWS_WITH_AS(SortStrategyFactory::create(""), "unknown sort criterion: ", std::invalid_argument);
    CHECK_THROWS_WITH_AS(SortStrategyFactory::create("isbn"), "unknown sort criterion: isbn", std::invalid_argument);
    CHECK_THROWS_WITH_AS(SortStrategyFactory::create("titel"), "unknown sort criterion: titel", std::invalid_argument);
}

// ── Search Strategy Tests ────────────────────────────────────────

TEST_CASE("search strategy: TitleSearch") {
    TitleSearch ts;
    CHECK(std::string(ts.name()) == "title");

    auto book = make_book("Big apple pie", "Author", "0000000001", "Genre");

    // Substring at start, middle, end, whole, different case
    CHECK(ts.matches(book, "big") == true);
    CHECK(ts.matches(book, "APPLE") == true);
    CHECK(ts.matches(book, "PIE") == true);
    CHECK(ts.matches(book, "Big apple pie") == true);
    CHECK(ts.matches(book, "bIG aPPLE pIE") == true);
    CHECK(ts.matches(book, "xyz") == false);

    // Arabic bytes found
    auto arabic = make_book(kArabicBook, kArabicAuthor, "9780000000008", "Fiction");
    CHECK(ts.matches(arabic, "\xD9\x83\xD8\xAA") == true);
}

TEST_CASE("search strategy: AuthorSearch") {
    AuthorSearch as;
    CHECK(std::string(as.name()) == "author");

    auto book = make_book("Title", "Jane Doe", "0000000001", "Genre");
    CHECK(as.matches(book, "jane") == true);
    CHECK(as.matches(book, "DOE") == true);
    CHECK(as.matches(book, "ane Do") == true);
    CHECK(as.matches(book, "Jane Doe") == true);
    CHECK(as.matches(book, "Smith") == false);
}

TEST_CASE("search strategy: GenreSearch exact match") {
    GenreSearch gs;
    CHECK(std::string(gs.name()) == "genre");

    auto fiction = make_book("T", "A", "0000000001", "Fiction");
    auto nonfiction = make_book("T", "A", "0000000002", "Non-Fiction");
    auto scifi = make_book("T", "A", "0000000003", "Science Fiction");

    // Exact (case-insensitive) match
    CHECK(gs.matches(fiction, "Fiction") == true);
    CHECK(gs.matches(fiction, "FICTION") == true);
    CHECK(gs.matches(fiction, "fiction") == true);

    // Substring must NOT match
    CHECK(gs.matches(nonfiction, "fiction") == false);
    CHECK(gs.matches(nonfiction, "Fiction") == false);
    CHECK(gs.matches(scifi, "fiction") == false);
    CHECK(gs.matches(scifi, "Fiction") == false);

    // Correct matches
    CHECK(gs.matches(nonfiction, "Non-Fiction") == true);
    CHECK(gs.matches(nonfiction, "non-fiction") == true);
    CHECK(gs.matches(scifi, "Science Fiction") == true);
}

TEST_CASE("search strategy: IsbnSearch") {
    IsbnSearch is;
    CHECK(std::string(is.name()) == "isbn");

    auto book13 = make_book("T", "A", "9780134685991", "G");
    auto book10 = make_book("T", "A", "0306406152", "G");

    // Exact match after normalization
    CHECK(is.matches(book13, "9780134685991") == true);
    CHECK(is.matches(book13, "978-0-13-468599-1") == true);
    CHECK(is.matches(book13, " 978 0 13 468599 1 ") == true);

    // 10-digit ISBN does not match 13-digit form
    CHECK(is.matches(book13, "0134685991") == false);

    // Partial ISBN matches nothing
    CHECK(is.matches(book13, "978013468") == false);

    // Invalid string matches nothing and doesn't throw
    CHECK(is.matches(book13, "abc") == false);
    CHECK(is.matches(book13, "") == false);

    // 10-digit with X
    auto bookX = make_book("T", "A", "080442957X", "G");
    CHECK(is.matches(bookX, "0-8044-2957-x") == true);
    CHECK(is.matches(bookX, "080442957X") == true);
}

// ── SearchStrategyFactory Tests ──────────────────────────────────

TEST_CASE("SearchStrategyFactory: valid keys") {
    auto ts = SearchStrategyFactory::create("title");
    CHECK(std::string(ts->name()) == "title");

    auto as = SearchStrategyFactory::create("  AUTHOR  ");
    CHECK(std::string(as->name()) == "author");

    auto is = SearchStrategyFactory::create("ISBN");
    CHECK(std::string(is->name()) == "isbn");

    auto gs = SearchStrategyFactory::create("Genre");
    CHECK(std::string(gs->name()) == "genre");
}

TEST_CASE("SearchStrategyFactory: invalid keys") {
    CHECK_THROWS_WITH_AS(SearchStrategyFactory::create(""), "unknown search criterion: ", std::invalid_argument);
    CHECK_THROWS_WITH_AS(SearchStrategyFactory::create("price"), "unknown search criterion: price", std::invalid_argument);
}

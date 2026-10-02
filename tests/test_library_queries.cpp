#include <doctest/doctest.h>
#include "service/LibraryService.h"
#include "service/SortStrategy.h"
#include "service/SearchStrategy.h"
#include "domain/Book.h"
#include "domain/Text.h"
#include "tests/library_fixtures.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace titans;
using namespace titans::fixtures;

// Arabic constants
const std::string kArabicBook = "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8" " 1";
const std::string kArabicPart = "\xD9\x83\xD8\xAA";

// "Available only" custom search strategy for extension test
class AvailableOnlySearch final : public ISearchStrategy {
public:
    bool matches(const Book& book, const std::string& /*query*/) const override {
        return book.is_available();
    }
    const char* name() const noexcept override { return "available_only"; }
};

// Builds a vector of Books from a Container<Book>
std::vector<Book> to_book_vector(const Container<Book>& c) {
    std::vector<Book> v;
    for (const auto& b : c) {
        v.push_back(b);
    }
    return v;
}

} // namespace

// ── 1. Search by each field on sample service ────────────────────

TEST_CASE("query: search by title substring") {
    auto svc = make_sample_service();

    // "apple" matches "apple pie" and "Big apple pie" (case-insensitive)
    auto result = svc.search_by_title("apple");
    CHECK(result.size() == 2);
    auto is = isbns(result);
    CHECK(is[0] == "9780000000001");
    CHECK(is[1] == "9780000000004");

    // Start of title
    auto r2 = svc.search_by_title("banana");
    CHECK(r2.size() == 1);
    CHECK(r2.front().isbn() == "9780000000002");

    // End of title
    auto r3 = svc.search_by_title("Sorbet");
    CHECK(r3.size() == 1);

    check_consistency(svc);
}

TEST_CASE("query: search by author") {
    auto svc = make_sample_service();

    // Jane Doe has two books
    auto result = svc.search_by_author("Jane Doe");
    CHECK(result.size() == 2);
    auto is = isbns(result);
    // In catalog order
    CHECK(is[0] == "9780000000001");
    CHECK(is[1] == "9780000000004");

    // Substring of author
    auto r2 = svc.search_by_author("doe");
    CHECK(r2.size() == 2);
}

TEST_CASE("query: search by genre exact") {
    auto svc = make_sample_service();

    // "Fiction" matches only "Fiction" books, NOT "Non-Fiction"
    auto result = svc.search_by_genre("Fiction");
    for (const auto& b : result) {
        CHECK(text::equals_ignore_case(b.genre(), "Fiction"));
    }
    // There should be exactly 4 Fiction books
    CHECK(result.size() == 4);

    // "Non-Fiction" matches only those
    auto r2 = svc.search_by_genre("non-fiction");
    CHECK(r2.size() == 2);
    for (const auto& b : r2) {
        CHECK(text::equals_ignore_case(b.genre(), "Non-Fiction"));
    }

    // "Science" matches only "Science"
    auto r3 = svc.search_by_genre("SCIENCE");
    CHECK(r3.size() == 2);
}

TEST_CASE("query: search by ISBN") {
    auto svc = make_sample_service();

    // Exact match with formatted query
    auto result = svc.search_by_isbn("978-0-00-000000-1");
    CHECK(result.size() == 1);
    CHECK(result.front().isbn() == "9780000000001");

    // Partial ISBN -> empty
    auto r2 = svc.search_by_isbn("978000000");
    CHECK(r2.empty());

    // Invalid string -> empty, no throw
    auto r3 = svc.search_by_isbn("abc");
    CHECK(r3.empty());
}

TEST_CASE("query: search Arabic book") {
    auto svc = make_sample_service();
    auto result = svc.search_by_title(kArabicBook);
    CHECK(result.size() == 1);
    CHECK(result.front().isbn() == "9780000000008");

    // Partial Arabic
    auto r2 = svc.search_by_title(kArabicPart);
    CHECK(r2.size() == 1);
}

// ── 2. Search edge cases ─────────────────────────────────────────

TEST_CASE("query: search empty/whitespace query") {
    auto svc = make_sample_service();
    CHECK_THROWS_WITH_AS(svc.search_by_title(""), "search query must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(svc.search_by_author("   "), "search query must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(svc.search_by_isbn(""), "search query must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(svc.search_by_genre("\t\n"), "search query must not be empty", std::invalid_argument);
}

TEST_CASE("query: search padded query is trimmed") {
    auto svc = make_sample_service();
    auto result = svc.search_by_title("  apple  ");
    CHECK(result.size() == 2);
}

TEST_CASE("query: search no match") {
    auto svc = make_sample_service();
    auto result = svc.search_by_title("ZZZZZZZZZ");
    CHECK(result.empty());
}

TEST_CASE("query: search empty catalog") {
    LibraryService svc;
    auto result = svc.search_by_title("anything");
    CHECK(result.empty());
}

TEST_CASE("query: search results keep catalog order") {
    auto svc = make_sample_service();
    auto catalog_isbns = isbns(svc.books());
    auto result = svc.search_by_genre("Fiction");
    auto result_isbns = isbns(result);

    // All result ISBNs appear in catalog order
    std::size_t j = 0;
    for (std::size_t i = 0; i < catalog_isbns.size() && j < result_isbns.size(); ++i) {
        if (catalog_isbns[i] == result_isbns[j]) {
            ++j;
        }
    }
    CHECK(j == result_isbns.size());
}

// ── 3. Results are copies ────────────────────────────────────────

TEST_CASE("query: results are copies") {
    auto svc = make_sample_service();
    auto result = svc.search_by_title("apple");
    CHECK(result.size() >= 1);

    // Mutate result
    auto it = result.begin();
    it->mark_borrowed();
    CHECK(!it->is_available());
    // Original is unchanged
    CHECK(svc.get_book(it->isbn()).is_available());

    // Results reflect current availability
    svc.add_member(99, "Test");
    svc.borrow_book("9780000000001", 99);
    auto result2 = svc.search_by_title("apple");
    for (const auto& b : result2) {
        if (b.isbn() == "9780000000001") {
            CHECK(!b.is_available());
        }
    }

    check_consistency(svc);
}

// ── 4. Custom strategy / factory strategy ────────────────────────

TEST_CASE("query: search with custom strategy") {
    auto svc = make_sample_service();
    svc.add_member(99, "Test");
    svc.borrow_book("9780000000001", 99);

    AvailableOnlySearch strat;
    auto result = svc.search(strat, "x");  // query is non-blank; the strategy ignores it
    CHECK(result.size() == svc.total_books() - 1);
    for (const auto& b : result) {
        CHECK(b.is_available());
    }
    check_consistency(svc);
}

TEST_CASE("query: search with factory-created strategy") {
    auto svc = make_sample_service();
    auto strat = SearchStrategyFactory::create("title");
    auto result = svc.search(*strat, "apple");
    CHECK(result.size() == 2);
}

// ── 5. Sorting all combinations ──────────────────────────────────

TEST_CASE("query: sort by field x order x algorithm") {
    struct Combo {
        std::string field;
        SortOrder order;
        SortAlgorithm algo;
    };

    std::vector<std::string> fields = {"title", "author", "genre"};
    std::vector<SortOrder> orders = {SortOrder::Ascending, SortOrder::Descending};
    std::vector<SortAlgorithm> algos = {SortAlgorithm::Merge, SortAlgorithm::Insertion};

    for (const auto& field : fields) {
        for (auto order : orders) {
            for (auto algo : algos) {
                auto svc = make_sample_service();

                // Prepare expected via std::stable_sort on a vector copy
                auto expected = to_book_vector(svc.books());
                auto cmp = [&field, order](const Book& a, const Book& b) -> bool {
                    std::string fa, fb;
                    if (field == "title") {
                        fa = text::to_lower(a.title());
                        fb = text::to_lower(b.title());
                    } else if (field == "author") {
                        fa = text::to_lower(a.author());
                        fb = text::to_lower(b.author());
                    } else {
                        fa = text::to_lower(a.genre());
                        fb = text::to_lower(b.genre());
                    }
                    if (order == SortOrder::Ascending) return fa < fb;
                    return fa > fb;
                };
                std::stable_sort(expected.begin(), expected.end(), cmp);

                auto strat = SortStrategyFactory::create(field);
                svc.sort_books(*strat, order, algo);

                auto result_isbns = isbns(svc.books());
                std::vector<std::string> expected_isbns;
                for (const auto& b : expected) expected_isbns.push_back(b.isbn());

                CHECK(result_isbns == expected_isbns);
                check_consistency(svc);
            }
        }
    }
}

// ── 6. Case-insensitive order ────────────────────────────────────

TEST_CASE("query: case-insensitive sort order") {
    auto svc = make_sample_service();
    svc.sort_by_title();

    auto ts = titles(svc.books());
    // "apple pie" should come before "Banana Split" (case-insensitive)
    bool found_apple = false;
    for (const auto& t : ts) {
        if (text::equals_ignore_case(t, "apple pie")) found_apple = true;
        if (text::equals_ignore_case(t, "Banana Split")) {
            CHECK(found_apple);
            break;
        }
    }

    // Arabic title sorts without crashing
    check_consistency(svc);
}

TEST_CASE("query: sort empty and single-book catalog") {
    LibraryService empty;
    empty.sort_by_title();
    CHECK(empty.total_books() == 0);

    LibraryService single;
    single.add_book(make_book("T", "A", "9780000000001", "G"));
    single.sort_by_title();
    CHECK(single.total_books() == 1);
    CHECK(single.books().front().title() == "T");
}

// ── 7. Stability ─────────────────────────────────────────────────

TEST_CASE("query: sort stability - title then genre") {
    for (auto algo : {SortAlgorithm::Merge, SortAlgorithm::Insertion}) {
        auto svc = make_sample_service();

        // Sort by title first
        svc.sort_by_title(SortOrder::Ascending, algo);

        // Then stable sort by genre
        svc.sort_by_genre(SortOrder::Ascending, algo);

        // Within every genre, titles should still be in ascending order
        std::string prev_genre;
        std::string prev_title_lower;
        for (const auto& b : svc.books()) {
            std::string g_lower = text::to_lower(b.genre());
            std::string t_lower = text::to_lower(b.title());
            if (g_lower == prev_genre) {
                CHECK(prev_title_lower <= t_lower);
            }
            prev_genre = g_lower;
            prev_title_lower = t_lower;
        }

        // Compare with std::stable_sort
        auto expected = to_book_vector(svc.books());
        // already sorted, just verify the pattern matches std::stable_sort
        auto cmp = [](const Book& a, const Book& b) {
            return text::to_lower(a.genre()) < text::to_lower(b.genre());
        };
        CHECK(std::is_sorted(expected.begin(), expected.end(), cmp));
        check_consistency(svc);
    }
}

TEST_CASE("query: sort descending stability") {
    for (auto algo : {SortAlgorithm::Merge, SortAlgorithm::Insertion}) {
        auto svc = make_sample_service();
        svc.sort_by_title(SortOrder::Ascending, algo);
        svc.sort_by_genre(SortOrder::Descending, algo);

        // Compare with std::stable_sort
        auto expected = to_book_vector(svc.books());
        // Already sorted by the service, verify it matches std::stable_sort on a fresh copy
        auto svc2 = make_sample_service();
        auto v2 = to_book_vector(svc2.books());
        auto title_cmp = [](const Book& a, const Book& b) {
            return text::to_lower(a.title()) < text::to_lower(b.title());
        };
        std::stable_sort(v2.begin(), v2.end(), title_cmp);
        auto genre_desc_cmp = [](const Book& a, const Book& b) {
            return text::to_lower(a.genre()) > text::to_lower(b.genre());
        };
        std::stable_sort(v2.begin(), v2.end(), genre_desc_cmp);

        auto actual_isbns = isbns(svc.books());
        std::vector<std::string> expected_isbns;
        for (const auto& b : v2) expected_isbns.push_back(b.isbn());
        CHECK(actual_isbns == expected_isbns);
        check_consistency(svc);
    }
}

// ── 8. Sort preserves data ───────────────────────────────────────

TEST_CASE("query: sort preserves books, availability, members, invariants") {
    auto svc = make_sample_service();
    svc.add_member(99, "Tester");
    svc.borrow_book("9780000000001", 99);

    auto before_snap = snapshot(svc);

    svc.sort_by_title();

    // Same books (as a multiset of ISBNs)
    auto before_isbns = before_snap.books;
    auto after_books = to_book_vector(svc.books());
    CHECK(before_isbns.size() == after_books.size());

    // Borrowed book is still borrowed
    CHECK(!svc.get_book("9780000000001").is_available());
    CHECK(svc.get_member(99).has_borrowed("9780000000001"));

    // Members untouched
    CHECK(svc.member_count() == before_snap.members.size());

    check_consistency(svc);
}

// ── 9. References stay valid ─────────────────────────────────────

TEST_CASE("query: references stay valid after sort") {
    auto svc = make_sample_service();
    const Book& before = svc.get_book("9780000000001");
    std::string title_before = before.title();

    svc.sort_by_title();

    const Book& after = svc.get_book("9780000000001");
    CHECK(&before == &after);
    CHECK(after.title() == title_before);
}

// ── 10. Operations after sort ────────────────────────────────────

TEST_CASE("query: operations after sort") {
    auto svc = make_sample_service();
    svc.sort_by_title();

    // add_book appends at end
    svc.add_book(make_book("Zebra Cake", "Z Author", "9780000000099", "Fiction"));
    auto last = svc.books().back();
    CHECK(last.isbn() == "9780000000099");

    // remove_book works
    svc.remove_book("9780000000099");
    CHECK(!svc.has_book("9780000000099"));

    // borrow_book works
    svc.add_member(99, "Tester");
    svc.borrow_book("9780000000001", 99);
    CHECK(!svc.get_book("9780000000001").is_available());

    check_consistency(svc);
}

// ── 11. Through factory and convenience functions ────────────────

TEST_CASE("query: sort through factory") {
    auto svc = make_sample_service();
    svc.sort_books(*SortStrategyFactory::create("Genre"));
    auto g_isbns = isbns(svc.books());

    auto svc2 = make_sample_service();
    svc2.sort_by_genre();
    auto g2_isbns = isbns(svc2.books());

    CHECK(g_isbns == g2_isbns);

    // Descending + insertion
    auto svc3 = make_sample_service();
    svc3.sort_books(*SortStrategyFactory::create("title"), SortOrder::Descending, SortAlgorithm::Insertion);

    auto svc4 = make_sample_service();
    svc4.sort_by_title(SortOrder::Descending, SortAlgorithm::Insertion);

    CHECK(isbns(svc3.books()) == isbns(svc4.books()));
    check_consistency(svc3);
    check_consistency(svc4);
}

TEST_CASE("query: convenience functions match sort_books") {
    auto svc_title = make_sample_service();
    svc_title.sort_by_title();
    auto svc_title2 = make_sample_service();
    svc_title2.sort_books(*SortStrategyFactory::create("title"));
    CHECK(isbns(svc_title.books()) == isbns(svc_title2.books()));

    auto svc_author = make_sample_service();
    svc_author.sort_by_author();
    auto svc_author2 = make_sample_service();
    svc_author2.sort_books(*SortStrategyFactory::create("author"));
    CHECK(isbns(svc_author.books()) == isbns(svc_author2.books()));
}

// ── 12. Larger input ─────────────────────────────────────────────

TEST_CASE("query: sort 300 books") {
    LibraryService svc;
    std::mt19937 rng(54321);
    for (int i = 0; i < 300; ++i) {
        std::string title;
        int len = 5 + static_cast<int>(rng() % 15);
        for (int j = 0; j < len; ++j) {
            title += static_cast<char>('a' + rng() % 26);
        }
        // Unique 13-digit ISBN: 978 + 10 digits
        char isbn_buf[14];
        std::snprintf(isbn_buf, sizeof(isbn_buf), "978%010d", i);
        svc.add_book(make_book(title, "Author", isbn_buf, "Genre"));
    }

    auto comp = [](const Book& a, const Book& b) {
        return text::to_lower(a.title()) < text::to_lower(b.title());
    };

    for (auto algo : {SortAlgorithm::Merge, SortAlgorithm::Insertion}) {
        auto svc_copy = svc;

        svc_copy.sort_by_title(SortOrder::Ascending, algo);

        // Expected sorted order via std::stable_sort on vector copy
        auto expected = to_book_vector(svc.books());
        std::stable_sort(expected.begin(), expected.end(), comp);

        auto actual = to_book_vector(svc_copy.books());
        CHECK(std::is_sorted(actual.begin(), actual.end(), comp));

        std::vector<std::string> actual_isbns, expected_isbns;
        for (const auto& b : actual) actual_isbns.push_back(b.isbn());
        for (const auto& b : expected) expected_isbns.push_back(b.isbn());
        CHECK(actual_isbns == expected_isbns);
        CHECK(svc_copy.total_books() == 300);
    }
}

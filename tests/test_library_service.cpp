#include <doctest/doctest.h>
#include "service/LibraryService.h"
#include "service/LibraryErrors.h"
#include "domain/Book.h"
#include "domain/Member.h"
#include "tests/library_fixtures.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace titans;
using namespace titans::fixtures;

} // namespace

// ── 1. Empty service ─────────────────────────────────────────────

TEST_CASE("service: empty state") {
    LibraryService svc;
    CHECK(svc.total_books() == 0);
    CHECK(svc.member_count() == 0);
    CHECK(svc.available_books() == 0);
    CHECK(svc.borrowed_books() == 0);
    CHECK(svc.books().empty());
    CHECK(svc.members().empty());
    CHECK(!svc.has_member(1));
    CHECK(!svc.has_book("9780134685991"));
    check_consistency(svc);
}

// ── 2. Members ───────────────────────────────────────────────────

TEST_CASE("service: add and query members") {
    LibraryService svc;
    svc.add_member(10, "Alice");
    svc.add_member(20, "Bob");
    svc.add_member(30, "Charlie");

    CHECK(svc.member_count() == 3);
    CHECK(svc.has_member(10));
    CHECK(svc.has_member(20));
    CHECK(svc.has_member(30));
    CHECK(svc.get_member(10).name() == "Alice");
    CHECK(svc.get_member(20).name() == "Bob");

    // Order kept
    CHECK(member_ids(svc.members()) == std::vector<int>{10, 20, 30});
    check_consistency(svc);
}

TEST_CASE("service: add member duplicate") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.add_member(1, "Bob"), "member already exists: 1", DuplicateError);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: add member invalid") {
    LibraryService svc;
    auto snap = snapshot(svc);

    CHECK_THROWS_AS(svc.add_member(0, "Alice"), std::invalid_argument);
    CHECK(snapshot(svc) == snap);

    CHECK_THROWS_AS(svc.add_member(-5, "Alice"), std::invalid_argument);
    CHECK(snapshot(svc) == snap);

    CHECK_THROWS_AS(svc.add_member(1, ""), std::invalid_argument);
    CHECK(snapshot(svc) == snap);

    CHECK_THROWS_AS(svc.add_member(1, "   "), std::invalid_argument);
    CHECK(snapshot(svc) == snap);

    check_consistency(svc);
}

TEST_CASE("service: get_member unknown") {
    LibraryService svc;
    CHECK_THROWS_WITH_AS(svc.get_member(99), "member not found: 99", NotFoundError);
}

TEST_CASE("service: remove members") {
    LibraryService svc;
    svc.add_member(1, "A");
    svc.add_member(2, "B");
    svc.add_member(3, "C");

    // Remove middle
    svc.remove_member(2);
    CHECK(svc.member_count() == 2);
    CHECK(member_ids(svc.members()) == std::vector<int>{1, 3});

    // Remove first
    svc.remove_member(1);
    CHECK(member_ids(svc.members()) == std::vector<int>{3});

    // Remove last
    svc.remove_member(3);
    CHECK(svc.member_count() == 0);

    check_consistency(svc);
}

TEST_CASE("service: remove member unknown") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.remove_member(99), "member not found: 99", NotFoundError);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: remove member with borrowed books") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_book(make_book("T", "A", "9780000000001", "G"));
    svc.borrow_book("9780000000001", 1);

    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.remove_member(1), "member still has borrowed books", std::logic_error);
    CHECK(snapshot(svc) == snap);

    // After returning the book, removal succeeds
    svc.return_book("9780000000001");
    svc.remove_member(1);
    CHECK(svc.member_count() == 0);
    check_consistency(svc);
}

// ── 3. Books ─────────────────────────────────────────────────────

TEST_CASE("service: add and query books") {
    LibraryService svc;
    svc.add_book(make_book("T1", "A1", "9780000000001", "G1"));
    svc.add_book(make_book("T2", "A2", "9780000000002", "G2"));
    svc.add_book(make_book("T3", "A3", "9780000000003", "G3"));

    CHECK(svc.total_books() == 3);
    CHECK(svc.has_book("9780000000001"));
    CHECK(svc.get_book("9780000000001").title() == "T1");

    // Order kept (appended)
    CHECK(isbns(svc.books()) == std::vector<std::string>{"9780000000001", "9780000000002", "9780000000003"});
    check_consistency(svc);
}

TEST_CASE("service: add book duplicate ISBN") {
    LibraryService svc;
    svc.add_book(make_book("T1", "A1", "978-0-13-468599-1", "G1"));
    auto snap = snapshot(svc);

    // Same ISBN different format
    CHECK_THROWS_WITH_AS(svc.add_book(make_book("T2", "A2", "9780134685991", "G2")),
                         "book already exists: 9780134685991", DuplicateError);
    CHECK(snapshot(svc) == snap);

    // Same ISBN in original format
    CHECK_THROWS_WITH_AS(svc.add_book(make_book("T3", "A3", "978-0-13-468599-1", "G3")),
                         "book already exists: 9780134685991", DuplicateError);
    CHECK(snapshot(svc) == snap);

    // Only title differs
    CHECK_THROWS_WITH_AS(svc.add_book(make_book("Different Title", "A1", "978-0-13-468599-1", "G1")),
                         "book already exists: 9780134685991", DuplicateError);
    CHECK(snapshot(svc) == snap);

    check_consistency(svc);
}

TEST_CASE("service: add unavailable book") {
    LibraryService svc;
    auto snap = snapshot(svc);
    Book unavail("T", "A", "9780000000001", "G", false);
    CHECK_THROWS_WITH_AS(svc.add_book(std::move(unavail)),
                         "a new book must be available", std::invalid_argument);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: add unavailable duplicate gives availability error first") {
    LibraryService svc;
    svc.add_book(make_book("T1", "A1", "9780000000001", "G1"));
    auto snap = snapshot(svc);

    // Unavailable AND duplicate: availability error has priority
    Book unavail_dup("T2", "A2", "9780000000001", "G2", false);
    CHECK_THROWS_WITH_AS(svc.add_book(std::move(unavail_dup)),
                         "a new book must be available", std::invalid_argument);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: get_book and has_book with formatted ISBN") {
    LibraryService svc;
    svc.add_book(make_book("T", "A", "978-0-13-468599-1", "G"));

    CHECK(svc.has_book("978-0-13-468599-1"));
    CHECK(svc.has_book("9780134685991"));
    CHECK(svc.get_book("978-0-13-468599-1").title() == "T");

    // Invalid ISBN -> has_book false, get_book throws
    CHECK(!svc.has_book("abc"));
    CHECK_THROWS_AS(svc.get_book("abc"), std::invalid_argument);

    // Unknown ISBN
    CHECK(!svc.has_book("9780000000099"));
    CHECK_THROWS_WITH_AS(svc.get_book("9780000000099"), "book not found: 9780000000099", NotFoundError);

    check_consistency(svc);
}

TEST_CASE("service: remove books") {
    LibraryService svc;
    svc.add_book(make_book("T1", "A1", "9780000000001", "G1"));
    svc.add_book(make_book("T2", "A2", "9780000000002", "G2"));
    svc.add_book(make_book("T3", "A3", "9780000000003", "G3"));

    // Remove middle
    svc.remove_book("9780000000002");
    CHECK(svc.total_books() == 2);
    CHECK(isbns(svc.books()) == std::vector<std::string>{"9780000000001", "9780000000003"});

    // Remove first
    svc.remove_book("9780000000001");
    CHECK(isbns(svc.books()) == std::vector<std::string>{"9780000000003"});

    // Remove last
    svc.remove_book("9780000000003");
    CHECK(svc.total_books() == 0);
    check_consistency(svc);
}

TEST_CASE("service: remove book unknown") {
    LibraryService svc;
    svc.add_book(make_book("T", "A", "9780000000001", "G"));
    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.remove_book("9780000000099"), "book not found: 9780000000099", NotFoundError);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: remove borrowed book") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_book(make_book("T", "A", "9780000000001", "G"));
    svc.borrow_book("9780000000001", 1);

    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.remove_book("9780000000001"), "cannot remove a borrowed book", std::logic_error);
    CHECK(snapshot(svc) == snap);

    // After return, removal works
    svc.return_book("9780000000001");
    svc.remove_book("9780000000001");
    CHECK(svc.total_books() == 0);
    check_consistency(svc);
}

// ── 4. Borrow success ────────────────────────────────────────────

TEST_CASE("service: borrow success") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_member(2, "Bob");
    svc.add_book(make_book("T1", "A1", "9780000000001", "G1"));
    svc.add_book(make_book("T2", "A2", "9780000000002", "G2"));

    svc.borrow_book("9780000000001", 1);
    CHECK(!svc.get_book("9780000000001").is_available());
    CHECK(svc.get_member(1).has_borrowed("9780000000001"));

    // Formatted ISBN
    svc.borrow_book("978-0-00-000000-2", 2);
    CHECK(!svc.get_book("9780000000002").is_available());
    CHECK(svc.get_member(2).has_borrowed("9780000000002"));

    check_consistency(svc);
}

// ── 5. Borrow failures ──────────────────────────────────────────

TEST_CASE("service: borrow invalid ISBN") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    auto snap = snapshot(svc);
    CHECK_THROWS_AS(svc.borrow_book("abc", 1), std::invalid_argument);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: borrow unknown member") {
    LibraryService svc;
    svc.add_book(make_book("T", "A", "9780000000001", "G"));
    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.borrow_book("9780000000001", 99), "member not found: 99", NotFoundError);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: borrow unknown book") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.borrow_book("9780000000099", 1), "book not found: 9780000000099", NotFoundError);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: borrow already borrowed book") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_member(2, "Bob");
    svc.add_book(make_book("T", "A", "9780000000001", "G"));
    svc.borrow_book("9780000000001", 1);

    // Same member
    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.borrow_book("9780000000001", 1), "book is not available", std::logic_error);
    CHECK(snapshot(svc) == snap);

    // Different member
    CHECK_THROWS_WITH_AS(svc.borrow_book("9780000000001", 2), "book is not available", std::logic_error);
    CHECK(snapshot(svc) == snap);

    check_consistency(svc);
}

TEST_CASE("service: borrow limit reached") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    for (int i = 1; i <= static_cast<int>(Member::kMaxBorrowed) + 1; ++i) {
        svc.add_book(make_book("T" + std::to_string(i), "A", "978000000000" + std::to_string(i), "G"));
    }
    for (int i = 1; i <= static_cast<int>(Member::kMaxBorrowed); ++i) {
        svc.borrow_book("978000000000" + std::to_string(i), 1);
    }

    auto snap = snapshot(svc);
    int extra = static_cast<int>(Member::kMaxBorrowed) + 1;
    CHECK_THROWS_WITH_AS(svc.borrow_book("978000000000" + std::to_string(extra), 1),
                         "borrow limit reached", std::logic_error);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

TEST_CASE("service: borrow precedence - unknown member vs unknown book") {
    LibraryService svc;
    auto snap = snapshot(svc);
    // Both member and book unknown -> member checked first
    CHECK_THROWS_WITH_AS(svc.borrow_book("9780000000001", 99), "member not found: 99", NotFoundError);
    CHECK(snapshot(svc) == snap);
}

TEST_CASE("service: borrow precedence - unavailable vs limit") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_member(2, "Bob");
    for (int i = 1; i <= static_cast<int>(Member::kMaxBorrowed) + 1; ++i) {
        svc.add_book(make_book("T" + std::to_string(i), "A", "978000000000" + std::to_string(i), "G"));
    }
    // Fill member 1's borrow limit
    for (int i = 1; i <= static_cast<int>(Member::kMaxBorrowed); ++i) {
        svc.borrow_book("978000000000" + std::to_string(i), 1);
    }
    // Member 2 borrows the extra book
    int extra = static_cast<int>(Member::kMaxBorrowed) + 1;
    svc.borrow_book("978000000000" + std::to_string(extra), 2);

    auto snap = snapshot(svc);
    // Book is unavailable AND member 1 is at limit -> "book is not available" wins
    CHECK_THROWS_WITH_AS(svc.borrow_book("978000000000" + std::to_string(extra), 1),
                         "book is not available", std::logic_error);
    CHECK(snapshot(svc) == snap);
    check_consistency(svc);
}

// ── 6. Limit cycle ───────────────────────────────────────────────

TEST_CASE("service: borrow limit cycle") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    for (int i = 1; i <= static_cast<int>(Member::kMaxBorrowed) + 1; ++i) {
        svc.add_book(make_book("T" + std::to_string(i), "A", "978000000000" + std::to_string(i), "G"));
    }
    for (int i = 1; i <= static_cast<int>(Member::kMaxBorrowed); ++i) {
        svc.borrow_book("978000000000" + std::to_string(i), 1);
    }

    int extra = static_cast<int>(Member::kMaxBorrowed) + 1;
    CHECK_THROWS_AS(svc.borrow_book("978000000000" + std::to_string(extra), 1), std::logic_error);

    // Return one, borrow again
    svc.return_book("9780000000001");
    svc.borrow_book("978000000000" + std::to_string(extra), 1);
    CHECK(svc.get_member(1).borrowed_count() == Member::kMaxBorrowed);
    check_consistency(svc);
}

// ── 7. Return ────────────────────────────────────────────────────

TEST_CASE("service: return success") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_book(make_book("T1", "A1", "9780000000001", "G1"));
    svc.add_book(make_book("T2", "A2", "9780000000002", "G2"));
    svc.add_book(make_book("T3", "A3", "9780000000003", "G3"));
    svc.borrow_book("9780000000001", 1);
    svc.borrow_book("9780000000002", 1);
    svc.borrow_book("9780000000003", 1);

    // Return middle
    int holder = svc.return_book("9780000000002");
    CHECK(holder == 1);
    CHECK(svc.get_book("9780000000002").is_available());
    CHECK(!svc.get_member(1).has_borrowed("9780000000002"));
    // Other borrows intact
    CHECK(svc.get_member(1).has_borrowed("9780000000001"));
    CHECK(svc.get_member(1).has_borrowed("9780000000003"));

    // Formatted ISBN
    int h2 = svc.return_book("978-0-00-000000-1");
    CHECK(h2 == 1);
    CHECK(svc.get_book("9780000000001").is_available());
    check_consistency(svc);
}

TEST_CASE("service: return failures") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_book(make_book("T", "A", "9780000000001", "G"));

    // Unknown book
    CHECK_THROWS_WITH_AS(svc.return_book("9780000000099"), "book not found: 9780000000099", NotFoundError);

    // Not borrowed
    auto snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.return_book("9780000000001"), "book is not borrowed", std::logic_error);
    CHECK(snapshot(svc) == snap);

    // Invalid ISBN
    CHECK_THROWS_AS(svc.return_book("abc"), std::invalid_argument);

    // Double return
    svc.borrow_book("9780000000001", 1);
    svc.return_book("9780000000001");
    snap = snapshot(svc);
    CHECK_THROWS_WITH_AS(svc.return_book("9780000000001"), "book is not borrowed", std::logic_error);
    CHECK(snapshot(svc) == snap);

    check_consistency(svc);
}

TEST_CASE("service: re-borrow by different member after return") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_member(2, "Bob");
    svc.add_book(make_book("T", "A", "9780000000001", "G"));

    svc.borrow_book("9780000000001", 1);
    svc.return_book("9780000000001");
    svc.borrow_book("9780000000001", 2);
    CHECK(svc.get_member(2).has_borrowed("9780000000001"));
    CHECK(!svc.get_member(1).has_borrowed("9780000000001"));
    check_consistency(svc);
}

// ── 8. Statistics ────────────────────────────────────────────────

TEST_CASE("service: statistics after sequence") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_member(2, "Bob");
    svc.add_book(make_book("T1", "A1", "9780000000001", "G1"));
    svc.add_book(make_book("T2", "A2", "9780000000002", "G2"));
    svc.add_book(make_book("T3", "A3", "9780000000003", "G3"));

    CHECK(svc.total_books() == 3);
    CHECK(svc.available_books() == 3);
    CHECK(svc.borrowed_books() == 0);
    CHECK(svc.member_count() == 2);

    svc.borrow_book("9780000000001", 1);
    svc.borrow_book("9780000000002", 2);
    CHECK(svc.available_books() == 1);
    CHECK(svc.borrowed_books() == 2);

    svc.return_book("9780000000001");
    CHECK(svc.available_books() == 2);
    CHECK(svc.borrowed_books() == 1);

    svc.remove_book("9780000000001");
    CHECK(svc.total_books() == 2);
    CHECK(svc.available_books() == 1);
    CHECK(svc.borrowed_books() == 1);

    // Sum consistency
    std::size_t total_borrowed = 0;
    for (const auto& m : svc.members()) {
        total_borrowed += m.borrowed_count();
    }
    CHECK(svc.borrowed_books() == total_borrowed);

    check_consistency(svc);
}

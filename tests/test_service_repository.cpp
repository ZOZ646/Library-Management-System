#include <doctest/doctest.h>
#include "service/LibraryService.h"
#include "repository/NullRepository.h"
#include "repository/RepositoryErrors.h"
#include "tests/repository_fixtures.h"
#include "tests/library_fixtures.h"

#include <memory>
#include <string>
#include <vector>

using namespace titans;
using namespace titans::fixtures;

TEST_SUITE("ServiceRepository") {

TEST_CASE("1. Default-constructed LibraryService works in-memory without writes") {
    LibraryService service;
    service.add_member(1, "Alice");
    service.add_book(Book("Title", "Author", "978-0-306-40615-7", "Fiction", true));
    service.borrow_book("978-0-306-40615-7", 1);
    service.return_book("978-0-306-40615-7");
    service.remove_book("978-0-306-40615-7");
    service.remove_member(1);

    LibraryService copy = service;
    copy.add_member(2, "Bob");
    CHECK(copy.has_member(2));
    CHECK_FALSE(service.has_member(2));
}

TEST_CASE("2. Replaying stored data in open() produces no write calls") {
    auto repo = std::make_shared<RecordingRepository>();
    repo->stored.members.emplace_back(1, "Alice");
    repo->stored.books.emplace_back("Book 1", "Author 1", "9780306406157", "Fiction", true);
    repo->stored.open_loans.push_back({"9780306406157", 1});

    LibraryService service = LibraryService::open(repo);
    CHECK(repo->calls.size() == 1);
    CHECK(repo->calls[0] == "load");
    CHECK(repo->write_count() == 0);
    CHECK(service.member_count() == 1);
    CHECK(service.total_books() == 1);
    CHECK(service.borrowed_books() == 1);
}

TEST_CASE("3. Successful mutations produce exactly one write with normalized arguments") {
    auto repo = std::make_shared<RecordingRepository>();
    LibraryService service = LibraryService::open(repo);
    repo->calls.clear();

    // 1. add_member
    service.add_member(1, "Alice");
    REQUIRE(repo->calls.size() == 1);
    CHECK(repo->calls[0] == "add_member:1");

    // 2. add_book (hyphenated ISBN)
    service.add_book(Book("B1", "A1", "978-0-306-40615-7", "G1", true));
    REQUIRE(repo->calls.size() == 2);
    CHECK(repo->calls[1] == "add_book:9780306406157");

    // 3. borrow_book (hyphenated ISBN)
    service.borrow_book("978-0-306-40615-7", 1);
    REQUIRE(repo->calls.size() == 3);
    CHECK(repo->calls[2] == "record_borrow:9780306406157:1");

    // 4. return_book (hyphenated ISBN)
    service.return_book("978-0-306-40615-7");
    REQUIRE(repo->calls.size() == 4);
    CHECK(repo->calls[3] == "record_return:9780306406157");

    // 5. remove_book
    service.remove_book("978-0-306-40615-7");
    REQUIRE(repo->calls.size() == 5);
    CHECK(repo->calls[4] == "remove_book:9780306406157");

    // 6. remove_member
    service.remove_member(1);
    REQUIRE(repo->calls.size() == 6);
    CHECK(repo->calls[5] == "remove_member:1");
}

TEST_CASE("4. Failing mutations produce zero writes and preserve snapshot") {
    auto repo = std::make_shared<RecordingRepository>();
    LibraryService service = LibraryService::open(repo);
    service.add_member(1, "Alice");
    service.add_book(Book("Book 1", "Author 1", "9780306406157", "Fiction", true));
    service.add_book(Book("Book 2", "Author 2", "9780140449136", "History", true));
    service.borrow_book("9780306406157", 1);
    repo->calls.clear();

    Snapshot before = snapshot(service);

    // Invalid member data
    CHECK_THROWS_AS(service.add_member(-5, "Bad"), std::invalid_argument);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Duplicate member
    CHECK_THROWS_AS(service.add_member(1, "Alice"), DuplicateError);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Unknown member removal
    CHECK_THROWS_AS(service.remove_member(99), NotFoundError);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Member holding books removal
    CHECK_THROWS_AS(service.remove_member(1), std::logic_error);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Duplicate book
    CHECK_THROWS_AS(service.add_book(Book("Dup", "Auth", "978-0-306-40615-7", "G", true)), DuplicateError);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Unavailable book on add
    CHECK_THROWS_AS(service.add_book(Book("Unavail", "Auth", "9780201896831", "G", false)), std::invalid_argument);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Unknown book removal
    CHECK_THROWS_AS(service.remove_book("9780201896831"), NotFoundError);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Removing borrowed book
    CHECK_THROWS_AS(service.remove_book("9780306406157"), std::logic_error);
    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);

    // Borrow failures: invalid ISBN, unknown member, unknown book, not available, limit reached
    CHECK_THROWS_AS(service.borrow_book("invalid-isbn", 1), std::invalid_argument);
    CHECK_THROWS_AS(service.borrow_book("9780140449136", 999), NotFoundError);
    CHECK_THROWS_AS(service.borrow_book("9780201896831", 1), NotFoundError);
    CHECK_THROWS_AS(service.borrow_book("9780306406157", 1), std::logic_error);

    // Return failures: invalid ISBN, unknown book, not borrowed
    CHECK_THROWS_AS(service.return_book("invalid-isbn"), std::invalid_argument);
    CHECK_THROWS_AS(service.return_book("9780201896831"), NotFoundError);
    CHECK_THROWS_AS(service.return_book("9780140449136"), std::logic_error);

    CHECK(repo->calls.empty());
    CHECK(snapshot(service) == before);
}

TEST_CASE("5. Read operations, searches and sorts produce zero repository writes") {
    auto repo = std::make_shared<RecordingRepository>();
    LibraryService service = LibraryService::open(repo);
    service.add_member(1, "Alice");
    service.add_book(Book("B1", "A1", "9780306406157", "Fiction", true));
    service.add_book(Book("B2", "A2", "9780140449136", "History", true));
    repo->calls.clear();

    (void)service.get_book("9780306406157");
    (void)service.get_member(1);
    (void)service.has_book("9780306406157");
    (void)service.has_member(1);
    (void)service.available_books();
    (void)service.borrowed_books();
    (void)service.member_count();
    (void)service.total_books();
    (void)service.books();
    (void)service.members();
    (void)service.search_by_title("B");
    (void)service.search_by_author("A");
    (void)service.search_by_isbn("9780306406157");
    (void)service.search_by_genre("Fiction");

    TitleSort sort_strat;
    service.sort_books(sort_strat);
    service.sort_by_title();
    service.sort_by_author();
    service.sort_by_genre();

    CHECK(repo->calls.empty());
}

TEST_CASE("6. Write-ahead order verified through repository hook") {
    auto repo = std::make_shared<RecordingRepository>();
    LibraryService service = LibraryService::open(repo);

    // 1. During add_book: book is not yet in catalog
    repo->hook = [&](const std::string& call) {
        if (call == "add_book:9780306406157") {
            CHECK_FALSE(service.has_book("9780306406157"));
        }
    };
    service.add_book(Book("Title", "Author", "9780306406157", "Genre", true));

    // 2. During add_member: member not yet in registry
    repo->hook = [&](const std::string& call) {
        if (call == "add_member:1") {
            CHECK_FALSE(service.has_member(1));
        }
    };
    service.add_member(1, "Alice");

    // 3. During borrow_book: book still available, member does not yet hold it
    repo->hook = [&](const std::string& call) {
        if (call.rfind("record_borrow:", 0) == 0) {
            CHECK(service.get_book("9780306406157").is_available());
            CHECK_FALSE(service.get_member(1).has_borrowed("9780306406157"));
        }
    };
    service.borrow_book("9780306406157", 1);

    // 4. During return_book: book still unavailable
    repo->hook = [&](const std::string& call) {
        if (call.rfind("record_return:", 0) == 0) {
            CHECK_FALSE(service.get_book("9780306406157").is_available());
            CHECK(service.get_member(1).has_borrowed("9780306406157"));
        }
    };
    service.return_book("9780306406157");

    // 5. During remove_book: book is still there
    repo->hook = [&](const std::string& call) {
        if (call.rfind("remove_book:", 0) == 0) {
            CHECK(service.has_book("9780306406157"));
        }
    };
    service.remove_book("9780306406157");

    // 6. During remove_member: member is still there
    repo->hook = [&](const std::string& call) {
        if (call.rfind("remove_member:", 0) == 0) {
            CHECK(service.has_member(1));
        }
    };
    service.remove_member(1);

    repo->hook = nullptr;
}

TEST_CASE("7. Strong guarantee under repository failure (16-step story)") {
    struct Step {
        std::string desc;
        std::function<void(LibraryService&)> action;
    };

    std::string b1 = "9780000000019";
    std::string b2 = "9780000000026";
    std::string b3 = "9780000000033";
    std::string b4 = "9780000000040";
    std::string b5 = "9780000000057";
    std::string b6 = "9780000000064";
    std::string b7 = "9780000000071";
    std::string b8 = "9780000000088";
    std::string b9 = "9780000000095";
    std::string b101 = "9780000000101";
    std::string b102 = "9780000000118";

    std::vector<Step> story = {
        {"add_member 101", [](LibraryService& s) { s.add_member(101, "M101"); }},
        {"add_member 102", [](LibraryService& s) { s.add_member(102, "M102"); }},
        {"add_book B101", [b101](LibraryService& s) { s.add_book(Book("B101", "A101", b101, "G", true)); }},
        {"add_book B102", [b102](LibraryService& s) { s.add_book(Book("B102", "A102", b102, "G", true)); }},
        {"borrow B1 by 1", [b1](LibraryService& s) { s.borrow_book(b1, 1); }},
        {"borrow B2 by 2", [b2](LibraryService& s) { s.borrow_book(b2, 2); }},
        {"borrow B3 by 3", [b3](LibraryService& s) { s.borrow_book(b3, 3); }},
        {"return B4", [b4](LibraryService& s) { s.return_book(b4); }},
        {"return B5", [b5](LibraryService& s) { s.return_book(b5); }},
        {"return B6", [b6](LibraryService& s) { s.return_book(b6); }},
        {"remove B7", [b7](LibraryService& s) { s.remove_book(b7); }},
        {"remove B8", [b8](LibraryService& s) { s.remove_book(b8); }},
        {"remove B9", [b9](LibraryService& s) { s.remove_book(b9); }},
        {"remove M7", [](LibraryService& s) { s.remove_member(7); }},
        {"remove M8", [](LibraryService& s) { s.remove_member(8); }},
        {"remove M9", [](LibraryService& s) { s.remove_member(9); }}
    };
    REQUIRE(story.size() >= 15);

    auto make_initial_stored = [&]() {
        StoredLibrary stored;
        for (int i = 1; i <= 9; ++i) {
            stored.members.emplace_back(i, "Member " + std::to_string(i));
        }
        std::vector<std::string> isbns = {b1, b2, b3, b4, b5, b6, b7, b8, b9};
        for (int i = 0; i < 9; ++i) {
            stored.books.emplace_back("Book " + std::to_string(i + 1),
                                      "Author " + std::to_string(i + 1),
                                      isbns[i], "Genre", true);
        }
        stored.open_loans.push_back({b4, 4});
        stored.open_loans.push_back({b5, 5});
        stored.open_loans.push_back({b6, 6});
        return stored;
    };

    for (std::size_t k = 1; k <= story.size(); ++k) {
        auto underlying = std::make_shared<RecordingRepository>();
        underlying->stored = make_initial_stored();
        auto failing = std::make_shared<FailingRepository>(*underlying, k);
        LibraryService service = LibraryService::open(failing);

        for (std::size_t step_idx = 0; step_idx < story.size(); ++step_idx) {
            Snapshot before = snapshot(service);
            if (step_idx + 1 == k) {
                CHECK_THROWS_WITH_AS(story[step_idx].action(service), "injected failure", DatabaseError);
                CHECK(snapshot(service) == before);
                check_consistency(service);
            } else {
                story[step_idx].action(service);
                check_consistency(service);
            }
        }
    }
}

TEST_CASE("8. open() null check and state restoration from crafted StoredLibrary") {
    CHECK_THROWS_WITH_AS(LibraryService::open(nullptr), "repository must not be null", std::invalid_argument);

    auto repo = std::make_shared<RecordingRepository>();
    repo->stored.members.emplace_back(10, "Member 10");
    repo->stored.members.emplace_back(20, "Member 20");
    repo->stored.books.emplace_back("Book A", "Author A", "9780306406157", "Genre A", true);
    repo->stored.books.emplace_back("Book B", "Author B", "9780140449136", "Genre B", true);
    repo->stored.open_loans.push_back({"9780306406157", 10});

    LibraryService service = LibraryService::open(repo);
    CHECK(service.member_count() == 2);
    CHECK(service.total_books() == 2);
    CHECK(service.available_books() == 1);
    CHECK(service.borrowed_books() == 1);
    CHECK_FALSE(service.get_book("9780306406157").is_available());
    CHECK(service.get_book("9780140449136").is_available());
    CHECK(service.get_member(10).borrowed_count() == 1);

    // New mutations write to the SAME repo
    service.add_member(30, "Member 30");
    CHECK(repo->calls.back() == "add_member:30");
}

TEST_CASE("9. Inconsistent stored data throws DatabaseError starting with 'database contents are inconsistent: '") {
    auto check_inconsistent = [](const StoredLibrary& lib) {
        auto repo = std::make_shared<RecordingRepository>();
        repo->stored = lib;
        try {
            LibraryService::open(repo);
            FAIL("expected DatabaseError for inconsistent data");
        } catch (const DatabaseError& e) {
            std::string msg = e.what();
            CHECK(msg.rfind("database contents are inconsistent: ", 0) == 0);
        }
    };

    // (a) Duplicate ISBN
    {
        StoredLibrary lib;
        lib.books.emplace_back("B1", "A1", "9780306406157", "G1", true);
        lib.books.emplace_back("B2", "A2", "9780306406157", "G2", true);
        check_inconsistent(lib);
    }

    // (b) Duplicate member id
    {
        StoredLibrary lib;
        lib.members.emplace_back(1, "Alice");
        lib.members.emplace_back(1, "Bob");
        check_inconsistent(lib);
    }

    // (c) Loan for unknown book
    {
        StoredLibrary lib;
        lib.members.emplace_back(1, "Alice");
        lib.open_loans.push_back({"9780306406157", 1});
        check_inconsistent(lib);
    }

    // (d) Loan for unknown member
    {
        StoredLibrary lib;
        lib.books.emplace_back("B1", "A1", "9780306406157", "G1", true);
        lib.open_loans.push_back({"9780306406157", 1});
        check_inconsistent(lib);
    }

    // (e) Two open loans for same book
    {
        StoredLibrary lib;
        lib.books.emplace_back("B1", "A1", "9780306406157", "G1", true);
        lib.members.emplace_back(1, "Alice");
        lib.members.emplace_back(2, "Bob");
        lib.open_loans.push_back({"9780306406157", 1});
        lib.open_loans.push_back({"9780306406157", 2});
        check_inconsistent(lib);
    }

    // (f) Member with more than kMaxBorrowed (5) loans
    {
        StoredLibrary lib;
        lib.members.emplace_back(1, "Alice");
        std::vector<std::string> isbns = {
            "9780306406157", "9780140449136", "9780201896831",
            "9780262033848", "9780131103627", "9780596007126"
        };
        for (const auto& isbn : isbns) {
            lib.books.emplace_back("Title", "Author", isbn, "Genre", true);
            lib.open_loans.push_back({isbn, 1});
        }
        check_inconsistent(lib);
    }
}

TEST_CASE("10. Copies of persistent service share the repository pointer") {
    auto repo = std::make_shared<RecordingRepository>();
    LibraryService service = LibraryService::open(repo);
    repo->calls.clear();

    LibraryService copy = service;
    copy.add_member(99, "Shared");

    REQUIRE(repo->calls.size() == 1);
    CHECK(repo->calls[0] == "add_member:99");
}

} // TEST_SUITE

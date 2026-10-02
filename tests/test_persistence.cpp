#include <doctest/doctest.h>
#include "service/LibraryService.h"
#include "repository/SqliteRepository.h"
#include "repository/RepositoryErrors.h"
#include "ui/ConsoleApp.h"
#include "ui/DemoData.h"
#include "tests/repository_fixtures.h"
#include "tests/library_fixtures.h"

#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace titans;
using namespace titans::fixtures;

namespace {

std::string make_isbn(int id) {
    std::string prefix = "978" + std::string(9 - std::to_string(id).length(), '0') + std::to_string(id);
    int sum = 0;
    for (int i = 0; i < 12; ++i) {
        int d = prefix[i] - '0';
        sum += (i % 2 == 0) ? d : d * 3;
    }
    int check = (10 - (sum % 10)) % 10;
    return prefix + std::to_string(check);
}

std::string run_script(LibraryService& service, const std::string& input, int* exit_code = nullptr) {
    std::istringstream in(input);
    std::ostringstream out;
    ConsoleApp app(service, in, out);
    int code = app.run();
    if (exit_code) {
        *exit_code = code;
    }
    return out.str();
}

} // namespace

TEST_SUITE("Persistence") {

TEST_CASE("1. open on empty file DB, populate, reopen without sorting matches exact snapshot") {
    TempDbPath temp;
    Snapshot saved_snapshot;
    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        CHECK(service.total_books() == 0);
        CHECK(service.member_count() == 0);

        service.add_member(1, "Alice");
        service.add_member(2, "Bob");
        service.add_book(Book("B1", "A1", make_isbn(1), "G1", true));
        service.add_book(Book("B2", "A2", make_isbn(2), "G2", true));
        service.add_book(Book("B3", "A3", make_isbn(3), "G3", true));

        service.borrow_book(make_isbn(1), 1);
        service.borrow_book(make_isbn(2), 2);
        service.return_book(make_isbn(1));
        service.borrow_book(make_isbn(1), 2);

        saved_snapshot = snapshot(service);
        check_consistency(service);
    }

    {
        auto repo2 = std::make_shared<SqliteRepository>(temp.path());
        LibraryService reloaded = LibraryService::open(repo2);
        CHECK(snapshot(reloaded) == saved_snapshot);
        check_consistency(reloaded);
    }
}

TEST_CASE("2. Availability, statistics, and member borrowed order survive restart") {
    TempDbPath temp;
    std::string isbn1 = make_isbn(1);
    std::string isbn2 = make_isbn(2);
    std::string isbn3 = make_isbn(3);

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        service.add_member(1, "Alice");
        service.add_book(Book("B1", "A1", isbn1, "G1", true));
        service.add_book(Book("B2", "A2", isbn2, "G2", true));
        service.add_book(Book("B3", "A3", isbn3, "G3", true));

        // Borrow 2 books in order: isbn1, then isbn2
        service.borrow_book(isbn1, 1);
        service.borrow_book(isbn2, 1);
        // Borrow and return isbn3
        service.borrow_book(isbn3, 1);
        service.return_book(isbn3);
    }

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);

        CHECK_FALSE(service.get_book(isbn1).is_available());
        CHECK_FALSE(service.get_book(isbn2).is_available());
        CHECK(service.get_book(isbn3).is_available());

        CHECK(service.total_books() == 3);
        CHECK(service.available_books() == 1);
        CHECK(service.borrowed_books() == 2);

        const Member& m = service.get_member(1);
        CHECK(m.borrowed_count() == 2);
        auto borrowed = borrowed_list(m);
        REQUIRE(borrowed.size() == 2);
        CHECK(borrowed[0] == isbn1);
        CHECK(borrowed[1] == isbn2);
        check_consistency(service);
    }
}

TEST_CASE("3. Removals persist, duplicates rejected, removed ISBN can be re-added") {
    TempDbPath temp;
    std::string isbn = make_isbn(1);
    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        service.add_member(1, "Alice");
        service.add_book(Book("Title", "Author", isbn, "Genre", true));
        service.remove_book(isbn);
        service.remove_member(1);
    }

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        CHECK_FALSE(service.has_book(isbn));
        CHECK_FALSE(service.has_member(1));

        // Removed items can be added again
        service.add_member(1, "Alice Again");
        service.add_book(Book("New Title", "New Author", isbn, "New Genre", true));
        CHECK(service.has_book(isbn));
        CHECK(service.has_member(1));

        // Duplicate rejection
        CHECK_THROWS_AS(service.add_member(1, "Dup"), DuplicateError);
        CHECK_THROWS_AS(service.add_book(Book("Dup", "Dup", isbn, "Dup", true)), DuplicateError);
    }
}

TEST_CASE("4. Failed operations write nothing to database") {
    TempDbPath temp;
    Snapshot before;
    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        service.add_member(1, "Alice");
        service.add_book(Book("B1", "A1", make_isbn(1), "G1", true));
        before = snapshot(service);

        // Series of failing calls
        CHECK_THROWS_AS(service.add_member(1, "Alice Dup"), DuplicateError);
        CHECK_THROWS_AS(service.add_book(Book("B1", "A1", make_isbn(1), "G1", true)), DuplicateError);
        CHECK_THROWS_AS(service.borrow_book(make_isbn(99), 1), NotFoundError);
        CHECK_THROWS_AS(service.return_book(make_isbn(1)), std::logic_error);
        CHECK_THROWS_AS(service.remove_book(make_isbn(99)), NotFoundError);
    }

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        CHECK(snapshot(service) == before);
    }
}

TEST_CASE("5. Sorting is not persisted: catalog returns in insertion order on reload") {
    TempDbPath temp;
    std::string isbn1 = make_isbn(1);
    std::string isbn2 = make_isbn(2);
    std::string isbn3 = make_isbn(3);

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        service.add_book(Book("Zebra", "Author", isbn1, "G", true));
        service.add_book(Book("Apple", "Author", isbn2, "G", true));
        service.add_book(Book("Mango", "Author", isbn3, "G", true));

        // Sort descending by title: Zebra, Mango, Apple
        service.sort_by_title(SortOrder::Descending);
        auto t = titles(service.books());
        REQUIRE(t.size() == 3);
        CHECK(t[0] == "Zebra");
        CHECK(t[1] == "Mango");
        CHECK(t[2] == "Apple");
    }

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService reloaded = LibraryService::open(repo);
        // Insertion order restored: Zebra, Apple, Mango
        auto t = titles(reloaded.books());
        REQUIRE(t.size() == 3);
        CHECK(t[0] == "Zebra");
        CHECK(t[1] == "Apple");
        CHECK(t[2] == "Mango");
    }
}

TEST_CASE("6. Demo data persists across restarts and second run adds 0") {
    TempDbPath temp;
    std::size_t initial_books = 0;
    std::size_t initial_members = 0;

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        auto res = load_demo_data(service);
        CHECK(res.books_added > 0);
        CHECK(res.members_added > 0);
        initial_books = service.total_books();
        initial_members = service.member_count();
    }

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        CHECK(service.total_books() == initial_books);
        CHECK(service.member_count() == initial_members);

        auto res2 = load_demo_data(service);
        CHECK(res2.books_added == 0);
        CHECK(res2.members_added == 0);
        CHECK(service.total_books() == initial_books);
        CHECK(service.member_count() == initial_members);
    }
}

TEST_CASE("7. Planted corrupt data throws DatabaseError or fails raw insert") {
    // (a) Open loan for a book that does not exist
    {
        TempDbPath temp;
        {
            SqliteRepository repo(temp.path());
            repo.add_member(Member(1, "Alice"));
        }
        raw_exec(temp.path(), "INSERT INTO loans(book_isbn, member_id) VALUES ('9780306406157', 1);");

        auto repo = std::make_shared<SqliteRepository>(temp.path());
        try {
            LibraryService::open(repo);
            FAIL("expected DatabaseError");
        } catch (const DatabaseError& e) {
            std::string msg = e.what();
            CHECK(msg.rfind("database contents are inconsistent: ", 0) == 0);
        }
    }

    // (b) Member with 6 open loans
    {
        TempDbPath temp;
        {
            SqliteRepository repo(temp.path());
            repo.add_member(Member(1, "Alice"));
            for (int i = 1; i <= 6; ++i) {
                repo.add_book(Book("B", "A", make_isbn(i), "G", true));
            }
        }
        for (int i = 1; i <= 6; ++i) {
            raw_exec(temp.path(), "INSERT INTO loans(book_isbn, member_id) VALUES ('" + make_isbn(i) + "', 1);");
        }

        auto repo = std::make_shared<SqliteRepository>(temp.path());
        try {
            LibraryService::open(repo);
            FAIL("expected DatabaseError");
        } catch (const DatabaseError& e) {
            std::string msg = e.what();
            CHECK(msg.rfind("database contents are inconsistent: ", 0) == 0);
        }
    }

    // (c) Two open loans for one book fails raw insert because of the partial unique index
    {
        TempDbPath temp;
        {
            SqliteRepository repo(temp.path());
            repo.add_member(Member(1, "Alice"));
            repo.add_member(Member(2, "Bob"));
            repo.add_book(Book("B", "A", make_isbn(1), "G", true));
        }
        raw_exec(temp.path(), "INSERT INTO loans(book_isbn, member_id) VALUES ('" + make_isbn(1) + "', 1);");
        CHECK_THROWS_AS(raw_exec(temp.path(), "INSERT INTO loans(book_isbn, member_id) VALUES ('" + make_isbn(1) + "', 2);"),
                        std::runtime_error);
    }
}

TEST_CASE("8. UI integration with persistence and failing repository") {
    TempDbPath temp;
    std::string isbn = make_isbn(50);

    // Script 1: Add member 101, add book, borrow it, exit
    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        std::string script =
            "9\n2\n101\nJohn Doe\n0\n"                     // Add member 101, back to main
            "2\nClean Code\nUncle Bob\n" + isbn + "\nTech\n" // Add book
            "5\n" + isbn + "\n101\n"                         // Borrow book
            "0\n";                                           // Exit
        run_script(service, script);
    }

    // Reopen and verify
    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        CHECK(service.has_member(101));
        CHECK(service.has_book(isbn));
        CHECK_FALSE(service.get_book(isbn).is_available());
        CHECK(service.get_member(101).has_borrowed(isbn));

        // Script 2: FailingRepository fails on write 1
        auto failing = std::make_shared<FailingRepository>(*repo, 1);
        LibraryService fail_svc = LibraryService::open(failing);
        Snapshot before = snapshot(fail_svc);

        std::string script = "9\n2\n202\nJane\n0\n0\n";
        std::string out = run_script(fail_svc, script);
        CHECK(out.find("Error: injected failure") != std::string::npos);
        CHECK(snapshot(fail_svc) == before);
    }
}

TEST_CASE("9. Randomized round-trip testing") {
    // 1. :memory: DB with 2000 operations
    {
        auto repo = std::make_shared<SqliteRepository>(":memory:");
        LibraryService live = LibraryService::open(repo);

        std::vector<Book> pool_books;
        for (int i = 1; i <= 30; ++i) {
            pool_books.push_back(Book("Title " + std::to_string(i),
                                      "Author " + std::to_string(i),
                                      make_isbn(i),
                                      (i % 2 == 0) ? "Fiction" : "Science",
                                      true));
        }

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> op_dist(0, 6);
        std::uniform_int_distribution<int> book_dist(0, 29);
        std::uniform_int_distribution<int> member_dist(1, 8);
        std::uniform_int_distribution<int> strat_dist(0, 2);
        std::uniform_int_distribution<int> order_dist(0, 1);
        std::uniform_int_distribution<int> algo_dist(0, 1);

        int successes[7] = {0};

        for (int step = 1; step <= 2000; ++step) {
            int op = op_dist(rng);
            try {
                switch (op) {
                    case 0: { // add_book
                        const auto& b = pool_books[static_cast<std::size_t>(book_dist(rng))];
                        live.add_book(b);
                        ++successes[0];
                        break;
                    }
                    case 1: { // remove_book
                        const auto& b = pool_books[static_cast<std::size_t>(book_dist(rng))];
                        live.remove_book(b.isbn());
                        ++successes[1];
                        break;
                    }
                    case 2: { // add_member
                        int m_id = member_dist(rng);
                        live.add_member(m_id, "Member " + std::to_string(m_id));
                        ++successes[2];
                        break;
                    }
                    case 3: { // remove_member
                        int m_id = member_dist(rng);
                        live.remove_member(m_id);
                        ++successes[3];
                        break;
                    }
                    case 4: { // borrow_book
                        const auto& b = pool_books[static_cast<std::size_t>(book_dist(rng))];
                        int m_id = member_dist(rng);
                        live.borrow_book(b.isbn(), m_id);
                        ++successes[4];
                        break;
                    }
                    case 5: { // return_book
                        const auto& b = pool_books[static_cast<std::size_t>(book_dist(rng))];
                        live.return_book(b.isbn());
                        ++successes[5];
                        break;
                    }
                    case 6: { // sort_books
                        SortOrder order = (order_dist(rng) == 0) ? SortOrder::Ascending : SortOrder::Descending;
                        SortAlgorithm algo = (algo_dist(rng) == 0) ? SortAlgorithm::Merge : SortAlgorithm::Insertion;
                        int strat = strat_dist(rng);
                        if (strat == 0) {
                            TitleSort s;
                            live.sort_books(s, order, algo);
                        } else if (strat == 1) {
                            AuthorSort s;
                            live.sort_books(s, order, algo);
                        } else {
                            GenreSort s;
                            live.sort_books(s, order, algo);
                        }
                        ++successes[6];
                        break;
                    }
                }
            } catch (const NotFoundError&) {
            } catch (const DuplicateError&) {
            } catch (const std::invalid_argument&) {
            } catch (const std::logic_error&) {
            }

            if (step % 25 == 0) {
                LibraryService reloaded = LibraryService::open(repo);
                INFO(describe_difference(live, reloaded));
                CHECK(same_state_ignoring_order(live, reloaded));
                check_consistency(reloaded);
            }
        }

        for (int i = 0; i < 7; ++i) {
            CHECK(successes[i] > 0);
        }
    }

    // 2. File DB with 300 operations and fresh SqliteRepository connection
    {
        TempDbPath temp;
        std::vector<Book> pool_books;
        for (int i = 1; i <= 15; ++i) {
            pool_books.push_back(Book("T" + std::to_string(i),
                                      "A" + std::to_string(i),
                                      make_isbn(i + 100),
                                      "G",
                                      true));
        }

        std::mt19937 rng(12345);
        std::uniform_int_distribution<int> op_dist(0, 5);
        std::uniform_int_distribution<int> book_dist(0, 14);
        std::uniform_int_distribution<int> member_dist(1, 5);

        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService live = LibraryService::open(repo);

        for (int step = 1; step <= 300; ++step) {
            int op = op_dist(rng);
            try {
                switch (op) {
                    case 0:
                        live.add_book(pool_books[static_cast<std::size_t>(book_dist(rng))]);
                        break;
                    case 1:
                        live.remove_book(pool_books[static_cast<std::size_t>(book_dist(rng))].isbn());
                        break;
                    case 2: {
                        int m = member_dist(rng);
                        live.add_member(m, "M" + std::to_string(m));
                        break;
                    }
                    case 3:
                        live.remove_member(member_dist(rng));
                        break;
                    case 4:
                        live.borrow_book(pool_books[static_cast<std::size_t>(book_dist(rng))].isbn(), member_dist(rng));
                        break;
                    case 5:
                        live.return_book(pool_books[static_cast<std::size_t>(book_dist(rng))].isbn());
                        break;
                }
            } catch (const NotFoundError&) {
            } catch (const DuplicateError&) {
            } catch (const std::invalid_argument&) {
            } catch (const std::logic_error&) {
            }

            if (step % 25 == 0) {
                auto reloaded_repo = std::make_shared<SqliteRepository>(temp.path());
                LibraryService reloaded = LibraryService::open(reloaded_repo);
                INFO(describe_difference(live, reloaded));
                CHECK(same_state_ignoring_order(live, reloaded));
                check_consistency(reloaded);
            }
        }
    }
}

TEST_CASE("10. SQL-injection style input stored, restored, and DB still works") {
    TempDbPath temp;
    std::string bad_title = "Title'; DROP TABLE books;--";
    std::string bad_author = "Author' OR 1=1;--";
    std::string isbn = make_isbn(90);

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        service.add_book(Book(bad_title, bad_author, isbn, "Tech", true));
    }

    {
        auto repo = std::make_shared<SqliteRepository>(temp.path());
        LibraryService service = LibraryService::open(repo);
        REQUIRE(service.has_book(isbn));
        CHECK(service.get_book(isbn).title() == bad_title);
        CHECK(service.get_book(isbn).author() == bad_author);

        // Verify DB still works normally
        service.add_member(1, "Post Injection Member");
        service.borrow_book(isbn, 1);
        CHECK_FALSE(service.get_book(isbn).is_available());
    }
}

} // TEST_SUITE

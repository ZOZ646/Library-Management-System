#include <doctest/doctest.h>
#include "repository/SqliteRepository.h"
#include "repository/RepositoryErrors.h"
#include "tests/repository_fixtures.h"
#include "domain/Book.h"
#include "domain/Member.h"

#include <fstream>
#include <memory>
#include <string>

using namespace titans;
using namespace titans::fixtures;

TEST_SUITE("SqliteRepository") {

TEST_CASE("1. Fresh :memory: database has empty load and is isolated") {
    SqliteRepository repo1(":memory:");
    StoredLibrary lib1 = repo1.load();
    CHECK(lib1.books.empty());
    CHECK(lib1.members.empty());
    CHECK(lib1.open_loans.empty());

    SqliteRepository repo2(":memory:");
    repo1.add_book(Book("Title", "Author", "978-0-306-40615-7", "Fiction", true));
    CHECK(repo1.load().books.size() == 1);
    CHECK(repo2.load().books.empty());
}

TEST_CASE("2. Fresh file database has correct schema and user_version = 1") {
    TempDbPath temp;
    {
        SqliteRepository repo(temp.path());
        CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='books';") == 1);
        CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='members';") == 1);
        CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='loans';") == 1);
        CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM sqlite_master WHERE type='index' AND name='one_open_loan_per_book';") == 1);
        CHECK(raw_scalar(temp.path(), "PRAGMA user_version;") == 1);
    }
}

TEST_CASE("3. Books insertion order, duplicate prevention, remove and error handling") {
    SqliteRepository repo(":memory:");
    Book b1("Book One", "Author A", "9780306406157", "Fiction", true);
    Book b2("Book Two", "Author B", "9780140449136", "History", true);
    Book b3("Book Three", "Author C", "9780201896831", "Science", true);

    repo.add_book(b1);
    repo.add_book(b2);
    repo.add_book(b3);

    auto lib = repo.load();
    REQUIRE(lib.books.size() == 3);
    CHECK(lib.books[0].isbn() == b1.isbn());
    CHECK(lib.books[1].isbn() == b2.isbn());
    CHECK(lib.books[2].isbn() == b3.isbn());
    CHECK(lib.books[0].is_available());

    // Duplicate ISBN
    CHECK_THROWS_AS(repo.add_book(Book("Duplicate", "Other", "9780306406157", "Art", true)), DatabaseError);
    CHECK(repo.load().books.size() == 3);

    // Remove middle book
    repo.remove_book("9780140449136");
    lib = repo.load();
    REQUIRE(lib.books.size() == 2);
    CHECK(lib.books[0].isbn() == b1.isbn());
    CHECK(lib.books[1].isbn() == b3.isbn());

    // Remove unknown book
    CHECK_THROWS_WITH_AS(repo.remove_book("9780140449136"), "book not found in database: 9780140449136", DatabaseError);
    CHECK(repo.load().books.size() == 2);
}

TEST_CASE("4. Members insertion order, duplicate prevention, remove and error handling") {
    SqliteRepository repo(":memory:");
    Member m1(10, "Alice");
    Member m2(20, "Bob");
    Member m3(30, "Charlie");

    repo.add_member(m1);
    repo.add_member(m2);
    repo.add_member(m3);

    auto lib = repo.load();
    REQUIRE(lib.members.size() == 3);
    CHECK(lib.members[0].id() == 10);
    CHECK(lib.members[1].id() == 20);
    CHECK(lib.members[2].id() == 30);
    CHECK(lib.members[0].name() == "Alice");

    CHECK_THROWS_AS(repo.add_member(Member(10, "Another Alice")), DatabaseError);
    CHECK(repo.load().members.size() == 3);

    repo.remove_member(20);
    lib = repo.load();
    REQUIRE(lib.members.size() == 2);
    CHECK(lib.members[0].id() == 10);
    CHECK(lib.members[1].id() == 30);

    CHECK_THROWS_WITH_AS(repo.remove_member(999), "member not found in database: 999", DatabaseError);
    CHECK(repo.load().members.size() == 2);
}

TEST_CASE("5. Round-trip fidelity: Arabic UTF-8, 10k-char title, SQL injection attempts") {
    TempDbPath temp;
    SqliteRepository repo(temp.path());

    // Arabic text (hex escaped UTF-8)
    std::string arabic_title = "\xd8\xa3\xd9\x84\xd9\x81 \xd9\x84\xd9\x8a\xd9\x84\xd8\xa9 \xd9\x88\xd9\x84\xd9\x8a\xd9\x84\xd8\xa9";
    std::string arabic_author = "\xd9\x86\xd8\xac\xd9\x8a\xd8\xa8 \xd9\x85\xd8\xad\xd9\x81\xd9\x88\xd8\xb8";
    repo.add_book(Book(arabic_title, arabic_author, "9780306406157", "Classics", true));

    // 10 000 character title
    std::string long_title(10000, 'X');
    repo.add_book(Book(long_title, "Author", "9780140449136", "Long", true));

    // SQL Injection attempt string
    std::string injection_title = "Robert'); DROP TABLE books;--";
    std::string injection_author = "Bobby'; SELECT * FROM members;--";
    repo.add_book(Book(injection_title, injection_author, "9780201896831", "Drama", true));

    auto lib = repo.load();
    REQUIRE(lib.books.size() == 3);
    CHECK(lib.books[0].title() == arabic_title);
    CHECK(lib.books[0].author() == arabic_author);
    CHECK(lib.books[1].title() == long_title);
    CHECK(lib.books[2].title() == injection_title);
    CHECK(lib.books[2].author() == injection_author);

    // Verify books table still exists
    CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='books';") == 1);
}

TEST_CASE("6. Loans tracking, history preservation, and constraints") {
    TempDbPath temp;
    SqliteRepository repo(temp.path());

    repo.add_book(Book("B1", "A1", "9780306406157", "G1", true));
    repo.add_book(Book("B2", "A2", "9780140449136", "G2", true));
    repo.add_member(Member(1, "M1"));
    repo.add_member(Member(2, "M2"));

    repo.record_borrow("9780306406157", 1);
    repo.record_borrow("9780140449136", 2);

    auto lib = repo.load();
    REQUIRE(lib.open_loans.size() == 2);
    CHECK(lib.open_loans[0].isbn == "9780306406157");
    CHECK(lib.open_loans[0].member_id == 1);
    CHECK(lib.open_loans[1].isbn == "9780140449136");
    CHECK(lib.open_loans[1].member_id == 2);

    // Return book 1
    repo.record_return("9780306406157");
    lib = repo.load();
    REQUIRE(lib.open_loans.size() == 1);
    CHECK(lib.open_loans[0].isbn == "9780140449136");
    CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM loans WHERE returned_at IS NOT NULL;") == 1);

    // Borrow book 1 again works
    repo.record_borrow("9780306406157", 2);
    CHECK(repo.load().open_loans.size() == 2);

    // Second OPEN loan for book 1 throws (partial unique index)
    CHECK_THROWS_AS(repo.record_borrow("9780306406157", 1), DatabaseError);

    // Foreign key enforcement: unknown ISBN or member throws
    CHECK_THROWS_AS(repo.record_borrow("9780000000000", 1), DatabaseError);
    CHECK_THROWS_AS(repo.record_borrow("9780306406157", 999), DatabaseError);

    // Record return on a book that has no open loan throws exact message
    CHECK_THROWS_WITH_AS(repo.record_return("9780000000000"), "no open loan for book: 9780000000000", DatabaseError);
}

TEST_CASE("7. Foreign key CASCADE deletes closed loan history") {
    TempDbPath temp;
    SqliteRepository repo(temp.path());

    repo.add_book(Book("B1", "A1", "9780306406157", "G1", true));
    repo.add_book(Book("B2", "A2", "9780140449136", "G2", true));
    repo.add_member(Member(1, "M1"));
    repo.add_member(Member(2, "M2"));

    repo.record_borrow("9780306406157", 1);
    repo.record_return("9780306406157");
    repo.record_borrow("9780140449136", 2);
    repo.record_return("9780140449136");

    CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM loans;") == 2);

    // Remove book 1 cascades its loans
    repo.remove_book("9780306406157");
    CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM loans WHERE book_isbn='9780306406157';") == 0);
    CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM loans;") == 1);

    // Remove member 2 cascades their loans
    repo.remove_member(2);
    CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM loans WHERE member_id=2;") == 0);
    CHECK(raw_scalar(temp.path(), "SELECT count(*) FROM loans;") == 0);
}

TEST_CASE("8. Persistence across separate connections") {
    TempDbPath temp;
    {
        SqliteRepository repo(temp.path());
        repo.add_book(Book("B1", "A1", "9780306406157", "G1", true));
        repo.add_member(Member(1, "M1"));
        repo.record_borrow("9780306406157", 1);
    }
    {
        SqliteRepository repo2(temp.path());
        auto lib = repo2.load();
        REQUIRE(lib.books.size() == 1);
        CHECK(lib.books[0].isbn() == "9780306406157");
        REQUIRE(lib.members.size() == 1);
        CHECK(lib.members[0].id() == 1);
        REQUIRE(lib.open_loans.size() == 1);
        CHECK(lib.open_loans[0].isbn == "9780306406157");
        CHECK(lib.open_loans[0].member_id == 1);
    }
}

TEST_CASE("9. Errors at open and reconnecting") {
    // Empty path
    CHECK_THROWS_WITH_AS(SqliteRepository(""), "database path must not be empty", std::invalid_argument);

    // Nonexistent directory
    CHECK_THROWS_AS(SqliteRepository("/nonexistent_dir_12345/file.db"), DatabaseError);

    // Corrupted file (plain text, not SQLite)
    TempDbPath plain_temp;
    {
        std::ofstream ofs(plain_temp.path());
        ofs << "This is not a sqlite database file.\n";
    }
    CHECK_THROWS_AS(SqliteRepository(plain_temp.path()), DatabaseError);

    // Unsupported user_version = 99
    TempDbPath ver_temp;
    {
        SqliteRepository repo(ver_temp.path());
    }
    raw_exec(ver_temp.path(), "PRAGMA user_version = 99;");
    CHECK_THROWS_WITH_AS(SqliteRepository(ver_temp.path()), "unsupported database version: 99", DatabaseError);

    // Opening valid DB twice does not duplicate or wipe data
    TempDbPath valid_temp;
    {
        SqliteRepository repo(valid_temp.path());
        repo.add_member(Member(5, "Existing"));
    }
    {
        SqliteRepository repo(valid_temp.path());
        CHECK(repo.load().members.size() == 1);
    }
}

TEST_CASE("10. load() of hand-corrupted data throws DatabaseError") {
    TempDbPath temp;
    {
        SqliteRepository repo(temp.path());
    }
    // Plant invalid ISBN row directly via raw_exec
    raw_exec(temp.path(), "INSERT INTO books(isbn, title, author, genre) VALUES ('abc', 'Bad Book', 'Bad Author', 'Bad');");

    SqliteRepository repo(temp.path());
    try {
        repo.load();
        FAIL("expected load() to throw DatabaseError");
    } catch (const DatabaseError& e) {
        std::string msg = e.what();
        CHECK(msg.rfind("corrupt data in database:", 0) == 0);
    }
}

} // TEST_SUITE

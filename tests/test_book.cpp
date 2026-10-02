#include <doctest/doctest.h>
#include "domain/Book.h"
#include "core/Container.h"

#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

namespace {

using titans::Book;
using titans::Container;

std::vector<std::string> book_titles_to_vector(const Container<Book>& c) {
    std::vector<std::string> result;
    for (const auto& b : c) {
        result.push_back(b.title());
    }
    return result;
}

std::vector<std::string> book_isbns_to_vector(const Container<Book>& c) {
    std::vector<std::string> result;
    for (const auto& b : c) {
        result.push_back(b.isbn());
    }
    return result;
}

} // namespace

TEST_CASE("1. Book construction and availability") {
    Book b1("The Hobbit", "J.R.R. Tolkien", "978-0-261-10221-7", "Fantasy");
    CHECK(b1.title() == "The Hobbit");
    CHECK(b1.author() == "J.R.R. Tolkien");
    CHECK(b1.isbn() == "9780261102217");
    CHECK(b1.genre() == "Fantasy");
    CHECK(b1.is_available() == true);

    Book b2("1984", "George Orwell", "0-452-28423-6", "Dystopian", false);
    CHECK(b2.title() == "1984");
    CHECK(b2.author() == "George Orwell");
    CHECK(b2.isbn() == "0452284236");
    CHECK(b2.genre() == "Dystopian");
    CHECK(b2.is_available() == false);
}

TEST_CASE("2. Book trimming of surrounding whitespace") {
    Book b("  Clean Code  ", "\t Robert C. Martin \n", "0-13-235088-2", "  Software Engineering  ");
    CHECK(b.title() == "Clean Code");
    CHECK(b.author() == "Robert C. Martin");
    CHECK(b.genre() == "Software Engineering");
}

TEST_CASE("3. Book blank title, author, genre validation") {
    CHECK_THROWS_WITH_AS(Book("", "Author", "0-306-40615-2", "Genre"), "title must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(Book("   \t\n  ", "Author", "0-306-40615-2", "Genre"), "title must not be empty", std::invalid_argument);

    CHECK_THROWS_WITH_AS(Book("Title", "", "0-306-40615-2", "Genre"), "author must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(Book("Title", "   \t\r ", "0-306-40615-2", "Genre"), "author must not be empty", std::invalid_argument);

    CHECK_THROWS_WITH_AS(Book("Title", "Author", "0-306-40615-2", ""), "genre must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(Book("Title", "Author", "0-306-40615-2", "   \n "), "genre must not be empty", std::invalid_argument);
}

TEST_CASE("4. ISBN normalization") {
    CHECK(Book::normalize_isbn("978-0-13-468599-1") == "9780134685991");
    CHECK(Book::normalize_isbn("0-306-40615-2") == "0306406152");
    CHECK(Book::normalize_isbn("  978 0 13 468599 1 ") == "9780134685991");
    CHECK(Book::normalize_isbn("0-8044-2957-x") == "080442957X");

    Book b("Title", "Author", "0-8044-2957-x", "Genre");
    CHECK(b.isbn() == "080442957X");
}

TEST_CASE("5. ISBN validity and error propagation") {
    const std::vector<std::string> valid_isbns = {
        "0306406152",
        "0-306-40615-2",
        "080442957X",
        "0-8044-2957-x",
        "9780134685991",
        "978-0-13-468599-1",
        " 978 0 13 468599 1 "
    };

    for (const auto& raw : valid_isbns) {
        CHECK(Book::is_valid_isbn(raw));
        CHECK_NOTHROW(Book("Valid", "Author", raw, "Genre"));
    }

    const std::vector<std::string> invalid_isbns = {
        "",
        "   \t\n ",
        "123456789",          // 9 chars
        "12345678901",        // 11 chars
        "123456789012",       // 12 chars
        "12345678901234",     // 14 chars
        "978-0-13-468A99-1",  // letter in 13-digit
        "0-30X-40615-2",      // X in middle of 10-char
        "978013468599X",      // X at end of 13-char
        "abc"
    };

    for (const auto& raw : invalid_isbns) {
        CHECK(!Book::is_valid_isbn(raw));
        std::string expected_msg = "invalid ISBN: " + raw;
        CHECK_THROWS_WITH_AS(Book::normalize_isbn(raw), expected_msg.c_str(), std::invalid_argument);
        CHECK_THROWS_WITH_AS(Book("Title", "Author", raw, "Genre"), expected_msg.c_str(), std::invalid_argument);
    }
}

TEST_CASE("6. Book setters validation and strong exception guarantee") {
    Book b("Original Title", "Original Author", "0306406152", "Original Genre");

    b.set_title("  New Title  ");
    CHECK(b.title() == "New Title");
    CHECK_THROWS_WITH_AS(b.set_title("   "), "title must not be empty", std::invalid_argument);
    CHECK(b.title() == "New Title");

    b.set_author("  New Author  ");
    CHECK(b.author() == "New Author");
    CHECK_THROWS_WITH_AS(b.set_author(""), "author must not be empty", std::invalid_argument);
    CHECK(b.author() == "New Author");

    b.set_genre("  New Genre  ");
    CHECK(b.genre() == "New Genre");
    CHECK_THROWS_WITH_AS(b.set_genre("\t\n"), "genre must not be empty", std::invalid_argument);
    CHECK(b.genre() == "New Genre");
}

TEST_CASE("7. Availability state machine") {
    Book b("Dune", "Frank Herbert", "0-441-17271-7", "Sci-Fi");
    CHECK(b.is_available() == true);

    b.mark_borrowed();
    CHECK(b.is_available() == false);
    CHECK_THROWS_WITH_AS(b.mark_borrowed(), "book is already borrowed", std::logic_error);
    CHECK(b.is_available() == false);

    b.mark_returned();
    CHECK(b.is_available() == true);
    CHECK_THROWS_WITH_AS(b.mark_returned(), "book is not borrowed", std::logic_error);
    CHECK(b.is_available() == true);

    // Full cycle again
    b.mark_borrowed();
    CHECK(b.is_available() == false);
}

TEST_CASE("8. Book equality based on ISBN identity") {
    Book b1("Title A", "Author A", "978-0-13-468599-1", "Genre A", true);
    Book b2("Title B", "Author B", "9780134685991", "Genre B", false);
    Book b3("Title A", "Author A", "0-306-40615-2", "Genre A", true);

    CHECK(b1 == b2);
    CHECK(b2 == b1);
    CHECK(b1 == b1);
    CHECK(!(b1 != b2));

    CHECK(b1 != b3);
    CHECK(b3 != b1);
    CHECK(!(b1 == b3));
}

TEST_CASE("9. Book copy/move behavior and type traits") {
    static_assert(std::is_copy_constructible_v<Book>, "Book should be copy constructible");
    static_assert(std::is_nothrow_move_constructible_v<Book>, "Book should be nothrow move constructible");
    static_assert(!std::is_default_constructible_v<Book>, "Book must not be default constructible");

    Book original("Clean Code", "Robert Martin", "0132350882", "Tech");
    Book copy = original;
    copy.set_title("Different Title");
    CHECK(original.title() == "Clean Code");
    CHECK(copy.title() == "Different Title");

    Book moved_target = std::move(original);
    CHECK(moved_target.title() == "Clean Code");
    CHECK(moved_target.author() == "Robert Martin");
    CHECK(moved_target.isbn() == "0132350882");
    CHECK(moved_target.genre() == "Tech");
}

TEST_CASE("10. Integration with Container<Book>") {
    Container<Book> shelf;

    shelf.push_back(Book("Foundation", "Isaac Asimov", "0-553-29335-4", "Sci-Fi"));
    shelf.push_back(Book("Neuromancer", "William Gibson", "0-441-56959-5", "Sci-Fi"));
    shelf.push_back(Book("Snow Crash", "Neal Stephenson", "0-553-38095-8", "Cyberpunk"));

    CHECK(shelf.size() == 3);

    // find with probe by ISBN
    Book probe("Dummy Title", "Dummy Author", "0-441-56959-5", "Other");
    auto it = shelf.find(probe);
    CHECK(it != shelf.end());
    CHECK(it->title() == "Neuromancer");

    // mutate through iterator
    CHECK(it->is_available() == true);
    it->mark_borrowed();
    CHECK(it->is_available() == false);
    CHECK(shelf.at(1).is_available() == false);

    // remove_value with probe
    CHECK(shelf.remove_value(probe) == true);
    CHECK(shelf.size() == 2);
    CHECK(shelf.find(probe) == shelf.end());

    // remove_value unknown probe returns false
    Book unknown("Unknown", "Unknown", "0-000-00000-0", "Unknown");
    CHECK(shelf.remove_value(unknown) == false);
    CHECK(shelf.size() == 2);

    // find_if by title
    auto it_sc = shelf.find_if([](const Book& b) { return b.title() == "Snow Crash"; });
    CHECK(it_sc != shelf.end());
    CHECK(it_sc->isbn() == "0553380958");

    // Stability test with at least 3 genres and 8 books
    Container<Book> library;
    library.push_back(Book("A1", "Auth", "1111111111", "Fiction"));
    library.push_back(Book("H8", "Auth", "8888888888", "History"));
    library.push_back(Book("C3", "Auth", "3333333333", "Fiction"));
    library.push_back(Book("D4", "Auth", "4444444444", "Science"));
    library.push_back(Book("E5", "Auth", "5555555555", "History"));
    library.push_back(Book("F6", "Auth", "6666666666", "Fiction"));
    library.push_back(Book("G7", "Auth", "7777777777", "Science"));
    library.push_back(Book("B2", "Auth", "2222222222", "History"));

    CHECK(library.size() == 8);

    // First sort by title
    library.sort([](const Book& lhs, const Book& rhs) {
        return lhs.title() < rhs.title();
    });

    std::vector<std::string> expected_titles = {"A1", "B2", "C3", "D4", "E5", "F6", "G7", "H8"};
    CHECK(book_titles_to_vector(library) == expected_titles);

    // Then stable sort by genre
    library.sort([](const Book& lhs, const Book& rhs) {
        return lhs.genre() < rhs.genre();
    });

    // In alphabetical order of genre:
    // Fiction: A1, C3, F6
    // History: B2, E5, H8
    // Science: D4, G7
    std::vector<std::string> expected_stable_titles = {"A1", "C3", "F6", "B2", "E5", "H8", "D4", "G7"};
    CHECK(book_titles_to_vector(library) == expected_stable_titles);

    // Container copy is deep
    Container<Book> library_copy = library;
    CHECK(library_copy.size() == 8);
    library_copy.front().set_title("Modified A1");
    CHECK(library_copy.front().title() == "Modified A1");
    CHECK(library.front().title() == "A1");

    CHECK(library.size() == 8);
    CHECK(library.front().title() == "A1");
    CHECK(library.back().title() == "G7");
}

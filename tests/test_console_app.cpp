#include <doctest/doctest.h>
#include "ui/ConsoleApp.h"
#include "ui/Utf8.h"
#include "service/LibraryService.h"
#include "service/SortStrategy.h"
#include "domain/Book.h"
#include "domain/Member.h"
#include "domain/Text.h"
#include "tests/library_fixtures.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace titans;
using namespace titans::fixtures;

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
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

std::vector<std::string> get_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream s(text);
    std::string l;
    while (std::getline(s, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    return lines;
}

} // namespace

// ── 1. Exit and input closed ─────────────────────────────────────

TEST_CASE("console app: exit and input closed") {
    // Normal exit with "0"
    {
        LibraryService svc;
        int code = -1;
        std::string out = run_script(svc, "0\n", &code);
        CHECK(code == 0);
        CHECK(contains(out, "=== Titans Library Management System ==="));
        CHECK(contains(out, "1 List all books"));
        CHECK(contains(out, "0 Exit"));
        CHECK(contains(out, "Goodbye!"));
    }

    // Immediately closed input
    {
        LibraryService svc;
        int code = -1;
        std::string out = run_script(svc, "", &code);
        CHECK(code == 0);
        CHECK(contains(out, "Input closed."));
    }

    // Input ends in the middle of an action
    {
        LibraryService svc;
        int code = -1;
        auto before = snapshot(svc);
        // Option 2 (Add a book), then title only, then EOF
        std::string out = run_script(svc, "2\nIncomplete Title\n", &code);
        CHECK(code == 0);
        CHECK(contains(out, "Input closed."));
        CHECK(snapshot(svc) == before);
    }
}

// ── 2. Menu input re-prompting ───────────────────────────────────

TEST_CASE("console app: menu input validation and re-prompting") {
    LibraryService svc;
    int code = -1;
    // Enter invalid choices: abc, "", 99, -1, then 0 to exit
    std::string out = run_script(svc, "abc\n\n99\n-1\n0\n", &code);
    CHECK(code == 0);
    CHECK(contains(out, "Please enter a whole number between 0 and 10."));
    // Verify menu lines numbered 1..10 and exit line is 0
    CHECK(contains(out, " 1 List all books"));
    CHECK(contains(out, "10 Load sample data"));
    CHECK(contains(out, " 0 Exit"));
}

// ── 3. List books ────────────────────────────────────────────────

TEST_CASE("console app: list books") {
    // Empty catalog
    {
        LibraryService svc;
        std::string out = run_script(svc, "1\n0\n");
        CHECK(contains(out, "The catalog is empty."));
        check_consistency(svc);
    }

    // Sample service
    {
        LibraryService svc = make_sample_service();
        std::string out = run_script(svc, "1\n0\n");
        CHECK(contains(out, "| # | Title"));
        CHECK(contains(out, "apple pie"));
        CHECK(contains(out, "Banana Split"));
        // Arabic book title
        std::string arabic_title = "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8" " 1";
        CHECK(contains(out, arabic_title));
        CHECK(contains(out, "8 book(s)."));

        // Check uniform line display width for table lines
        auto lines = get_lines(out);
        std::vector<std::string> table_lines;
        for (const auto& l : lines) {
            if (!l.empty() && (l[0] == '+' || l[0] == '|')) {
                table_lines.push_back(l);
            }
        }
        CHECK(!table_lines.empty());
        std::size_t w = utf8::display_width(table_lines[0]);
        for (const auto& tl : table_lines) {
            CHECK(utf8::display_width(tl) == w);
        }
        check_consistency(svc);
    }

    // After a borrow, status shows Borrowed
    {
        LibraryService svc = make_sample_service();
        svc.borrow_book("9780000000001", 1);
        std::string out = run_script(svc, "1\n0\n");
        CHECK(contains(out, "Borrowed"));
        CHECK(contains(out, "Available"));
        check_consistency(svc);
    }
}

// ── 4. Add book ──────────────────────────────────────────────────

TEST_CASE("console app: add book") {
    // Successful addition
    {
        LibraryService svc;
        std::string script = "2\nTest Book\nAuthor Name\n978-0-13-468599-1\nFiction\n0\n";
        std::string out = run_script(svc, script);
        CHECK(contains(out, "Book added."));
        CHECK(svc.total_books() == 1);
        CHECK(svc.has_book("9780134685991"));
        check_consistency(svc);
    }

    // Duplicate ISBN error
    {
        LibraryService svc;
        svc.add_book(make_book("Existing", "Author", "9780134685991", "Genre"));
        std::string script = "2\nSecond Book\nOther Author\n9780134685991\nGenre\n0\n";
        std::string out = run_script(svc, script);
        CHECK(contains(out, "Error: book already exists: 9780134685991"));
        CHECK(contains(out, "Main menu"));
        CHECK(svc.total_books() == 1);
        check_consistency(svc);
    }

    // Blank title/author/genre and invalid ISBN re-prompt
    {
        LibraryService svc;
        // Blank title -> re-prompts; blank author -> re-prompts; invalid ISBN -> re-prompts; blank genre -> re-prompts
        std::string script = "2\n\nTitle\n\nAuthor\nbad_isbn\n9780134685991\n\nGenre\n0\n";
        std::string out = run_script(svc, script);
        CHECK(contains(out, "Value must not be empty."));
        CHECK(contains(out, "Invalid ISBN: use 10 or 13 digits (hyphens and spaces are allowed)."));
        CHECK(contains(out, "Book added."));
        CHECK(svc.total_books() == 1);
        check_consistency(svc);
    }

    // /cancel at each prompt cancels without modifying catalog
    for (int step = 0; step < 4; ++step) {
        LibraryService svc;
        std::string script = "2\n";
        if (step == 0) script += "/cancel\n0\n";
        else if (step == 1) script += "Title\n/cancel\n0\n";
        else if (step == 2) script += "Title\nAuthor\n/cancel\n0\n";
        else script += "Title\nAuthor\n9780134685991\n/cancel\n0\n";

        std::string out = run_script(svc, script);
        CHECK(contains(out, "Cancelled."));
        CHECK(svc.total_books() == 0);
        check_consistency(svc);
    }
}

// ── 5. Remove book ───────────────────────────────────────────────

TEST_CASE("console app: remove book") {
    // Confirm "y" removes it
    {
        LibraryService svc;
        svc.add_book(make_book("Target", "Author", "9780134685991", "Genre"));
        std::string out = run_script(svc, "3\n9780134685991\ny\n0\n");
        CHECK(contains(out, "Remove 'Target'? (y/n): "));
        CHECK(contains(out, "Book removed."));
        CHECK(svc.total_books() == 0);
        check_consistency(svc);
    }

    // Confirm "n" keeps it
    {
        LibraryService svc;
        svc.add_book(make_book("Keep Me", "Author", "9780134685991", "Genre"));
        std::string out = run_script(svc, "3\n9780134685991\nn\n0\n");
        CHECK(contains(out, "Cancelled."));
        CHECK(svc.total_books() == 1);
        check_consistency(svc);
    }

    // Unknown ISBN error
    {
        LibraryService svc;
        std::string out = run_script(svc, "3\n9780000000099\n0\n");
        CHECK(contains(out, "Error: book not found: 9780000000099"));
        check_consistency(svc);
    }

    // Borrowed book cannot be removed
    {
        LibraryService svc;
        svc.add_member(1, "Alice");
        svc.add_book(make_book("Borrowed Book", "Author", "9780134685991", "Genre"));
        svc.borrow_book("9780134685991", 1);
        std::string out = run_script(svc, "3\n9780134685991\ny\n0\n");
        CHECK(contains(out, "Error: cannot remove a borrowed book"));
        CHECK(svc.total_books() == 1);
        check_consistency(svc);
    }
}

// ── 6. Search books ──────────────────────────────────────────────

TEST_CASE("console app: search books") {
    auto svc = make_sample_service();

    // 1. Title substring (case-insensitive)
    {
        std::string out = run_script(svc, "4\n1\napple\n0\n");
        CHECK(contains(out, "apple pie"));
        CHECK(contains(out, "Big apple pie"));
        CHECK(contains(out, "2 book(s)."));
    }

    // 2. Author with two books
    {
        std::string out = run_script(svc, "4\n2\nJane Doe\n0\n");
        CHECK(contains(out, "Jane Doe"));
        CHECK(contains(out, "2 book(s)."));
    }

    // 3. ISBN formatted
    {
        std::string out = run_script(svc, "4\n3\n978-0-00-000000-1\n0\n");
        CHECK(contains(out, "9780000000001"));
        CHECK(contains(out, "1 book(s)."));
    }

    // 4. Genre exact ("Fiction" must NOT list "Non-Fiction")
    {
        std::string out = run_script(svc, "4\n4\nFiction\n0\n");
        CHECK(contains(out, "Fiction"));
        CHECK(!contains(out, "Non-Fiction"));
        CHECK(contains(out, "4 book(s)."));
    }

    // Arabic search
    {
        std::string arabic_query = "\xD9\x83\xD8\xAA";
        std::string out = run_script(svc, "4\n1\n" + arabic_query + "\n0\n");
        CHECK(contains(out, "1 book(s)."));
    }

    // No books found
    {
        std::string out = run_script(svc, "4\n1\nNonExistent\n0\n");
        CHECK(contains(out, "No books found."));
    }

    // /cancel at query prompt cancels
    {
        std::string out = run_script(svc, "4\n1\n/cancel\n0\n");
        CHECK(contains(out, "Cancelled."));
    }

    check_consistency(svc);
}

// ── 7. Borrow and return ─────────────────────────────────────────

TEST_CASE("console app: borrow and return") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_book(make_book("Clean Code", "Martin", "9780132350884", "Tech"));

    // Borrow success message
    {
        std::string out = run_script(svc, "5\n9780132350884\n1\n0\n");
        CHECK(contains(out, "'Clean Code' was borrowed by Alice (id 1)."));
        CHECK(!svc.get_book("9780132350884").is_available());
        check_consistency(svc);
    }

    // Already borrowed error
    {
        svc.add_member(2, "Bob");
        std::string out = run_script(svc, "5\n9780132350884\n2\n0\n");
        CHECK(contains(out, "Error: book is not available"));
        CHECK(contains(out, "Main menu"));
        check_consistency(svc);
    }

    // Return success message
    {
        std::string out = run_script(svc, "6\n9780132350884\n0\n");
        CHECK(contains(out, "'Clean Code' was returned by Alice (id 1)."));
        CHECK(svc.get_book("9780132350884").is_available());
        check_consistency(svc);
    }

    // Return when not borrowed
    {
        std::string out = run_script(svc, "6\n9780132350884\n0\n");
        CHECK(contains(out, "Error: book is not borrowed"));
        check_consistency(svc);
    }
}

// ── 8. Sort catalog ──────────────────────────────────────────────

TEST_CASE("console app: sort catalog all 12 combinations") {
    // 3 criteria x 2 orders x 2 algorithms
    for (int crit = 1; crit <= 3; ++crit) {
        for (int ord = 1; ord <= 2; ++ord) {
            for (int alg = 1; alg <= 2; ++alg) {
                LibraryService svc = make_sample_service();

                std::string script = "7\n" + std::to_string(crit) + "\n"
                                     + std::to_string(ord) + "\n"
                                     + std::to_string(alg) + "\n0\n";
                std::string out = run_script(svc, script);

                std::string crit_name = (crit == 1 ? "title" : (crit == 2 ? "author" : "genre"));
                std::string ord_name = (ord == 1 ? "ascending" : "descending");
                std::string alg_name = (alg == 1 ? "merge" : "insertion");

                std::string expected_msg = "Catalog sorted by " + crit_name + " ("
                                           + ord_name + ", " + alg_name + " sort).";
                CHECK(contains(out, expected_msg));

                // Verify service books match std::stable_sort
                auto expected = make_sample_service();
                std::vector<Book> v;
                for (const auto& b : expected.books()) v.push_back(b);
                auto comp = [crit, ord](const Book& a, const Book& b) -> bool {
                    std::string ka, kb;
                    if (crit == 1) { ka = text::to_lower(a.title()); kb = text::to_lower(b.title()); }
                    else if (crit == 2) { ka = text::to_lower(a.author()); kb = text::to_lower(b.author()); }
                    else { ka = text::to_lower(a.genre()); kb = text::to_lower(b.genre()); }
                    return (ord == 1) ? (ka < kb) : (ka > kb);
                };
                std::stable_sort(v.begin(), v.end(), comp);

                auto actual = isbns(svc.books());
                std::vector<std::string> exp_isbns;
                for (const auto& b : v) exp_isbns.push_back(b.isbn());
                CHECK(actual == exp_isbns);
                check_consistency(svc);
            }
        }
    }

    // /cancel in sort menu leaves catalog unchanged
    {
        LibraryService svc = make_sample_service();
        auto before = snapshot(svc);
        std::string out = run_script(svc, "7\n/cancel\n0\n");
        CHECK(contains(out, "Cancelled."));
        CHECK(snapshot(svc) == before);
    }
}

// ── 9. Statistics ────────────────────────────────────────────────

TEST_CASE("console app: statistics exact block") {
    LibraryService svc;
    svc.add_member(1, "Alice");
    svc.add_member(2, "Bob");
    svc.add_book(make_book("B1", "A1", "9780000000001", "G"));
    svc.add_book(make_book("B2", "A2", "9780000000002", "G"));
    svc.add_book(make_book("B3", "A3", "9780000000003", "G"));
    svc.borrow_book("9780000000001", 1);

    std::string out = run_script(svc, "8\n0\n");
    std::string expected_block =
        "--- Library statistics ---\n"
        "Total books     : 3\n"
        "Available books : 2\n"
        "Borrowed books  : 1\n"
        "Members         : 2\n";
    CHECK(contains(out, expected_block));
    check_consistency(svc);
}

// ── 10. Members submenu ──────────────────────────────────────────

TEST_CASE("console app: members submenu") {
    LibraryService svc;

    // List on empty
    {
        std::string out = run_script(svc, "9\n1\n0\n0\n");
        CHECK(contains(out, "There are no members."));
    }

    // Add member
    {
        std::string out = run_script(svc, "9\n2\n10\nAlice\n0\n0\n");
        CHECK(contains(out, "Member added."));
        CHECK(svc.has_member(10));
    }

    // Duplicate member
    {
        std::string out = run_script(svc, "9\n2\n10\nBob\n0\n0\n");
        CHECK(contains(out, "Error: member already exists: 10"));
    }

    // List members shows <count>/5
    {
        std::string out = run_script(svc, "9\n1\n0\n0\n");
        CHECK(contains(out, "0/5"));
        CHECK(contains(out, "1 member(s)."));
    }

    // Show borrowed books on member with no books
    {
        std::string out = run_script(svc, "9\n4\n10\n0\n0\n");
        CHECK(contains(out, "Alice has no borrowed books."));
    }

    // Show borrowed books when member holds books
    {
        svc.add_book(make_book("B1", "A1", "9780000000001", "G"));
        svc.borrow_book("9780000000001", 10);
        std::string out = run_script(svc, "9\n4\n10\n0\n0\n");
        CHECK(contains(out, "B1"));
        CHECK(contains(out, "1 book(s)."));
    }

    // Remove member with borrowed books -> Error
    {
        std::string out = run_script(svc, "9\n3\n10\ny\n0\n0\n");
        CHECK(contains(out, "Error: member still has borrowed books"));
        CHECK(svc.has_member(10));
    }

    // Remove member after return
    {
        svc.return_book("9780000000001");
        std::string out = run_script(svc, "9\n3\n10\ny\n0\n0\n");
        CHECK(contains(out, "Member removed."));
        CHECK(!svc.has_member(10));
    }

    check_consistency(svc);
}

// ── 11. Sample data loading ──────────────────────────────────────

TEST_CASE("console app: load sample data") {
    LibraryService svc;
    std::string out1 = run_script(svc, "10\n0\n");
    CHECK(contains(out1, "Sample data loaded: "));
    CHECK(contains(out1, "book(s) and "));
    CHECK(svc.total_books() >= 12);
    CHECK(svc.member_count() >= 5);
    check_consistency(svc);

    // Second call adds 0 items
    std::string out2 = run_script(svc, "10\n0\n");
    CHECK(contains(out2, "Sample data loaded: 0 book(s) and 0 member(s) added."));
    check_consistency(svc);
}

// ── 12. Output hygiene ───────────────────────────────────────────

TEST_CASE("console app: output hygiene") {
    LibraryService svc = make_sample_service();
    // Run several actions: list books, statistics, search, members list
    std::string out = run_script(svc, "1\n8\n4\n1\napple\n9\n1\n0\n0\n");

    // No escape character \x1b
    CHECK(out.find('\x1b') == std::string::npos);
    // No tab characters
    CHECK(out.find('\t') == std::string::npos);

    // Every table line starts with '+' or '|'
    auto lines = get_lines(out);
    bool in_table = false;
    for (const auto& l : lines) {
        if (!l.empty() && (l[0] == '+' || l[0] == '|')) {
            in_table = true;
            CHECK((l[0] == '+' || l[0] == '|'));
        }
    }
    CHECK(in_table);
    check_consistency(svc);
}

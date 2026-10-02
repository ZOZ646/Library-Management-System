#include <doctest/doctest.h>
#include "service/LibraryService.h"
#include "service/LibraryErrors.h"
#include "service/SortStrategy.h"
#include "service/SearchStrategy.h"
#include "domain/Book.h"
#include "domain/Member.h"
#include "domain/Text.h"
#include "tests/library_fixtures.h"

#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace titans;
using namespace titans::fixtures;

} // namespace

// ── 1. End-to-End Story (Future Demo) ────────────────────────────

TEST_CASE("scenario: end-to-end library workflow") {
    LibraryService service;
    check_consistency(service);

    // 1. Register members
    service.add_member(1, "Alice Smith");
    service.add_member(2, "Bob Jones");
    service.add_member(3, "Charlie Brown");
    CHECK(service.member_count() == 3);
    check_consistency(service);

    // 2. Add books
    service.add_book(make_book("The C++ Programming Language", "Bjarne Stroustrup", "9780321563842", "Programming"));
    service.add_book(make_book("Design Patterns", "Gang of Four", "9780201633610", "Programming"));
    service.add_book(make_book("Clean Code", "Robert C. Martin", "9780132350884", "Programming"));
    service.add_book(make_book("Dune", "Frank Herbert", "9780441013593", "Sci-Fi"));
    service.add_book(make_book("Foundation", "Isaac Asimov", "9780553293357", "Sci-Fi"));
    service.add_book(make_book("1984", "George Orwell", "9780451524935", "Fiction"));
    service.add_book(make_book("Animal Farm", "George Orwell", "9780451526342", "Fiction"));
    CHECK(service.total_books() == 7);
    check_consistency(service);

    // 3. Search several ways
    // By title substring
    auto prog_books = service.search_by_title("Programming");
    CHECK(prog_books.size() == 1);
    CHECK(prog_books.front().isbn() == "9780321563842");

    // By author
    auto orwell_books = service.search_by_author("Orwell");
    CHECK(orwell_books.size() == 2);

    // By genre (exact)
    auto scifi_books = service.search_by_genre("Sci-Fi");
    CHECK(scifi_books.size() == 2);

    // By ISBN (formatted)
    auto found_dune = service.search_by_isbn("978-0-441-01359-3");
    CHECK(found_dune.size() == 1);
    CHECK(found_dune.front().title() == "Dune");

    check_consistency(service);

    // 4. Borrow books
    service.borrow_book("9780321563842", 1);  // Alice borrows C++
    service.borrow_book("9780441013593", 2);  // Bob borrows Dune
    CHECK(!service.get_book("9780321563842").is_available());
    CHECK(!service.get_book("9780441013593").is_available());
    CHECK(service.get_member(1).has_borrowed("9780321563842"));
    CHECK(service.get_member(2).has_borrowed("9780441013593"));
    check_consistency(service);

    // 5. Try forbidden borrow operations
    auto snap_before = snapshot(service);
    // Already borrowed by Alice, Bob tries to borrow
    CHECK_THROWS_WITH_AS(service.borrow_book("9780321563842", 2),
                         "book is not available", std::logic_error);
    CHECK(snapshot(service) == snap_before);

    // Unknown member
    CHECK_THROWS_WITH_AS(service.borrow_book("9780201633610", 999),
                         "member not found: 999", NotFoundError);
    CHECK(snapshot(service) == snap_before);

    // Unknown book
    CHECK_THROWS_WITH_AS(service.borrow_book("9780000000099", 1),
                         "book not found: 9780000000099", NotFoundError);
    CHECK(snapshot(service) == snap_before);
    check_consistency(service);

    // 6. Check statistics
    CHECK(service.total_books() == 7);
    CHECK(service.available_books() == 5);
    CHECK(service.borrowed_books() == 2);
    CHECK(service.member_count() == 3);
    check_consistency(service);

    // 7. Sort
    service.sort_by_title(SortOrder::Ascending, SortAlgorithm::Merge);
    check_consistency(service);

    // 8. Return
    int holder = service.return_book("978-0-321-56384-2");
    CHECK(holder == 1);
    CHECK(service.get_book("9780321563842").is_available());
    CHECK(!service.get_member(1).has_borrowed("9780321563842"));
    check_consistency(service);

    // 9. Remove an available book
    service.remove_book("9780321563842");
    CHECK(!service.has_book("9780321563842"));
    check_consistency(service);

    // 10. Check statistics again
    CHECK(service.total_books() == 6);
    CHECK(service.available_books() == 5);
    CHECK(service.borrowed_books() == 1);
    CHECK(service.member_count() == 3);
    check_consistency(service);
}

// ── 2. Randomized Consistency Test ───────────────────────────────

TEST_CASE("scenario: randomized consistency stress test (3000 operations)") {
    LibraryService service;
    std::mt19937 rng(424242);

    // Pool of 40 books with 13-digit ISBNs
    std::vector<Book> book_pool;
    book_pool.reserve(40);
    const std::vector<std::string> genres = {"Fiction", "Non-Fiction", "Science", "History"};
    for (int i = 0; i < 40; ++i) {
        char isbn_buf[14];
        std::snprintf(isbn_buf, sizeof(isbn_buf), "97810000000%02d", i);
        std::string title = "Book " + std::to_string(i);
        std::string author = "Author " + std::to_string(i % 5);
        std::string genre = genres[static_cast<std::size_t>(i % 4)];
        book_pool.push_back(make_book(title, author, isbn_buf, genre));
    }

    // Member IDs 1..8
    const int kNumMemberIds = 8;

    int add_book_success = 0;
    int remove_book_success = 0;
    int add_member_success = 0;
    int remove_member_success = 0;
    int borrow_book_success = 0;
    int return_book_success = 0;
    int sort_books_success = 0;

    const std::vector<std::string> sort_keys = {"title", "author", "genre"};

    for (int step = 0; step < 3000; ++step) {
        Snapshot snap_before = snapshot(service);
        bool threw = false;

        // Choose random operation: 0..6
        int op = static_cast<int>(rng() % 7);

        try {
            switch (op) {
                case 0: { // add_book
                    int idx = static_cast<int>(rng() % book_pool.size());
                    service.add_book(book_pool[static_cast<std::size_t>(idx)]);
                    ++add_book_success;
                    break;
                }
                case 1: { // remove_book
                    int idx = static_cast<int>(rng() % book_pool.size());
                    service.remove_book(book_pool[static_cast<std::size_t>(idx)].isbn());
                    ++remove_book_success;
                    break;
                }
                case 2: { // add_member
                    int mid = 1 + static_cast<int>(rng() % kNumMemberIds);
                    service.add_member(mid, "Member " + std::to_string(mid));
                    ++add_member_success;
                    break;
                }
                case 3: { // remove_member
                    int mid = 1 + static_cast<int>(rng() % kNumMemberIds);
                    service.remove_member(mid);
                    ++remove_member_success;
                    break;
                }
                case 4: { // borrow_book
                    int idx = static_cast<int>(rng() % book_pool.size());
                    int mid = 1 + static_cast<int>(rng() % kNumMemberIds);
                    service.borrow_book(book_pool[static_cast<std::size_t>(idx)].isbn(), mid);
                    ++borrow_book_success;
                    break;
                }
                case 5: { // return_book
                    int idx = static_cast<int>(rng() % book_pool.size());
                    service.return_book(book_pool[static_cast<std::size_t>(idx)].isbn());
                    ++return_book_success;
                    break;
                }
                case 6: { // sort_books
                    int s_idx = static_cast<int>(rng() % sort_keys.size());
                    auto strat = SortStrategyFactory::create(sort_keys[static_cast<std::size_t>(s_idx)]);
                    SortOrder order = (rng() % 2 == 0) ? SortOrder::Ascending : SortOrder::Descending;
                    SortAlgorithm algo = (rng() % 2 == 0) ? SortAlgorithm::Merge : SortAlgorithm::Insertion;
                    service.sort_books(*strat, order, algo);
                    ++sort_books_success;
                    break;
                }
            }
        } catch (const NotFoundError&) {
            threw = true;
        } catch (const DuplicateError&) {
            threw = true;
        } catch (const std::invalid_argument&) {
            threw = true;
        } catch (const std::logic_error&) {
            threw = true;
        } catch (...) {
            FAIL("Unexpected exception type thrown during operation");
        }

        if (threw) {
            CHECK(snapshot(service) == snap_before);
        }

        check_consistency(service);
    }

    // Verify every operation succeeded at least once (non-vacuous test)
    CHECK(add_book_success > 0);
    CHECK(remove_book_success > 0);
    CHECK(add_member_success > 0);
    CHECK(remove_member_success > 0);
    CHECK(borrow_book_success > 0);
    CHECK(return_book_success > 0);
    CHECK(sort_books_success > 0);
}

// ── 3. Lifecycle ─────────────────────────────────────────────────

TEST_CASE("scenario: lifecycle copy-construct and independence") {
    LibraryService original = make_sample_service();
    check_consistency(original);

    // Initial snapshot of original
    Snapshot orig_before = snapshot(original);

    // Copy-construct
    LibraryService copy(original);
    CHECK(snapshot(copy) == orig_before);
    check_consistency(copy);

    // Mutate the copy: borrow, sort, remove, add member
    copy.borrow_book("9780000000001", 1);
    copy.sort_by_title(SortOrder::Descending, SortAlgorithm::Merge);
    copy.remove_book("9780000000002");
    copy.add_member(99, "Newbie Member");

    // Copy reflects mutations
    CHECK(!copy.get_book("9780000000001").is_available());
    CHECK(!copy.has_book("9780000000002"));
    CHECK(copy.has_member(99));
    CHECK(copy.get_member(1).has_borrowed("9780000000001"));
    check_consistency(copy);

    // Original is completely unchanged and stays consistent
    CHECK(snapshot(original) == orig_before);
    CHECK(original.get_book("9780000000001").is_available());
    CHECK(original.has_book("9780000000002"));
    CHECK(!original.has_member(99));
    CHECK(!original.get_member(1).has_borrowed("9780000000001"));
    check_consistency(original);
}

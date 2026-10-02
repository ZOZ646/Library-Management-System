#pragma once

#include "core/Container.h"
#include "domain/Book.h"
#include "domain/Member.h"
#include "service/LibraryErrors.h"
#include "service/SortStrategy.h"
#include "service/SearchStrategy.h"

#include <cstddef>
#include <string>

namespace titans {

enum class SortOrder { Ascending, Descending };
enum class SortAlgorithm { Merge, Insertion };

/*
 * LibraryService: Facade over the book catalog and the member registry.
 * This is the ONLY entry point the UI will use; it hides the Container,
 * the strategies and the domain rules.
 *
 * Invariants (maintained after every public call, including calls that throw):
 *   I1. ISBNs are unique in the catalog and member ids are unique.
 *   I2. A book is unavailable iff exactly one member holds its ISBN.
 *   I3. Every ISBN in a member's borrowed list belongs to a catalog book.
 *   I4. No member holds more than Member::kMaxBorrowed books.
 *
 * Exception guarantee: every mutating function has the STRONG guarantee:
 * it validates everything first and changes state afterwards; when two
 * objects must change, the step that can throw runs first. All invariants
 * hold after every public call, including calls that throw. Nobody outside
 * the class can break them, because only const access to the data is exposed.
 */
class LibraryService {
public:
    LibraryService() = default;

    // ── Members ──────────────────────────────────────────────────────

    // Adds a new member. Throws std::invalid_argument for bad id/name.
    // Throws DuplicateError if the id already exists.   // O(m)
    void add_member(int id, std::string name);

    // Removes a member. Throws NotFoundError if not found.
    // Throws std::logic_error if the member still has borrowed books.   // O(m)
    void remove_member(int id);

    // Returns true if a member with the given id exists.   // O(m)
    bool has_member(int id) const;

    // Returns the member. Throws NotFoundError if not found.
    // The reference stays valid until that member is removed.   // O(m)
    const Member& get_member(int id) const;

    // Number of registered members.   // O(1)
    std::size_t member_count() const noexcept;

    // Read-only access to the member list.   // O(1)
    const Container<Member>& members() const noexcept;

    // ── Books ────────────────────────────────────────────────────────

    // Adds a book to the catalog (appended at the END).
    // Throws std::invalid_argument if the book is not available.
    // Throws DuplicateError if the ISBN already exists.
    // Note: if the catalog was sorted it is no longer sorted after an add.   // O(n)
    void add_book(Book book);

    // Removes a book. Throws std::invalid_argument for invalid ISBN.
    // Throws NotFoundError if not found.
    // Throws std::logic_error if the book is borrowed.   // O(n)
    void remove_book(const std::string& isbn);

    // Returns true if a book with the given ISBN exists.
    // Returns false for an invalid ISBN string (does not throw).   // O(n)
    bool has_book(const std::string& isbn) const;

    // Returns the book. Throws std::invalid_argument for invalid ISBN.
    // Throws NotFoundError if not found.
    // The reference stays valid until that book is removed.   // O(n)
    const Book& get_book(const std::string& isbn) const;

    // Total number of books in the catalog.   // O(1)
    std::size_t total_books() const noexcept;

    // Read-only access to the book catalog.   // O(1)
    const Container<Book>& books() const noexcept;

    // ── Borrowing ────────────────────────────────────────────────────

    // Borrows a book for a member.
    // Throws std::invalid_argument for invalid ISBN.
    // Throws NotFoundError for unknown member or book.
    // Throws std::logic_error if book is not available or limit reached.   // O(n + m + k)
    void borrow_book(const std::string& isbn, int member_id);

    // Returns a book and yields the id of the member who held it.
    // Throws std::invalid_argument for invalid ISBN.
    // Throws NotFoundError for unknown book.
    // Throws std::logic_error if book is not borrowed.   // O(n + m*k)
    int return_book(const std::string& isbn);

    // ── Statistics ───────────────────────────────────────────────────

    // Number of available books.   // O(n)
    std::size_t available_books() const noexcept;

    // Number of borrowed books (total minus available).   // O(n)
    std::size_t borrowed_books() const noexcept;

    // ── Queries (results are independent copies in catalog order) ────

    // Searches the catalog with a strategy. The query is trimmed; blank -> std::invalid_argument.
    // No match -> empty container.   // O(n)
    Container<Book> search(const ISearchStrategy& strategy, const std::string& query) const;

    // Convenience search wrappers.   // O(n) each
    Container<Book> search_by_title(const std::string& query) const;
    Container<Book> search_by_author(const std::string& query) const;
    Container<Book> search_by_isbn(const std::string& query) const;
    Container<Book> search_by_genre(const std::string& query) const;

    // ── Sorting (reorders the stored catalog in place) ───────────────

    // Sorts the catalog using a strategy.
    // Books are relinked, never copied; references from get_book stay valid.
    // The sort is stable.   // O(n log n) Merge, O(n^2) Insertion
    void sort_books(const ISortStrategy& strategy,
                    SortOrder order = SortOrder::Ascending,
                    SortAlgorithm algorithm = SortAlgorithm::Merge);

    // Convenience sort wrappers.
    void sort_by_title(SortOrder order = SortOrder::Ascending,
                       SortAlgorithm algorithm = SortAlgorithm::Merge);
    void sort_by_author(SortOrder order = SortOrder::Ascending,
                        SortAlgorithm algorithm = SortAlgorithm::Merge);
    void sort_by_genre(SortOrder order = SortOrder::Ascending,
                       SortAlgorithm algorithm = SortAlgorithm::Merge);

private:
    Container<Book> books_;
    Container<Member> members_;

    // Private lookup helpers returning iterators (non-const and const).
    // isbn MUST already be normalized.
    Container<Book>::Iterator find_book(const std::string& normalized_isbn);
    Container<Book>::ConstIterator find_book(const std::string& normalized_isbn) const;

    Container<Member>::Iterator find_member(int id);
    Container<Member>::ConstIterator find_member(int id) const;
};

} // namespace titans

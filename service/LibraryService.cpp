#include "service/LibraryService.h"
#include "domain/Text.h"

#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace titans {

// ── Private helpers ──────────────────────────────────────────────

Container<Book>::Iterator LibraryService::find_book(const std::string& normalized_isbn) {
    return books_.find_if([&normalized_isbn](const Book& b) {
        return b.isbn() == normalized_isbn;
    });
}

Container<Book>::ConstIterator LibraryService::find_book(const std::string& normalized_isbn) const {
    return books_.find_if([&normalized_isbn](const Book& b) {
        return b.isbn() == normalized_isbn;
    });
}

Container<Member>::Iterator LibraryService::find_member(int id) {
    return members_.find_if([id](const Member& m) {
        return m.id() == id;
    });
}

Container<Member>::ConstIterator LibraryService::find_member(int id) const {
    return members_.find_if([id](const Member& m) {
        return m.id() == id;
    });
}

// ── Members ──────────────────────────────────────────────────────

void LibraryService::add_member(int id, std::string name) {
    Member member(id, std::move(name));  // validates id and name
    if (find_member(id) != members_.end()) {
        throw DuplicateError("member already exists: " + std::to_string(id));
    }
    members_.push_back(std::move(member));
}

void LibraryService::remove_member(int id) {
    auto it = find_member(id);
    if (it == members_.end()) {
        throw NotFoundError("member not found: " + std::to_string(id));
    }
    if (it->borrowed_count() > 0) {
        throw std::logic_error("member still has borrowed books");
    }
    members_.remove_at(static_cast<std::size_t>(std::distance(members_.begin(), it)));
}

bool LibraryService::has_member(int id) const {
    return find_member(id) != members_.end();
}

const Member& LibraryService::get_member(int id) const {
    auto it = find_member(id);
    if (it == members_.end()) {
        throw NotFoundError("member not found: " + std::to_string(id));
    }
    return *it;
}

std::size_t LibraryService::member_count() const noexcept {
    return members_.size();
}

const Container<Member>& LibraryService::members() const noexcept {
    return members_;
}

// ── Books ────────────────────────────────────────────────────────

void LibraryService::add_book(Book book) {
    if (!book.is_available()) {
        throw std::invalid_argument("a new book must be available");
    }
    if (find_book(book.isbn()) != books_.end()) {
        throw DuplicateError("book already exists: " + book.isbn());
    }
    books_.push_back(std::move(book));
}

void LibraryService::remove_book(const std::string& isbn) {
    std::string normalized = Book::normalize_isbn(isbn);
    auto it = find_book(normalized);
    if (it == books_.end()) {
        throw NotFoundError("book not found: " + normalized);
    }
    if (!it->is_available()) {
        throw std::logic_error("cannot remove a borrowed book");
    }
    books_.remove_at(static_cast<std::size_t>(std::distance(books_.begin(), it)));
}

bool LibraryService::has_book(const std::string& isbn) const {
    if (!Book::is_valid_isbn(isbn)) {
        return false;
    }
    std::string normalized;
    try {
        normalized = Book::normalize_isbn(isbn);
    } catch (...) {
        return false;
    }
    return find_book(normalized) != books_.end();
}

const Book& LibraryService::get_book(const std::string& isbn) const {
    std::string normalized = Book::normalize_isbn(isbn);
    auto it = find_book(normalized);
    if (it == books_.end()) {
        throw NotFoundError("book not found: " + normalized);
    }
    return *it;
}

std::size_t LibraryService::total_books() const noexcept {
    return books_.size();
}

const Container<Book>& LibraryService::books() const noexcept {
    return books_;
}

// ── Borrowing ────────────────────────────────────────────────────

void LibraryService::borrow_book(const std::string& isbn, int member_id) {
    // (1) Normalize ISBN
    std::string normalized = Book::normalize_isbn(isbn);
    // (2) Member lookup
    auto mit = find_member(member_id);
    if (mit == members_.end()) {
        throw NotFoundError("member not found: " + std::to_string(member_id));
    }
    // (3) Book lookup
    auto bit = find_book(normalized);
    if (bit == books_.end()) {
        throw NotFoundError("book not found: " + normalized);
    }
    // (4) Book availability
    if (!bit->is_available()) {
        throw std::logic_error("book is not available");
    }
    // (5) Member borrow limit
    if (!mit->can_borrow()) {
        throw std::logic_error("borrow limit reached");
    }
    // (6) Apply: member first (can throw for duplicate), then book
    mit->add_borrowed(normalized);
    bit->mark_borrowed();
}

int LibraryService::return_book(const std::string& isbn) {
    // (1) Normalize
    std::string normalized = Book::normalize_isbn(isbn);
    // (2) Book lookup
    auto bit = find_book(normalized);
    if (bit == books_.end()) {
        throw NotFoundError("book not found: " + normalized);
    }
    // (3) Book must be borrowed
    if (bit->is_available()) {
        throw std::logic_error("book is not borrowed");
    }
    // (4) Find the holder
    auto mit = members_.find_if([&normalized](const Member& m) {
        return m.has_borrowed(normalized);
    });
    // Invariant guard: if the book is marked borrowed, exactly one member must hold it.
    // This cannot be triggered through the public API.
    if (mit == members_.end()) {
        throw std::logic_error("inconsistent state: borrowed book has no borrower");
    }
    int holder_id = mit->id();
    // (5) Apply: remove from member first, then mark book returned
    mit->remove_borrowed(normalized);
    bit->mark_returned();
    return holder_id;
}

// ── Statistics ───────────────────────────────────────────────────

std::size_t LibraryService::available_books() const noexcept {
    std::size_t count = 0;
    for (const auto& book : books_) {
        if (book.is_available()) {
            ++count;
        }
    }
    return count;
}

std::size_t LibraryService::borrowed_books() const noexcept {
    return books_.size() - available_books();
}

// ── Queries ──────────────────────────────────────────────────────

Container<Book> LibraryService::search(const ISearchStrategy& strategy, const std::string& query) const {
    std::string trimmed_query = text::require_non_blank(query, "search query");
    Container<Book> result;
    for (const auto& book : books_) {
        if (strategy.matches(book, trimmed_query)) {
            result.push_back(book);
        }
    }
    return result;
}

Container<Book> LibraryService::search_by_title(const std::string& query) const {
    TitleSearch strategy;
    return search(strategy, query);
}

Container<Book> LibraryService::search_by_author(const std::string& query) const {
    AuthorSearch strategy;
    return search(strategy, query);
}

Container<Book> LibraryService::search_by_isbn(const std::string& query) const {
    IsbnSearch strategy;
    return search(strategy, query);
}

Container<Book> LibraryService::search_by_genre(const std::string& query) const {
    GenreSearch strategy;
    return search(strategy, query);
}

// ── Sorting ──────────────────────────────────────────────────────

void LibraryService::sort_books(const ISortStrategy& strategy,
                                SortOrder order,
                                SortAlgorithm algorithm) {
    auto comp = [&strategy, order](const Book& a, const Book& b) -> bool {
        if (order == SortOrder::Ascending) {
            return strategy.before(a, b);
        }
        return strategy.before(b, a);
    };

    if (algorithm == SortAlgorithm::Merge) {
        books_.sort(comp);
    } else {
        books_.insertion_sort(comp);
    }
}

void LibraryService::sort_by_title(SortOrder order, SortAlgorithm algorithm) {
    TitleSort strategy;
    sort_books(strategy, order, algorithm);
}

void LibraryService::sort_by_author(SortOrder order, SortAlgorithm algorithm) {
    AuthorSort strategy;
    sort_books(strategy, order, algorithm);
}

void LibraryService::sort_by_genre(SortOrder order, SortAlgorithm algorithm) {
    GenreSort strategy;
    sort_books(strategy, order, algorithm);
}

} // namespace titans

#pragma once

#include <string>

namespace titans {

/*
 * Invariants:
 * - title_, author_, and genre_ are trimmed and non-empty.
 * - isbn_ is always in normalized, valid form (10 or 13 characters).
 * - No Book object can ever violate these invariants.
 */
class Book {
public:
    // Constructs and validates a Book.
    // Throws std::invalid_argument if title, author, or genre is empty/whitespace,
    // or if isbn is invalid.
    Book(std::string title, std::string author, std::string isbn, std::string genre, bool available = true);

    Book(const Book&) = default;
    Book(Book&&) noexcept = default;
    Book& operator=(const Book&) = default;
    Book& operator=(Book&&) noexcept = default;
    ~Book() = default;

    // Getters (noexcept).
    const std::string& title() const noexcept;
    const std::string& author() const noexcept;
    const std::string& isbn() const noexcept;
    const std::string& genre() const noexcept;
    bool is_available() const noexcept;

    // Setters: validate and trim first; on std::invalid_argument old value is unchanged.
    // Throws std::invalid_argument if title is empty or whitespace-only.
    void set_title(std::string title);

    // Throws std::invalid_argument if author is empty or whitespace-only.
    void set_author(std::string author);

    // Throws std::invalid_argument if genre is empty or whitespace-only.
    void set_genre(std::string genre);

    // Marks book as borrowed; throws std::logic_error if already borrowed.
    void mark_borrowed();

    // Marks book as returned; throws std::logic_error if not borrowed.
    void mark_returned();

    // Normalizes ISBN; throws std::invalid_argument on invalid ISBN.
    static std::string normalize_isbn(const std::string& raw);

    // Checks if raw ISBN can be normalized successfully (does not throw).
    static bool is_valid_isbn(const std::string& raw) noexcept;

    // A book's identity is defined solely by its ISBN (title, author, genre and availability are ignored)
    // because Container::find and Container::remove_value use operator== to look up books by identity.
    friend bool operator==(const Book& lhs, const Book& rhs) noexcept {
        return lhs.isbn_ == rhs.isbn_;
    }

    friend bool operator!=(const Book& lhs, const Book& rhs) noexcept {
        return !(lhs == rhs);
    }

private:
    std::string title_;
    std::string author_;
    std::string isbn_;
    std::string genre_;
    bool available_{true};
};

} // namespace titans

#include "domain/Book.h"
#include "domain/Text.h"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace titans {

namespace {

// Normalizes raw ISBN by stripping whitespace and hyphens, and uppercasing trailing 'x'.
// Returns true and writes to out if valid; returns false otherwise.
bool try_normalize_isbn(const std::string& raw, std::string& out) {
    std::string normalized;
    normalized.reserve(raw.size());
    for (char c : raw) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '-') {
            continue;
        }
        if (c == 'x') {
            normalized.push_back('X');
        } else {
            normalized.push_back(c);
        }
    }

    if (normalized.length() == 13) {
        for (char c : normalized) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        out = std::move(normalized);
        return true;
    }

    if (normalized.length() == 10) {
        for (std::size_t i = 0; i < 9; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(normalized[i]))) {
                return false;
            }
        }
        char last = normalized[9];
        if (std::isdigit(static_cast<unsigned char>(last)) || last == 'X') {
            out = std::move(normalized);
            return true;
        }
        return false;
    }

    return false;
}

} // namespace

Book::Book(std::string title, std::string author, std::string isbn, std::string genre, bool available)
    : title_(text::require_non_blank(title, "title")),
      author_(text::require_non_blank(author, "author")),
      isbn_(normalize_isbn(isbn)),
      genre_(text::require_non_blank(genre, "genre")),
      available_(available) {}

const std::string& Book::title() const noexcept {
    return title_;
}

const std::string& Book::author() const noexcept {
    return author_;
}

const std::string& Book::isbn() const noexcept {
    return isbn_;
}

const std::string& Book::genre() const noexcept {
    return genre_;
}

bool Book::is_available() const noexcept {
    return available_;
}

void Book::set_title(std::string title) {
    std::string validated = text::require_non_blank(title, "title");
    title_ = std::move(validated);
}

void Book::set_author(std::string author) {
    std::string validated = text::require_non_blank(author, "author");
    author_ = std::move(validated);
}

void Book::set_genre(std::string genre) {
    std::string validated = text::require_non_blank(genre, "genre");
    genre_ = std::move(validated);
}

void Book::mark_borrowed() {
    if (!available_) {
        throw std::logic_error("book is already borrowed");
    }
    available_ = false;
}

void Book::mark_returned() {
    if (available_) {
        throw std::logic_error("book is not borrowed");
    }
    available_ = true;
}

std::string Book::normalize_isbn(const std::string& raw) {
    std::string out;
    if (!try_normalize_isbn(raw, out)) {
        throw std::invalid_argument("invalid ISBN: " + raw);
    }
    return out;
}

bool Book::is_valid_isbn(const std::string& raw) noexcept {
    std::string dummy;
    return try_normalize_isbn(raw, dummy);
}

} // namespace titans

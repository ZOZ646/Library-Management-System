#include "service/SearchStrategy.h"
#include "domain/Text.h"

#include <stdexcept>

namespace titans {

bool TitleSearch::matches(const Book& book, const std::string& query) const {
    return text::contains_ignore_case(book.title(), query);
}

const char* TitleSearch::name() const noexcept {
    return "title";
}

bool AuthorSearch::matches(const Book& book, const std::string& query) const {
    return text::contains_ignore_case(book.author(), query);
}

const char* AuthorSearch::name() const noexcept {
    return "author";
}

bool GenreSearch::matches(const Book& book, const std::string& query) const {
    return text::equals_ignore_case(book.genre(), query);
}

const char* GenreSearch::name() const noexcept {
    return "genre";
}

bool IsbnSearch::matches(const Book& book, const std::string& query) const {
    if (!Book::is_valid_isbn(query)) {
        return false;
    }
    return Book::normalize_isbn(query) == book.isbn();
}

const char* IsbnSearch::name() const noexcept {
    return "isbn";
}

std::unique_ptr<ISearchStrategy> SearchStrategyFactory::create(const std::string& key) {
    std::string normalized = text::to_lower(text::trim(key));
    if (normalized == "title") {
        return std::make_unique<TitleSearch>();
    }
    if (normalized == "author") {
        return std::make_unique<AuthorSearch>();
    }
    if (normalized == "isbn") {
        return std::make_unique<IsbnSearch>();
    }
    if (normalized == "genre") {
        return std::make_unique<GenreSearch>();
    }
    throw std::invalid_argument("unknown search criterion: " + key);
}

} // namespace titans

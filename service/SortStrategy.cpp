#include "service/SortStrategy.h"
#include "domain/Text.h"

#include <stdexcept>

namespace titans {

bool TitleSort::before(const Book& a, const Book& b) const noexcept {
    return text::compare_ignore_case(a.title(), b.title()) < 0;
}

const char* TitleSort::name() const noexcept {
    return "title";
}

bool AuthorSort::before(const Book& a, const Book& b) const noexcept {
    return text::compare_ignore_case(a.author(), b.author()) < 0;
}

const char* AuthorSort::name() const noexcept {
    return "author";
}

bool GenreSort::before(const Book& a, const Book& b) const noexcept {
    return text::compare_ignore_case(a.genre(), b.genre()) < 0;
}

const char* GenreSort::name() const noexcept {
    return "genre";
}

std::unique_ptr<ISortStrategy> SortStrategyFactory::create(const std::string& key) {
    std::string normalized = text::to_lower(text::trim(key));
    if (normalized == "title") {
        return std::make_unique<TitleSort>();
    }
    if (normalized == "author") {
        return std::make_unique<AuthorSort>();
    }
    if (normalized == "genre") {
        return std::make_unique<GenreSort>();
    }
    throw std::invalid_argument("unknown sort criterion: " + key);
}

} // namespace titans

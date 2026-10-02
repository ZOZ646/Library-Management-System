#pragma once

#include "domain/Book.h"

#include <memory>
#include <string>

namespace titans {

// Strategy interface for comparing two books (strict weak ordering).
// before(a, b) returns true iff a must come strictly before b.
class ISortStrategy {
public:
    virtual ~ISortStrategy() = default;
    virtual bool before(const Book& a, const Book& b) const noexcept = 0;
    virtual const char* name() const noexcept = 0;

protected:
    ISortStrategy() = default;
    ISortStrategy(const ISortStrategy&) = default;
    ISortStrategy(ISortStrategy&&) = default;
    ISortStrategy& operator=(const ISortStrategy&) = default;
    ISortStrategy& operator=(ISortStrategy&&) = default;
};

// Compares books by title, case-insensitive.
class TitleSort final : public ISortStrategy {
public:
    bool before(const Book& a, const Book& b) const noexcept override;
    const char* name() const noexcept override;
};

// Compares books by author, case-insensitive.
class AuthorSort final : public ISortStrategy {
public:
    bool before(const Book& a, const Book& b) const noexcept override;
    const char* name() const noexcept override;
};

// Compares books by genre, case-insensitive.
class GenreSort final : public ISortStrategy {
public:
    bool before(const Book& a, const Book& b) const noexcept override;
    const char* name() const noexcept override;
};

// Factory for sort strategies. Accepted keys: "title", "author", "genre" (trimmed, case-insensitive).
// Throws std::invalid_argument("unknown sort criterion: " + key) for anything else.
class SortStrategyFactory final {
public:
    SortStrategyFactory() = delete;
    static std::unique_ptr<ISortStrategy> create(const std::string& key);
};

} // namespace titans

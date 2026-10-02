#pragma once

#include "domain/Book.h"

#include <memory>
#include <string>

namespace titans {

// Strategy interface for searching books.
// Precondition: the service passes a query that is already trimmed and non-blank.
// Strategies never trim and never throw on any input.
class ISearchStrategy {
public:
    virtual ~ISearchStrategy() = default;
    virtual bool matches(const Book& book, const std::string& query) const = 0;
    virtual const char* name() const noexcept = 0;

protected:
    ISearchStrategy() = default;
    ISearchStrategy(const ISearchStrategy&) = default;
    ISearchStrategy(ISearchStrategy&&) = default;
    ISearchStrategy& operator=(const ISearchStrategy&) = default;
    ISearchStrategy& operator=(ISearchStrategy&&) = default;
};

// Matches books whose title contains the query (case-insensitive substring).
class TitleSearch final : public ISearchStrategy {
public:
    bool matches(const Book& book, const std::string& query) const override;
    const char* name() const noexcept override;
};

// Matches books whose author contains the query (case-insensitive substring).
class AuthorSearch final : public ISearchStrategy {
public:
    bool matches(const Book& book, const std::string& query) const override;
    const char* name() const noexcept override;
};

// Matches books whose genre equals the query (case-insensitive whole-word match).
// "fiction" does NOT match "Non-Fiction".
class GenreSearch final : public ISearchStrategy {
public:
    bool matches(const Book& book, const std::string& query) const override;
    const char* name() const noexcept override;
};

// Matches books whose normalized ISBN equals the normalized query.
// A partial or invalid ISBN simply matches nothing and never throws.
class IsbnSearch final : public ISearchStrategy {
public:
    bool matches(const Book& book, const std::string& query) const override;
    const char* name() const noexcept override;
};

// Factory for search strategies. Accepted keys: "title", "author", "isbn", "genre"
// (trimmed, case-insensitive).
// Throws std::invalid_argument("unknown search criterion: " + key) for anything else.
class SearchStrategyFactory final {
public:
    SearchStrategyFactory() = delete;
    static std::unique_ptr<ISearchStrategy> create(const std::string& key);
};

} // namespace titans

#pragma once

#include "repository/ILibraryRepository.h"

#include <memory>
#include <string>

struct sqlite3;

namespace titans {

struct SqliteCloser {
    void operator()(sqlite3* db) const noexcept;
};

class SqliteRepository final : public ILibraryRepository {
public:
    // Opens or creates a SQLite database at path (":memory:" for private in-memory DB).
    // Throws std::invalid_argument if path is empty.
    // Throws DatabaseError if opening, configuring, or schema verification fails.
    explicit SqliteRepository(const std::string& path);

    ~SqliteRepository() override;

    SqliteRepository(const SqliteRepository&) = delete;
    SqliteRepository& operator=(const SqliteRepository&) = delete;
    SqliteRepository(SqliteRepository&&) = delete;
    SqliteRepository& operator=(SqliteRepository&&) = delete;

    StoredLibrary load() override;
    void add_book(const Book& book) override;
    void remove_book(const std::string& isbn) override;
    void add_member(const Member& member) override;
    void remove_member(int id) override;
    void record_borrow(const std::string& isbn, int member_id) override;
    void record_return(const std::string& isbn) override;

private:
    std::unique_ptr<sqlite3, SqliteCloser> db_;
};

} // namespace titans

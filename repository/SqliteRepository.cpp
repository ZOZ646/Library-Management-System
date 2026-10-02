#include "repository/SqliteRepository.h"
#include "repository/RepositoryErrors.h"
#include "sqlite3.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace titans {

void SqliteCloser::operator()(sqlite3* db) const noexcept {
    if (db) {
        sqlite3_close_v2(db);
    }
}

namespace {

void exec(sqlite3* db, const char* sql, const char* op_name = "exec") {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : sqlite3_errmsg(db);
        sqlite3_free(err_msg);
        throw DatabaseError(std::string(op_name) + ": " + err);
    }
}

class Statement {
public:
    Statement(sqlite3* db, const char* sql, const char* op_name = "prepare")
        : db_(db), op_name_(op_name) {
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr);
        if (rc != SQLITE_OK) {
            throw DatabaseError(std::string(op_name_) + ": " + sqlite3_errmsg(db_));
        }
    }

    ~Statement() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&&) = delete;
    Statement& operator=(Statement&&) = delete;

    void bind_text(int index, const std::string& text) {
        int rc = sqlite3_bind_text(stmt_, index, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            throw DatabaseError(std::string(op_name_) + ": " + sqlite3_errmsg(db_));
        }
    }

    void bind_int(int index, int val) {
        int rc = sqlite3_bind_int(stmt_, index, val);
        if (rc != SQLITE_OK) {
            throw DatabaseError(std::string(op_name_) + ": " + sqlite3_errmsg(db_));
        }
    }

    bool step() {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) {
            return true;
        }
        if (rc == SQLITE_DONE) {
            return false;
        }
        throw DatabaseError(std::string(op_name_) + ": " + sqlite3_errmsg(db_));
    }

    std::string column_text(int col) {
        const unsigned char* txt = sqlite3_column_text(stmt_, col);
        if (!txt) {
            throw DatabaseError(std::string(op_name_) + ": unexpected NULL column");
        }
        int bytes = sqlite3_column_bytes(stmt_, col);
        return std::string(reinterpret_cast<const char*>(txt), static_cast<std::size_t>(bytes));
    }

    int column_int(int col) {
        return sqlite3_column_int(stmt_, col);
    }

private:
    sqlite3* db_;
    sqlite3_stmt* stmt_{nullptr};
    const char* op_name_;
};

} // namespace

SqliteRepository::SqliteRepository(const std::string& path) {
    if (path.empty()) {
        throw std::invalid_argument("database path must not be empty");
    }

    sqlite3* raw_db = nullptr;
    int rc = sqlite3_open_v2(path.c_str(), &raw_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    db_.reset(raw_db);
    if (rc != SQLITE_OK) {
        std::string msg = raw_db ? sqlite3_errmsg(raw_db) : "unknown error";
        throw DatabaseError("cannot open database '" + path + "': " + msg);
    }

    sqlite3_busy_timeout(db_.get(), 2000);

    exec(db_.get(), "PRAGMA foreign_keys = ON;", "foreign_keys");
    {
        Statement fk_stmt(db_.get(), "PRAGMA foreign_keys;", "check foreign_keys");
        if (!fk_stmt.step() || fk_stmt.column_int(0) != 1) {
            throw DatabaseError("foreign keys could not be enabled");
        }
    }

    int version = 0;
    {
        Statement ver_stmt(db_.get(), "PRAGMA user_version;", "check user_version");
        if (ver_stmt.step()) {
            version = ver_stmt.column_int(0);
        }
    }

    if (version == 0) {
        exec(db_.get(), "BEGIN IMMEDIATE;", "begin schema");
        try {
            // Closed loans are kept as history; deleting a book or member also deletes its (closed) history;
            // the partial unique index enforces "at most one open loan per book" in the database too.
            const char* schema_sql =
                "CREATE TABLE IF NOT EXISTS books (\n"
                "  seq    INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                "  isbn   TEXT NOT NULL UNIQUE,\n"
                "  title  TEXT NOT NULL CHECK (length(trim(title)) > 0),\n"
                "  author TEXT NOT NULL CHECK (length(trim(author)) > 0),\n"
                "  genre  TEXT NOT NULL CHECK (length(trim(genre)) > 0)\n"
                ");\n"
                "CREATE TABLE IF NOT EXISTS members (\n"
                "  seq  INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                "  id   INTEGER NOT NULL UNIQUE CHECK (id > 0),\n"
                "  name TEXT NOT NULL CHECK (length(trim(name)) > 0)\n"
                ");\n"
                "CREATE TABLE IF NOT EXISTS loans (\n"
                "  seq         INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                "  book_isbn   TEXT    NOT NULL REFERENCES books(isbn)  ON DELETE CASCADE,\n"
                "  member_id   INTEGER NOT NULL REFERENCES members(id)  ON DELETE CASCADE,\n"
                "  borrowed_at TEXT    NOT NULL DEFAULT (datetime('now')),\n"
                "  returned_at TEXT\n"
                ");\n"
                "CREATE UNIQUE INDEX IF NOT EXISTS one_open_loan_per_book ON loans(book_isbn) WHERE returned_at IS NULL;\n"
                "PRAGMA user_version = 1;\n";

            exec(db_.get(), schema_sql, "create schema");
            exec(db_.get(), "COMMIT;", "commit schema");
        } catch (...) {
            try { exec(db_.get(), "ROLLBACK;", "rollback schema"); } catch (...) {}
            throw;
        }
    } else if (version != 1) {
        throw DatabaseError("unsupported database version: " + std::to_string(version));
    }
}

SqliteRepository::~SqliteRepository() = default;

void SqliteRepository::add_book(const Book& book) {
    Statement stmt(db_.get(), "INSERT INTO books(isbn, title, author, genre) VALUES (?1, ?2, ?3, ?4);", "add_book");
    stmt.bind_text(1, book.isbn());
    stmt.bind_text(2, book.title());
    stmt.bind_text(3, book.author());
    stmt.bind_text(4, book.genre());
    stmt.step();
}

void SqliteRepository::remove_book(const std::string& isbn) {
    Statement stmt(db_.get(), "DELETE FROM books WHERE isbn = ?1;", "remove_book");
    stmt.bind_text(1, isbn);
    stmt.step();
    if (sqlite3_changes(db_.get()) == 0) {
        throw DatabaseError("book not found in database: " + isbn);
    }
}

void SqliteRepository::add_member(const Member& member) {
    Statement stmt(db_.get(), "INSERT INTO members(id, name) VALUES (?1, ?2);", "add_member");
    stmt.bind_int(1, member.id());
    stmt.bind_text(2, member.name());
    stmt.step();
}

void SqliteRepository::remove_member(int id) {
    Statement stmt(db_.get(), "DELETE FROM members WHERE id = ?1;", "remove_member");
    stmt.bind_int(1, id);
    stmt.step();
    if (sqlite3_changes(db_.get()) == 0) {
        throw DatabaseError("member not found in database: " + std::to_string(id));
    }
}

void SqliteRepository::record_borrow(const std::string& isbn, int member_id) {
    Statement stmt(db_.get(), "INSERT INTO loans(book_isbn, member_id) VALUES (?1, ?2);", "record_borrow");
    stmt.bind_text(1, isbn);
    stmt.bind_int(2, member_id);
    stmt.step();
}

void SqliteRepository::record_return(const std::string& isbn) {
    Statement stmt(db_.get(), "UPDATE loans SET returned_at = datetime('now') WHERE book_isbn = ?1 AND returned_at IS NULL;", "record_return");
    stmt.bind_text(1, isbn);
    stmt.step();
    if (sqlite3_changes(db_.get()) == 0) {
        throw DatabaseError("no open loan for book: " + isbn);
    }
}

StoredLibrary SqliteRepository::load() {
    exec(db_.get(), "BEGIN;", "begin load");
    try {
        StoredLibrary lib;

        // 1. Books
        {
            Statement b_stmt(db_.get(), "SELECT isbn, title, author, genre FROM books ORDER BY seq;", "load books");
            while (b_stmt.step()) {
                std::string isbn = b_stmt.column_text(0);
                std::string title = b_stmt.column_text(1);
                std::string author = b_stmt.column_text(2);
                std::string genre = b_stmt.column_text(3);
                try {
                    lib.books.emplace_back(std::move(title), std::move(author), std::move(isbn), std::move(genre));
                } catch (const std::invalid_argument& e) {
                    throw DatabaseError(std::string("corrupt data in database: ") + e.what());
                }
            }
        }

        // 2. Members
        {
            Statement m_stmt(db_.get(), "SELECT id, name FROM members ORDER BY seq;", "load members");
            while (m_stmt.step()) {
                int id = m_stmt.column_int(0);
                std::string name = m_stmt.column_text(1);
                try {
                    lib.members.emplace_back(id, std::move(name));
                } catch (const std::invalid_argument& e) {
                    throw DatabaseError(std::string("corrupt data in database: ") + e.what());
                }
            }
        }

        // 3. Open loans
        {
            Statement l_stmt(db_.get(), "SELECT book_isbn, member_id FROM loans WHERE returned_at IS NULL ORDER BY seq;", "load loans");
            while (l_stmt.step()) {
                std::string isbn = l_stmt.column_text(0);
                int member_id = l_stmt.column_int(1);
                lib.open_loans.push_back({std::move(isbn), member_id});
            }
        }

        exec(db_.get(), "COMMIT;", "commit load");
        return lib;
    } catch (...) {
        try { exec(db_.get(), "ROLLBACK;", "rollback load"); } catch (...) {}
        throw;
    }
}

} // namespace titans

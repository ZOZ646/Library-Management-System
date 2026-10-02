#pragma once

#include <doctest/doctest.h>
#include "tests/library_fixtures.h"
#include "repository/ILibraryRepository.h"
#include "repository/RepositoryErrors.h"
#include "sqlite3.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace titans::fixtures {

class TempDbPath {
public:
    TempDbPath() {
        static std::atomic<unsigned long long> counter{0};
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto count = counter.fetch_add(1);
        auto p = std::filesystem::temp_directory_path() /
                 ("titans_test_" + std::to_string(now) + "_" + std::to_string(count) + ".db");
        path_ = p.string();
    }

    ~TempDbPath() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
        std::filesystem::remove(path_ + "-journal", ec);
        std::filesystem::remove(path_ + "-wal", ec);
        std::filesystem::remove(path_ + "-shm", ec);
    }

    TempDbPath(const TempDbPath&) = delete;
    TempDbPath& operator=(const TempDbPath&) = delete;
    TempDbPath(TempDbPath&&) = delete;
    TempDbPath& operator=(TempDbPath&&) = delete;

    const std::string& path() const noexcept {
        return path_;
    }

private:
    std::string path_;
};

inline void raw_exec(const std::string& path, const std::string& sql) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = db ? sqlite3_errmsg(db) : "cannot open raw connection";
        if (db) {
            sqlite3_close_v2(db);
        }
        throw std::runtime_error("raw_exec open error: " + err);
    }
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : sqlite3_errmsg(db);
        sqlite3_free(err_msg);
        sqlite3_close_v2(db);
        throw std::runtime_error("raw_exec error: " + err);
    }
    sqlite3_close_v2(db);
}

inline long long raw_scalar(const std::string& path, const std::string& sql) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = db ? sqlite3_errmsg(db) : "cannot open raw connection";
        if (db) {
            sqlite3_close_v2(db);
        }
        throw std::runtime_error("raw_scalar open error: " + err);
    }
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_close_v2(db);
        throw std::runtime_error("raw_scalar prepare error: " + err);
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close_v2(db);
        throw std::runtime_error("raw_scalar step error: no rows returned");
    }
    long long val = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close_v2(db);
    return val;
}

class RecordingRepository : public ILibraryRepository {
public:
    std::vector<std::string> calls;
    StoredLibrary stored;
    std::function<void(const std::string&)> hook;

    StoredLibrary load() override {
        calls.push_back("load");
        return stored;
    }

    void add_book(const Book& book) override {
        std::string text = "add_book:" + book.isbn();
        if (hook) {
            hook(text);
        }
        calls.push_back(std::move(text));
    }

    void remove_book(const std::string& isbn) override {
        std::string text = "remove_book:" + isbn;
        if (hook) {
            hook(text);
        }
        calls.push_back(std::move(text));
    }

    void add_member(const Member& member) override {
        std::string text = "add_member:" + std::to_string(member.id());
        if (hook) {
            hook(text);
        }
        calls.push_back(std::move(text));
    }

    void remove_member(int id) override {
        std::string text = "remove_member:" + std::to_string(id);
        if (hook) {
            hook(text);
        }
        calls.push_back(std::move(text));
    }

    void record_borrow(const std::string& isbn, int member_id) override {
        std::string text = "record_borrow:" + isbn + ":" + std::to_string(member_id);
        if (hook) {
            hook(text);
        }
        calls.push_back(std::move(text));
    }

    void record_return(const std::string& isbn) override {
        std::string text = "record_return:" + isbn;
        if (hook) {
            hook(text);
        }
        calls.push_back(std::move(text));
    }

    std::size_t write_count() const noexcept {
        std::size_t count = 0;
        for (const auto& c : calls) {
            if (c != "load") {
                ++count;
            }
        }
        return count;
    }
};

class FailingRepository : public ILibraryRepository {
public:
    explicit FailingRepository(ILibraryRepository& target, std::size_t fail_on = 0)
        : target_(target), fail_on_write(fail_on) {}

    std::size_t fail_on_write{0};
    std::size_t write_count{0};

    StoredLibrary load() override {
        return target_.load();
    }

    void add_book(const Book& book) override {
        check_failure();
        target_.add_book(book);
    }

    void remove_book(const std::string& isbn) override {
        check_failure();
        target_.remove_book(isbn);
    }

    void add_member(const Member& member) override {
        check_failure();
        target_.add_member(member);
    }

    void remove_member(int id) override {
        check_failure();
        target_.remove_member(id);
    }

    void record_borrow(const std::string& isbn, int member_id) override {
        check_failure();
        target_.record_borrow(isbn, member_id);
    }

    void record_return(const std::string& isbn) override {
        check_failure();
        target_.record_return(isbn);
    }

private:
    void check_failure() {
        ++write_count;
        if (fail_on_write > 0 && write_count == fail_on_write) {
            throw DatabaseError("injected failure");
        }
    }

    ILibraryRepository& target_;
};

inline bool same_state_ignoring_order(const LibraryService& a, const LibraryService& b) {
    Snapshot snapA = snapshot(a);
    Snapshot snapB = snapshot(b);

    std::sort(snapA.books.begin(), snapA.books.end(), [](const BookSnap& x, const BookSnap& y) {
        return x.isbn < y.isbn;
    });
    std::sort(snapB.books.begin(), snapB.books.end(), [](const BookSnap& x, const BookSnap& y) {
        return x.isbn < y.isbn;
    });

    std::sort(snapA.members.begin(), snapA.members.end(), [](const MemberSnap& x, const MemberSnap& y) {
        return x.id < y.id;
    });
    std::sort(snapB.members.begin(), snapB.members.end(), [](const MemberSnap& x, const MemberSnap& y) {
        return x.id < y.id;
    });

    return snapA == snapB;
}

inline std::string describe_difference(const LibraryService& a, const LibraryService& b) {
    Snapshot snapA = snapshot(a);
    Snapshot snapB = snapshot(b);

    std::sort(snapA.books.begin(), snapA.books.end(), [](const BookSnap& x, const BookSnap& y) {
        return x.isbn < y.isbn;
    });
    std::sort(snapB.books.begin(), snapB.books.end(), [](const BookSnap& x, const BookSnap& y) {
        return x.isbn < y.isbn;
    });

    std::sort(snapA.members.begin(), snapA.members.end(), [](const MemberSnap& x, const MemberSnap& y) {
        return x.id < y.id;
    });
    std::sort(snapB.members.begin(), snapB.members.end(), [](const MemberSnap& x, const MemberSnap& y) {
        return x.id < y.id;
    });

    std::ostringstream oss;
    if (snapA.books.size() != snapB.books.size()) {
        oss << "Book count mismatch: " << snapA.books.size() << " vs " << snapB.books.size() << "; ";
    }
    if (snapA.members.size() != snapB.members.size()) {
        oss << "Member count mismatch: " << snapA.members.size() << " vs " << snapB.members.size() << "; ";
    }
    std::size_t min_b = std::min(snapA.books.size(), snapB.books.size());
    for (std::size_t i = 0; i < min_b; ++i) {
        if (!(snapA.books[i] == snapB.books[i])) {
            oss << "Book diff at " << i << ": [" << snapA.books[i].isbn << ", avail=" << snapA.books[i].available
                << "] vs [" << snapB.books[i].isbn << ", avail=" << snapB.books[i].available << "]; ";
            break;
        }
    }
    std::size_t min_m = std::min(snapA.members.size(), snapB.members.size());
    for (std::size_t i = 0; i < min_m; ++i) {
        if (!(snapA.members[i] == snapB.members[i])) {
            oss << "Member diff at " << i << ": [id=" << snapA.members[i].id << "] vs [id=" << snapB.members[i].id << "]; ";
            break;
        }
    }
    return oss.str();
}

} // namespace titans::fixtures

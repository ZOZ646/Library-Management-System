#pragma once

#include <doctest/doctest.h>
#include "domain/Book.h"
#include "domain/Member.h"
#include "service/LibraryService.h"
#include "core/Container.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace titans::fixtures {

inline Book make_book(const std::string& title, const std::string& author,
                      const std::string& isbn, const std::string& genre) {
    return Book(title, author, isbn, genre, true);
}

// Extracts ISBNs from a Container<Book> in container order.
inline std::vector<std::string> isbns(const Container<Book>& c) {
    std::vector<std::string> result;
    for (const auto& b : c) {
        result.push_back(b.isbn());
    }
    return result;
}

// Extracts titles from a Container<Book> in container order.
inline std::vector<std::string> titles(const Container<Book>& c) {
    std::vector<std::string> result;
    for (const auto& b : c) {
        result.push_back(b.title());
    }
    return result;
}

// Extracts member ids from a Container<Member> in container order.
inline std::vector<int> member_ids(const Container<Member>& c) {
    std::vector<int> result;
    for (const auto& m : c) {
        result.push_back(m.id());
    }
    return result;
}

// Extracts borrowed ISBNs from a member in container order.
inline std::vector<std::string> borrowed_list(const Member& m) {
    std::vector<std::string> result;
    for (const auto& isbn : m.borrowed_isbns()) {
        result.push_back(isbn);
    }
    return result;
}

// ── Snapshot for strong-guarantee verification ───────────────────

struct BookSnap {
    std::string isbn;
    std::string title;
    std::string author;
    std::string genre;
    bool available;

    bool operator==(const BookSnap& o) const {
        return isbn == o.isbn && title == o.title && author == o.author
               && genre == o.genre && available == o.available;
    }
};

struct MemberSnap {
    int id;
    std::string name;
    std::vector<std::string> borrowed;

    bool operator==(const MemberSnap& o) const {
        return id == o.id && name == o.name && borrowed == o.borrowed;
    }
};

struct Snapshot {
    std::vector<BookSnap> books;
    std::vector<MemberSnap> members;

    bool operator==(const Snapshot& o) const {
        return books == o.books && members == o.members;
    }
};

inline Snapshot snapshot(const LibraryService& svc) {
    Snapshot s;
    for (const auto& b : svc.books()) {
        s.books.push_back({b.isbn(), b.title(), b.author(), b.genre(), b.is_available()});
    }
    for (const auto& m : svc.members()) {
        s.members.push_back({m.id(), m.name(), borrowed_list(m)});
    }
    return s;
}

// ── Invariant checker ────────────────────────────────────────────

inline void check_consistency(const LibraryService& svc) {
    // I1: ISBNs unique
    {
        std::set<std::string> isbn_set;
        for (const auto& b : svc.books()) {
            CHECK(isbn_set.insert(b.isbn()).second);
        }
    }

    // I1: Member ids unique
    {
        std::set<int> id_set;
        for (const auto& m : svc.members()) {
            CHECK(id_set.insert(m.id()).second);
        }
    }

    // I3: Every ISBN in a member's borrowed list belongs to a catalog book
    // and I2 (partial): that book is unavailable
    for (const auto& m : svc.members()) {
        for (const auto& isbn : m.borrowed_isbns()) {
            CHECK(svc.has_book(isbn));
            if (svc.has_book(isbn)) {
                CHECK(!svc.get_book(isbn).is_available());
            }
        }
    }

    // I2 (full): each unavailable book is held by exactly ONE member
    for (const auto& b : svc.books()) {
        if (!b.is_available()) {
            int holder_count = 0;
            for (const auto& m : svc.members()) {
                if (m.has_borrowed(b.isbn())) {
                    ++holder_count;
                }
            }
            CHECK(holder_count == 1);
        }
    }

    // I4: No member above kMaxBorrowed
    for (const auto& m : svc.members()) {
        CHECK(m.borrowed_count() <= Member::kMaxBorrowed);
    }

    // Statistics consistency
    CHECK(svc.total_books() == svc.available_books() + svc.borrowed_books());

    // borrowed_books() equals sum of all members' borrowed_count()
    std::size_t total_borrowed = 0;
    for (const auto& m : svc.members()) {
        total_borrowed += m.borrowed_count();
    }
    CHECK(svc.borrowed_books() == total_borrowed);
}

// ── Sample service builder ───────────────────────────────────────
// At least 8 books, 3+ genres (one genre is a substring of another),
// mixed-case titles, one title a substring of another, one author with two books,
// one Arabic title/author.

inline LibraryService make_sample_service() {
    LibraryService svc;

    // Genre names: "Fiction", "Non-Fiction", "Science" — "Fiction" is a substring of "Non-Fiction"
    // Mixed-case titles; "apple pie" is a substring of "Big apple pie"
    // Author "Jane Doe" has two books

    svc.add_book(make_book("apple pie", "Jane Doe", "9780000000001", "Fiction"));
    svc.add_book(make_book("Banana Split", "John Smith", "9780000000002", "Fiction"));
    svc.add_book(make_book("Cherry Cake", "Alice Brown", "9780000000003", "Non-Fiction"));
    svc.add_book(make_book("Big apple pie", "Jane Doe", "9780000000004", "Science"));
    svc.add_book(make_book("Elderberry Jam", "Bob White", "9780000000005", "Non-Fiction"));
    svc.add_book(make_book("Fig Tart", "Charlie Green", "0123456789", "Science"));
    svc.add_book(make_book("Grape Sorbet", "Diana Black", "012345678X", "Fiction"));
    // Arabic title and author (hex-escaped UTF-8, split so hex escapes don't merge)
    // "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8" = Arabic word for "book"
    // "\xD9\x85\xD8\xA4\xD9\x84\xD9\x81" = Arabic word for "author"
    svc.add_book(make_book("\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8" " 1",
                           "\xD9\x85\xD8\xA4\xD9\x84\xD9\x81",
                           "9780000000008", "Fiction"));

    svc.add_member(1, "Alice");
    svc.add_member(2, "Bob");
    svc.add_member(3, "Charlie");
    svc.add_member(4, "Diana");

    return svc;
}

} // namespace titans::fixtures

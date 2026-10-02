#pragma once

#include "domain/Book.h"
#include "domain/Member.h"

#include <string>
#include <vector>

namespace titans {

/*
 * ILibraryRepository interface.
 *
 * Contract:
 * Every function throws DatabaseError on failure and a failed call leaves
 * the stored data unchanged.
 * The repository stores book identity and descriptive fields, member id and name,
 * and the loan history.
 * AVAILABILITY IS NOT STORED: a book is borrowed exactly when it has an open loan
 * (single source of truth).
 * add_book is only ever called for available books.
 */

struct StoredLoan {
    std::string isbn;
    int member_id;
};

struct StoredLibrary {
    std::vector<Book> books;             // catalog order (insertion order); every book available
    std::vector<Member> members;         // insertion order; borrowed lists empty
    std::vector<StoredLoan> open_loans;  // oldest first
};

class ILibraryRepository {
public:
    virtual ~ILibraryRepository() = default;

    // Loads the complete library state from the database.
    // Throws DatabaseError on failure.
    virtual StoredLibrary load() = 0;

    // Adds a new book to persistent storage.
    // Throws DatabaseError on failure (e.g. duplicate ISBN or constraint violation).
    virtual void add_book(const Book& book) = 0;

    // Removes a book from storage.
    // Throws DatabaseError on failure (e.g. book not found).
    virtual void remove_book(const std::string& isbn) = 0;

    // Adds a new member to storage.
    // Throws DatabaseError on failure (e.g. duplicate id).
    virtual void add_member(const Member& member) = 0;

    // Removes a member from storage.
    // Throws DatabaseError on failure (e.g. member not found).
    virtual void remove_member(int id) = 0;

    // Records an open borrow loan.
    // Throws DatabaseError on failure (e.g. foreign key or already open loan).
    virtual void record_borrow(const std::string& isbn, int member_id) = 0;

    // Closes an open loan by setting its returned_at timestamp.
    // Throws DatabaseError on failure (e.g. no open loan found).
    virtual void record_return(const std::string& isbn) = 0;

protected:
    ILibraryRepository() = default;
    ILibraryRepository(const ILibraryRepository&) = default;
    ILibraryRepository& operator=(const ILibraryRepository&) = default;
    ILibraryRepository(ILibraryRepository&&) = default;
    ILibraryRepository& operator=(ILibraryRepository&&) = default;
};

} // namespace titans

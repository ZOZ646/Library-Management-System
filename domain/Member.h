#pragma once

#include "core/Container.h"

#include <cstddef>
#include <string>

namespace titans {

/*
 * Invariants:
 * - id_ > 0.
 * - name_ is trimmed and non-empty.
 * - borrowed_ never contains duplicates and never holds more than kMaxBorrowed entries.
 */
class Member {
public:
    static constexpr std::size_t kMaxBorrowed = 5;

    // Constructs a Member.
    // Throws std::invalid_argument if id <= 0 or if name is empty/whitespace-only.
    Member(int id, std::string name);

    Member(const Member&) = default;
    Member(Member&&) noexcept = default;
    Member& operator=(const Member&) = default;
    Member& operator=(Member&&) noexcept = default;
    ~Member() = default;

    // Getters (noexcept).
    int id() const noexcept;
    const std::string& name() const noexcept;

    // Sets name; throws std::invalid_argument if name is empty/whitespace-only (old name preserved).
    void set_name(std::string name);

    // Number of currently borrowed books (noexcept).
    std::size_t borrowed_count() const noexcept;

    // True if borrowed_count < kMaxBorrowed (noexcept).
    bool can_borrow() const noexcept;

    // O(k) - checks if the member currently has the given ISBN borrowed. Does not throw.
    bool has_borrowed(const std::string& isbn) const;

    // O(k) - records an ISBN as borrowed by this member.
    // Throws std::invalid_argument if isbn is empty/whitespace-only.
    // Throws std::logic_error if book is already borrowed by this member or if borrow limit is reached.
    void add_borrowed(const std::string& isbn);

    // O(k) - removes an ISBN from the borrowed list.
    // Throws std::logic_error if the book was not borrowed by this member.
    void remove_borrowed(const std::string& isbn);

    // Returns a const reference to the container of borrowed ISBNs (noexcept).
    const titans::Container<std::string>& borrowed_isbns() const noexcept;

    // Equality by id: the member id is the member's identity.
    friend bool operator==(const Member& lhs, const Member& rhs) noexcept {
        return lhs.id_ == rhs.id_;
    }

    friend bool operator!=(const Member& lhs, const Member& rhs) noexcept {
        return !(lhs == rhs);
    }

private:
    int id_;
    std::string name_;
    titans::Container<std::string> borrowed_;
};

} // namespace titans

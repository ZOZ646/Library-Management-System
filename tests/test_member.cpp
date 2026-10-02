#include <doctest/doctest.h>
#include "domain/Member.h"
#include "core/Container.h"

#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

namespace {

using titans::Member;
using titans::Container;

std::vector<std::string> member_isbns_to_vector(const Member& m) {
    std::vector<std::string> result;
    for (const auto& isbn : m.borrowed_isbns()) {
        result.push_back(isbn);
    }
    return result;
}

std::vector<std::string> member_names_to_vector(const Container<Member>& c) {
    std::vector<std::string> result;
    for (const auto& m : c) {
        result.push_back(m.name());
    }
    return result;
}

} // namespace

TEST_CASE("1. Member construction and initial state") {
    Member m(42, "  Alice Smith  ");
    CHECK(m.id() == 42);
    CHECK(m.name() == "Alice Smith");
    CHECK(m.borrowed_count() == 0);
    CHECK(m.can_borrow() == true);
    CHECK(m.borrowed_isbns().empty());
}

TEST_CASE("2. Member construction invalid arguments") {
    CHECK_THROWS_WITH_AS(Member(0, "Alice"), "member id must be positive", std::invalid_argument);
    CHECK_THROWS_WITH_AS(Member(-5, "Alice"), "member id must be positive", std::invalid_argument);

    CHECK_THROWS_WITH_AS(Member(1, ""), "name must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(Member(1, "   \t\n "), "name must not be empty", std::invalid_argument);
}

TEST_CASE("3. Member set_name validation") {
    Member m(1, "Bob");
    m.set_name("  Robert  ");
    CHECK(m.name() == "Robert");

    CHECK_THROWS_WITH_AS(m.set_name(""), "name must not be empty", std::invalid_argument);
    CHECK(m.name() == "Robert");

    CHECK_THROWS_WITH_AS(m.set_name("   \n\t  "), "name must not be empty", std::invalid_argument);
    CHECK(m.name() == "Robert");
}

TEST_CASE("4. Member add_borrowed insertion order and query") {
    Member m(1, "Charlie");
    CHECK(!m.has_borrowed("9780134685991"));

    m.add_borrowed("9780134685991");
    CHECK(m.borrowed_count() == 1);
    CHECK(m.has_borrowed("9780134685991"));
    CHECK(!m.has_borrowed("0306406152"));

    m.add_borrowed("0306406152");
    CHECK(m.borrowed_count() == 2);
    CHECK(m.has_borrowed("0306406152"));

    std::vector<std::string> expected = {"9780134685991", "0306406152"};
    CHECK(member_isbns_to_vector(m) == expected);
}

TEST_CASE("5. Member add_borrowed error conditions and padding") {
    Member m(1, "Diana");
    m.add_borrowed("  1234567890  ");
    CHECK(m.borrowed_count() == 1);
    CHECK(m.has_borrowed("1234567890"));
    CHECK(member_isbns_to_vector(m) == std::vector<std::string>{"1234567890"});

    // Blank isbn throws invalid_argument
    CHECK_THROWS_WITH_AS(m.add_borrowed(""), "isbn must not be empty", std::invalid_argument);
    CHECK_THROWS_WITH_AS(m.add_borrowed("   \t "), "isbn must not be empty", std::invalid_argument);
    CHECK(m.borrowed_count() == 1);

    // Duplicate throws logic_error
    CHECK_THROWS_WITH_AS(m.add_borrowed("1234567890"), "book already borrowed by this member", std::logic_error);
    CHECK_THROWS_WITH_AS(m.add_borrowed("  1234567890  "), "book already borrowed by this member", std::logic_error);
    CHECK(m.borrowed_count() == 1);
}

TEST_CASE("6. Member borrowing limits and duplicate-vs-limit precedence") {
    Member m(1, "Edward");
    for (std::size_t i = 1; i <= Member::kMaxBorrowed; ++i) {
        CHECK(m.can_borrow() == true);
        m.add_borrowed("ISBN" + std::to_string(i));
    }

    CHECK(m.borrowed_count() == Member::kMaxBorrowed);
    CHECK(m.can_borrow() == false);

    // Duplicate vs Limit precedence: duplicate must throw duplicate error, not limit error
    CHECK_THROWS_WITH_AS(m.add_borrowed("ISBN1"), "book already borrowed by this member", std::logic_error);
    CHECK(m.borrowed_count() == Member::kMaxBorrowed);

    // New ISBN when full throws limit error
    CHECK_THROWS_WITH_AS(m.add_borrowed("ISBN_NEW"), "borrow limit reached", std::logic_error);
    CHECK(m.borrowed_count() == Member::kMaxBorrowed);

    // Remove one book restores can_borrow
    m.remove_borrowed("ISBN3");
    CHECK(m.borrowed_count() == Member::kMaxBorrowed - 1);
    CHECK(m.can_borrow() == true);

    m.add_borrowed("ISBN_NEW");
    CHECK(m.borrowed_count() == Member::kMaxBorrowed);
    CHECK(m.can_borrow() == false);
    CHECK(m.has_borrowed("ISBN_NEW"));
}

TEST_CASE("7. Member remove_borrowed ordering and error handling") {
    Member m(1, "Fiona");

    // Removing from empty throws
    CHECK_THROWS_WITH_AS(m.remove_borrowed("NONEXISTENT"), "book not borrowed by this member", std::logic_error);

    m.add_borrowed("A");
    m.add_borrowed("B");
    m.add_borrowed("C");
    m.add_borrowed("D");
    m.add_borrowed("E");

    // Remove middle ("C")
    m.remove_borrowed("C");
    CHECK(member_isbns_to_vector(m) == std::vector<std::string>{"A", "B", "D", "E"});

    // Remove first ("A")
    m.remove_borrowed("A");
    CHECK(member_isbns_to_vector(m) == std::vector<std::string>{"B", "D", "E"});

    // Remove last ("E")
    m.remove_borrowed("E");
    CHECK(member_isbns_to_vector(m) == std::vector<std::string>{"B", "D"});

    // Remove not borrowed throws
    CHECK_THROWS_WITH_AS(m.remove_borrowed("C"), "book not borrowed by this member", std::logic_error);
}

TEST_CASE("8. Member equality by id") {
    Member m1(10, "George");
    Member m2(10, "George Different");
    Member m3(20, "George");

    CHECK(m1 == m2);
    CHECK(m2 == m1);
    CHECK(m1 == m1);
    CHECK(!(m1 != m2));

    CHECK(m1 != m3);
    CHECK(m3 != m1);
    CHECK(!(m1 == m3));
}

TEST_CASE("9. Member copy/move semantics and type traits") {
    static_assert(!std::is_default_constructible_v<Member>, "Member must not be default constructible");
    static_assert(std::is_nothrow_move_constructible_v<Member>, "Member should be nothrow move constructible");
    static_assert(std::is_copy_constructible_v<Member>, "Member should be copy constructible");

    Member original(1, "Hannah");
    original.add_borrowed("ISBN1");
    original.add_borrowed("ISBN2");

    Member copy = original;
    copy.add_borrowed("ISBN3");
    CHECK(copy.borrowed_count() == 3);
    CHECK(original.borrowed_count() == 2);
    CHECK(!original.has_borrowed("ISBN3"));

    Member moved_target = std::move(original);
    CHECK(moved_target.id() == 1);
    CHECK(moved_target.name() == "Hannah");
    CHECK(moved_target.borrowed_count() == 2);
    CHECK(moved_target.has_borrowed("ISBN1"));
    CHECK(moved_target.has_borrowed("ISBN2"));
}

TEST_CASE("10. Integration with Container<Member>") {
    Container<Member> members;

    members.push_back(Member(103, "Charlie"));
    members.push_back(Member(101, "Alice"));
    members.push_back(Member(102, "Bob"));

    CHECK(members.size() == 3);

    // find with probe by ID
    Member probe(101, "Dummy Name");
    auto it = members.find(probe);
    CHECK(it != members.end());
    CHECK(it->name() == "Alice");

    // mutate through iterator
    it->add_borrowed("BOOK123");
    CHECK(it->has_borrowed("BOOK123"));
    CHECK(members.at(1).has_borrowed("BOOK123"));

    // remove_value with probe
    CHECK(members.remove_value(probe) == true);
    CHECK(members.size() == 2);
    CHECK(members.find(probe) == members.end());

    // find_if by name
    auto it_bob = members.find_if([](const Member& m) { return m.name() == "Bob"; });
    CHECK(it_bob != members.end());
    CHECK(it_bob->id() == 102);

    // sort by name with lambda
    members.sort([](const Member& lhs, const Member& rhs) {
        return lhs.name() < rhs.name();
    });

    std::vector<std::string> expected_order = {"Bob", "Charlie"};
    CHECK(member_names_to_vector(members) == expected_order);

    // Container copy is deep
    Container<Member> members_copy = members;
    CHECK(members_copy.size() == 2);
    members_copy.front().add_borrowed("NEW_ISBN");
    CHECK(members_copy.front().has_borrowed("NEW_ISBN"));
    CHECK(!members.front().has_borrowed("NEW_ISBN"));
}

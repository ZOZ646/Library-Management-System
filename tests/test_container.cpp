#include <doctest/doctest.h>
#include <core/Container.h>

#include <string>
#include <utility>

namespace {

struct Tracker {
    static int live_instances;
    static int copy_count;
    static int move_count;

    int value{0};

    static void reset_counts() {
        live_instances = 0;
        copy_count = 0;
        move_count = 0;
    }

    Tracker() : value(0) {
        ++live_instances;
    }

    explicit Tracker(int val) : value(val) {
        ++live_instances;
    }

    Tracker(const Tracker& other) : value(other.value) {
        ++live_instances;
        ++copy_count;
    }

    Tracker(Tracker&& other) noexcept : value(other.value) {
        ++live_instances;
        ++move_count;
        other.value = -1;
    }

    Tracker& operator=(const Tracker& other) {
        if (this != &other) {
            value = other.value;
            ++copy_count;
        }
        return *this;
    }

    Tracker& operator=(Tracker&& other) noexcept {
        if (this != &other) {
            value = other.value;
            ++move_count;
            other.value = -1;
        }
        return *this;
    }

    ~Tracker() {
        --live_instances;
    }

    bool operator==(const Tracker& other) const {
        return value == other.value;
    }
};

int Tracker::live_instances = 0;
int Tracker::copy_count = 0;
int Tracker::move_count = 0;

struct ThrowOnThirdCopy {
    static int live_instances;
    static int copy_attempts;
    int value{0};

    static void reset() {
        live_instances = 0;
        copy_attempts = 0;
    }

    ThrowOnThirdCopy() : value(0) {
        ++live_instances;
    }

    explicit ThrowOnThirdCopy(int val) : value(val) {
        ++live_instances;
    }

    ThrowOnThirdCopy(const ThrowOnThirdCopy& other) : value(other.value) {
        ++copy_attempts;
        if (copy_attempts == 3) {
            throw std::runtime_error("Simulated copy failure");
        }
        ++live_instances;
    }

    ThrowOnThirdCopy(ThrowOnThirdCopy&& other) noexcept : value(other.value) {
        ++live_instances;
        other.value = -1;
    }

    ThrowOnThirdCopy& operator=(const ThrowOnThirdCopy& other) = default;
    ThrowOnThirdCopy& operator=(ThrowOnThirdCopy&& other) noexcept = default;

    ~ThrowOnThirdCopy() {
        --live_instances;
    }
};

int ThrowOnThirdCopy::live_instances = 0;
int ThrowOnThirdCopy::copy_attempts = 0;

} // namespace

TEST_CASE("1. New container state and empty access exceptions") {
    titans::Container<int> c;
    CHECK(c.size() == 0);
    CHECK(c.empty());

    CHECK_THROWS_AS(c.front(), const std::out_of_range&);
    CHECK_THROWS_AS(c.back(), const std::out_of_range&);
    CHECK_THROWS_AS(c.at(0), const std::out_of_range&);

    const titans::Container<int>& const_c = c;
    CHECK_THROWS_AS(const_c.front(), const std::out_of_range&);
    CHECK_THROWS_AS(const_c.back(), const std::out_of_range&);
    CHECK_THROWS_AS(const_c.at(0), const std::out_of_range&);
}

TEST_CASE("2. push_back and push_front ordering") {
    SUBCASE("push_back x3 order") {
        titans::Container<int> c;
        c.push_back(10);
        c.push_back(20);
        c.push_back(30);

        CHECK(c.size() == 3);
        CHECK_FALSE(c.empty());
        CHECK(c.at(0) == 10);
        CHECK(c.at(1) == 20);
        CHECK(c.at(2) == 30);
    }

    SUBCASE("push_front x3 order") {
        titans::Container<int> c;
        c.push_front(10);
        c.push_front(20);
        c.push_front(30);

        CHECK(c.size() == 3);
        CHECK_FALSE(c.empty());
        CHECK(c.at(0) == 30);
        CHECK(c.at(1) == 20);
        CHECK(c.at(2) == 10);
    }

    SUBCASE("mix of push_back and push_front") {
        titans::Container<int> c;
        c.push_back(2);
        c.push_front(1);
        c.push_back(3);
        c.push_front(0);

        CHECK(c.size() == 4);
        CHECK(c.at(0) == 0);
        CHECK(c.at(1) == 1);
        CHECK(c.at(2) == 2);
        CHECK(c.at(3) == 3);
    }
}

TEST_CASE("3. front and back after each kind of insertion") {
    titans::Container<std::string> c;

    c.push_back("first");
    CHECK(c.front() == "first");
    CHECK(c.back() == "first");

    c.push_back("second");
    CHECK(c.front() == "first");
    CHECK(c.back() == "second");

    c.push_front("zero");
    CHECK(c.front() == "zero");
    CHECK(c.back() == "second");

    std::string val = "minus_one";
    c.push_front(std::move(val));
    CHECK(c.front() == "minus_one");
    CHECK(c.back() == "second");

    const auto& const_c = c;
    CHECK(const_c.front() == "minus_one");
    CHECK(const_c.back() == "second");
}

TEST_CASE("4. insert at position 0, middle, size(), and out of range") {
    titans::Container<int> c;

    // Insert into empty container at 0
    c.insert(0, 20);
    CHECK(c.size() == 1);
    CHECK(c.front() == 20);
    CHECK(c.back() == 20);

    // Insert at front (pos 0)
    c.insert(0, 10);
    CHECK(c.size() == 2);
    CHECK(c.at(0) == 10);
    CHECK(c.at(1) == 20);

    // Insert at end (pos == size())
    c.insert(c.size(), 40);
    CHECK(c.size() == 3);
    CHECK(c.at(2) == 40);

    // Insert in the middle
    c.insert(2, 30);
    CHECK(c.size() == 4);
    CHECK(c.at(0) == 10);
    CHECK(c.at(1) == 20);
    CHECK(c.at(2) == 30);
    CHECK(c.at(3) == 40);

    // Rvalue insert
    int temp = 5;
    c.insert(0, std::move(temp));
    CHECK(c.size() == 5);
    CHECK(c.front() == 5);

    // Out of range insert throws
    CHECK_THROWS_AS(c.insert(6, 99), const std::out_of_range&);
    CHECK_THROWS_AS(c.insert(100, 99), const std::out_of_range&);
}

TEST_CASE("5. remove_at: first, middle, last, only element, and out-of-range") {
    titans::Container<int> c;
    CHECK_THROWS_AS(c.remove_at(0), const std::out_of_range&);

    c.push_back(100);
    // Remove the only element
    c.remove_at(0);
    CHECK(c.empty());
    CHECK(c.size() == 0);
    CHECK_THROWS_AS(c.front(), const std::out_of_range&);
    CHECK_THROWS_AS(c.back(), const std::out_of_range&);

    // Container becomes empty and a later push works
    c.push_back(1);
    c.push_back(2);
    c.push_back(3);
    c.push_back(4);
    c.push_back(5);
    CHECK(c.size() == 5);

    // Remove middle (index 2 -> value 3)
    c.remove_at(2);
    CHECK(c.size() == 4);
    CHECK(c.at(0) == 1);
    CHECK(c.at(1) == 2);
    CHECK(c.at(2) == 4);
    CHECK(c.at(3) == 5);

    // Remove first (index 0 -> value 1)
    c.remove_at(0);
    CHECK(c.size() == 3);
    CHECK(c.front() == 2);
    CHECK(c.at(0) == 2);
    CHECK(c.at(1) == 4);
    CHECK(c.at(2) == 5);

    // Remove last (index 2 -> value 5)
    c.remove_at(2);
    CHECK(c.size() == 2);
    CHECK(c.front() == 2);
    CHECK(c.back() == 4);

    // Out of range check
    CHECK_THROWS_AS(c.remove_at(2), const std::out_of_range&);
    CHECK_THROWS_AS(c.remove_at(10), const std::out_of_range&);
}

TEST_CASE("6. remove_value: present, absent, duplicates, only, first, last") {
    titans::Container<int> c;

    // Absent in empty
    CHECK_FALSE(c.remove_value(42));

    // Only element
    c.push_back(42);
    CHECK(c.remove_value(42));
    CHECK(c.empty());
    CHECK(c.size() == 0);

    // Element absent when non-empty
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);
    CHECK_FALSE(c.remove_value(99));
    CHECK(c.size() == 3);

    // First element
    CHECK(c.remove_value(10));
    CHECK(c.size() == 2);
    CHECK(c.front() == 20);
    CHECK(c.back() == 30);

    // Last element
    CHECK(c.remove_value(30));
    CHECK(c.size() == 1);
    CHECK(c.front() == 20);
    CHECK(c.back() == 20);

    // Duplicates: removes only the FIRST element equal to value
    c.clear();
    c.push_back(5);
    c.push_back(7);
    c.push_back(5);
    c.push_back(9);
    c.push_back(5);

    CHECK(c.remove_value(5));
    CHECK(c.size() == 4);
    CHECK(c.at(0) == 7);
    CHECK(c.at(1) == 5);
    CHECK(c.at(2) == 9);
    CHECK(c.at(3) == 5);
}

TEST_CASE("7. Link integrity after removals from both ends") {
    titans::Container<int> c;
    for (int i = 1; i <= 5; ++i) {
        c.push_back(i * 10);
    }
    // [10, 20, 30, 40, 50]

    // Remove front
    c.remove_at(0); // [20, 30, 40, 50]
    CHECK(c.front() == 20);
    CHECK(c.back() == 50);
    CHECK(c.at(0) == 20);
    CHECK(c.at(3) == 50);

    // Remove back
    c.remove_at(c.size() - 1); // [20, 30, 40]
    CHECK(c.front() == 20);
    CHECK(c.back() == 40);
    CHECK(c.at(0) == 20);
    CHECK(c.at(1) == 30);
    CHECK(c.at(2) == 40);

    // Remove value at front
    CHECK(c.remove_value(20)); // [30, 40]
    CHECK(c.front() == 30);
    CHECK(c.back() == 40);

    // Remove value at back
    CHECK(c.remove_value(40)); // [30]
    CHECK(c.front() == 30);
    CHECK(c.back() == 30);

    // Remove last remaining
    CHECK(c.remove_value(30));
    CHECK(c.empty());

    // Verify container can still be pushed to and navigated
    c.push_front(999);
    CHECK(c.front() == 999);
    CHECK(c.back() == 999);
    CHECK(c.at(0) == 999);
}

TEST_CASE("8. Deep copy: constructor, assignment, and self-assignment") {
    titans::Container<std::string> original;
    original.push_back("alpha");
    original.push_back("beta");
    original.push_back("gamma");

    SUBCASE("Copy constructor independence") {
        titans::Container<std::string> copy(original);
        CHECK(copy.size() == original.size());
        CHECK(copy.at(0) == "alpha");
        CHECK(copy.at(1) == "beta");
        CHECK(copy.at(2) == "gamma");

        copy.at(0) = "modified";
        copy.push_back("delta");
        CHECK(original.at(0) == "alpha");
        CHECK(original.size() == 3);
        CHECK(copy.at(0) == "modified");
        CHECK(copy.size() == 4);
    }

    SUBCASE("Copy assignment independence") {
        titans::Container<std::string> assigned;
        assigned.push_back("initial");
        assigned = original;

        CHECK(assigned.size() == 3);
        CHECK(assigned.at(0) == "alpha");
        CHECK(assigned.at(1) == "beta");
        CHECK(assigned.at(2) == "gamma");

        assigned.remove_at(1);
        CHECK(assigned.size() == 2);
        CHECK(original.size() == 3);
        CHECK(original.at(1) == "beta");
    }

    SUBCASE("Self-assignment") {
        titans::Container<std::string>& alias = original;
        original = alias;
        CHECK(original.size() == 3);
        CHECK(original.at(0) == "alpha");
        CHECK(original.at(1) == "beta");
        CHECK(original.at(2) == "gamma");
    }

    SUBCASE("Exception safety during copy construction") {
        ThrowOnThirdCopy::reset();
        titans::Container<ThrowOnThirdCopy> source;
        source.push_back(ThrowOnThirdCopy(1));
        source.push_back(ThrowOnThirdCopy(2));
        source.push_back(ThrowOnThirdCopy(3));
        source.push_back(ThrowOnThirdCopy(4));

        CHECK(ThrowOnThirdCopy::live_instances == 4);
        ThrowOnThirdCopy::copy_attempts = 0;

        CHECK_THROWS_AS(titans::Container<ThrowOnThirdCopy> failed_copy(source), const std::runtime_error&);
        // After throwing and cleaning up partially allocated nodes, only the 4 source instances remain
        CHECK(ThrowOnThirdCopy::live_instances == 4);
    }
}

TEST_CASE("9. Move constructor and move assignment") {
    titans::Container<std::string> source;
    source.push_back("one");
    source.push_back("two");
    source.push_back("three");

    SUBCASE("Move constructor") {
        titans::Container<std::string> destination(std::move(source));
        CHECK(destination.size() == 3);
        CHECK(destination.at(0) == "one");
        CHECK(destination.at(1) == "two");
        CHECK(destination.at(2) == "three");

        CHECK(source.empty());
        CHECK(source.size() == 0);

        // Moved-from container is usable
        source.push_back("new_element");
        CHECK(source.size() == 1);
        CHECK(source.front() == "new_element");
    }

    SUBCASE("Move assignment") {
        titans::Container<std::string> destination;
        destination.push_back("prior");

        destination = std::move(source);
        CHECK(destination.size() == 3);
        CHECK(destination.at(0) == "one");
        CHECK(destination.at(1) == "two");
        CHECK(destination.at(2) == "three");

        CHECK(source.empty());
        CHECK(source.size() == 0);

        source.push_back("reusable");
        CHECK(source.size() == 1);
        CHECK(source.front() == "reusable");
    }

    SUBCASE("Move self-assignment") {
        titans::Container<std::string>& alias = source;
        source = std::move(alias);
        CHECK(source.size() == 3);
        CHECK(source.at(0) == "one");
        CHECK(source.at(1) == "two");
        CHECK(source.at(2) == "three");
    }
}

TEST_CASE("10. swap member and free function (ADL)") {
    titans::Container<int> a;
    a.push_back(1);
    a.push_back(2);

    titans::Container<int> b;
    b.push_back(10);
    b.push_back(20);
    b.push_back(30);

    SUBCASE("Member swap") {
        a.swap(b);
        CHECK(a.size() == 3);
        CHECK(a.at(0) == 10);
        CHECK(a.at(1) == 20);
        CHECK(a.at(2) == 30);

        CHECK(b.size() == 2);
        CHECK(b.at(0) == 1);
        CHECK(b.at(1) == 2);
    }

    SUBCASE("Free function swap via ADL") {
        using std::swap;
        swap(a, b);
        CHECK(a.size() == 3);
        CHECK(a.at(0) == 10);
        CHECK(b.size() == 2);
        CHECK(b.at(0) == 1);
    }
}

TEST_CASE("11. Type compatibility: int, std::string, Tracker, and move vs copy on push_back") {
    SUBCASE("int container") {
        titans::Container<int> nums;
        nums.push_back(42);
        CHECK(nums.front() == 42);
    }

    SUBCASE("std::string container") {
        titans::Container<std::string> words;
        words.push_back("Hello");
        words.push_back("Titans");
        CHECK(words.front() == "Hello");
        CHECK(words.back() == "Titans");
    }

    SUBCASE("Tracker instance counting and rvalue move verification") {
        Tracker::reset_counts();
        {
            titans::Container<Tracker> c;
            CHECK(Tracker::live_instances == 0);

            // push_back lvalue
            Tracker t1(10);
            c.push_back(t1);
            CHECK(Tracker::copy_count == 1);
            CHECK(Tracker::move_count == 0);

            // push_back rvalue should move, not copy
            int prior_copy_count = Tracker::copy_count;
            int prior_move_count = Tracker::move_count;
            c.push_back(Tracker(20));
            CHECK(Tracker::copy_count == prior_copy_count);
            CHECK(Tracker::move_count == prior_move_count + 1);

            // Copy container
            titans::Container<Tracker> c2 = c;
            CHECK(c2.size() == 2);

            // Destructor when out of scope
        }
        CHECK(Tracker::live_instances == 0);
    }
}

TEST_CASE("12. Stress test: 1000 elements, remove every other one, verify remaining") {
    titans::Container<int> c;
    constexpr int total = 1000;
    for (int i = 0; i < total; ++i) {
        c.push_back(i);
    }
    CHECK(c.size() == total);
    CHECK(c.front() == 0);
    CHECK(c.back() == total - 1);

    // Remove every other element (indices 0, 1, 2, ... in the shrinking container)
    // Step i=0 removes 0 (leaving 1, 2, 3, 4...)
    // Step i=1 removes current index 1 which is 2 (leaving 1, 3, 4, 5...)
    // This removes all originally even numbers: 0, 2, 4, 6...
    for (std::size_t i = 0; i < c.size(); ++i) {
        c.remove_at(i);
    }

    CHECK(c.size() == total / 2);
    CHECK(c.front() == 1);
    CHECK(c.back() == total - 1);

    for (std::size_t i = 0; i < c.size(); ++i) {
        int expected = static_cast<int>(2 * i + 1);
        CHECK(c.at(i) == expected);
    }

    // Clear and verify empty
    c.clear();
    CHECK(c.empty());
    CHECK(c.size() == 0);
}

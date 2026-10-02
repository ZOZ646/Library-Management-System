#include <doctest/doctest.h>
#include <core/Container.h>

#include <algorithm>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

namespace {

struct Point {
    int x{0};
    int y{0};

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

} // namespace

TEST_CASE("1. Empty container iterator semantics") {
    titans::Container<int> c;
    CHECK(c.begin() == c.end());
    CHECK(c.cbegin() == c.cend());

    int iterations = 0;
    for (int x : c) {
        (void)x;
        ++iterations;
    }
    CHECK(iterations == 0);
}

TEST_CASE("2. Forward traversal and range-for order") {
    titans::Container<int> c;
    const std::vector<int> expected = {10, 20, 30, 40, 50};
    for (int v : expected) {
        c.push_back(v);
    }

    std::vector<int> collected_manual;
    for (auto it = c.begin(); it != c.end(); ++it) {
        collected_manual.push_back(*it);
    }
    CHECK(collected_manual == expected);

    std::vector<int> collected_range_for;
    for (int v : c) {
        collected_range_for.push_back(v);
    }
    CHECK(collected_range_for == expected);
}

TEST_CASE("3. Backward traversal from end() stopping at begin()") {
    titans::Container<int> c;
    c.push_back(1);
    c.push_back(2);
    c.push_back(3);

    auto it = c.end();
    std::vector<int> backward;
    while (it != c.begin()) {
        --it;
        backward.push_back(*it);
    }

    const std::vector<int> expected = {3, 2, 1};
    CHECK(backward == expected);
    CHECK(it == c.begin());
}

TEST_CASE("4. Prefix vs postfix semantics for ++ and --") {
    titans::Container<int> c;
    c.push_back(100);
    c.push_back(200);
    c.push_back(300);

    auto it = c.begin();
    CHECK(*it == 100);

    auto old_it = it++;
    CHECK(*old_it == 100);
    CHECK(*it == 200);

    auto pre_it = ++it;
    CHECK(*pre_it == 300);
    CHECK(*it == 300);

    auto old_back = it--;
    CHECK(*old_back == 300);
    CHECK(*it == 200);

    auto pre_back = --it;
    CHECK(*pre_back == 100);
    CHECK(*it == 100);
}

TEST_CASE("5. Writing through iterator and arrow operator") {
    SUBCASE("Write through int iterator") {
        titans::Container<int> c;
        c.push_back(1);
        c.push_back(2);

        auto it = c.begin();
        *it = 42;
        CHECK(c.front() == 42);

        ++it;
        *it = 99;
        CHECK(c.back() == 99);
    }

    SUBCASE("Arrow operator on std::string") {
        titans::Container<std::string> words;
        words.push_back("Titans");

        auto it = words.begin();
        CHECK(it->size() == 6);
        CHECK_FALSE(it->empty());
    }

    SUBCASE("Arrow operator and modification on struct") {
        titans::Container<Point> points;
        points.push_back({10, 20});

        auto it = points.begin();
        CHECK(it->x == 10);
        CHECK(it->y == 20);

        it->x = 55;
        it->y = 77;
        CHECK(points.front().x == 55);
        CHECK(points.front().y == 77);
    }
}

TEST_CASE("6. Const-correctness") {
    titans::Container<int> c;
    c.push_back(10);
    c.push_back(20);

    const titans::Container<int>& const_c = c;
    auto cit = const_c.begin();
    static_assert(std::is_same_v<decltype(cit), titans::Container<int>::ConstIterator>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*cit)>>);

    CHECK(*cit == 10);
    ++cit;
    CHECK(*cit == 20);

    auto non_const_cbegin = c.cbegin();
    static_assert(std::is_same_v<decltype(non_const_cbegin), titans::Container<int>::ConstIterator>);
    CHECK(*non_const_cbegin == 10);
    CHECK(c.cbegin() != c.cend());
}

TEST_CASE("7. Iterator conversions and mixed comparisons") {
    static_assert(std::is_convertible_v<titans::Container<int>::Iterator, titans::Container<int>::ConstIterator>);
    static_assert(!std::is_convertible_v<titans::Container<int>::ConstIterator, titans::Container<int>::Iterator>);

    titans::Container<int> c;
    c.push_back(5);

    titans::Container<int>::Iterator it = c.begin();
    titans::Container<int>::ConstIterator cit = it; // implicit conversion

    CHECK(it == cit);
    CHECK(cit == it);
    CHECK_FALSE(it != cit);
    CHECK_FALSE(cit != it);

    titans::Container<int>::ConstIterator cend = c.cend();
    CHECK(it != cend);
    CHECK(cend != it);
    CHECK_FALSE(it == cend);
    CHECK_FALSE(cend == it);
}

TEST_CASE("8. Iterator traits and concept requirements") {
    using Iter = titans::Container<int>::Iterator;
    static_assert(std::is_same_v<
        typename std::iterator_traits<Iter>::iterator_category,
        std::bidirectional_iterator_tag
    >);
    static_assert(std::is_same_v<
        typename std::iterator_traits<Iter>::value_type,
        int
    >);
    static_assert(std::is_same_v<
        typename std::iterator_traits<Iter>::difference_type,
        std::ptrdiff_t
    >);
    static_assert(std::is_same_v<
        typename std::iterator_traits<Iter>::pointer,
        int*
    >);
    static_assert(std::is_same_v<
        typename std::iterator_traits<Iter>::reference,
        int&
    >);

    static_assert(std::is_default_constructible_v<Iter>);
    static_assert(std::is_copy_constructible_v<Iter>);
    static_assert(std::is_copy_assignable_v<Iter>);
    static_assert(std::is_destructible_v<Iter>);
}

TEST_CASE("9. Iterator error cases and bounds checking") {
    titans::Container<Point> c;
    c.push_back({1, 2});

    CHECK_THROWS_AS(*c.end(), const std::out_of_range&);
    CHECK_THROWS_AS(c.end()->x, const std::out_of_range&);
    CHECK_THROWS_AS(++c.end(), const std::out_of_range&);
    CHECK_THROWS_AS(c.end()++, const std::out_of_range&);
    CHECK_THROWS_AS(--c.begin(), const std::out_of_range&);
    CHECK_THROWS_AS(c.begin()--, const std::out_of_range&);

    titans::Container<Point> empty_c;
    CHECK_THROWS_AS(--empty_c.end(), const std::out_of_range&);
    CHECK_THROWS_AS(empty_c.end()--, const std::out_of_range&);

    titans::Container<Point>::Iterator singular;
    CHECK_THROWS_AS(*singular, const std::out_of_range&);
    CHECK_THROWS_AS(singular->x, const std::out_of_range&);
    CHECK_THROWS_AS(++singular, const std::out_of_range&);
    CHECK_THROWS_AS(singular++, const std::out_of_range&);
    CHECK_THROWS_AS(--singular, const std::out_of_range&);
    CHECK_THROWS_AS(singular--, const std::out_of_range&);

    titans::Container<Point>::Iterator singular2;
    CHECK(singular == singular2);

    titans::Container<Point> other_c;
    CHECK(c.end() != other_c.end());
}

TEST_CASE("10. next and prev methods on Container") {
    titans::Container<int> c;
    for (int i = 10; i <= 50; i += 10) {
        c.push_back(i);
    }
    // [10, 20, 30, 40, 50]

    SUBCASE("Iterator overload") {
        auto it = c.begin();
        CHECK(*c.next(it) == 20); // default step = 1
        CHECK(*c.next(it, 3) == 40);
        CHECK(c.next(it, 0) == it);
        CHECK(*c.prev(c.end()) == 50);
        CHECK(*c.next(c.begin()) == 20);

        CHECK_THROWS_AS(c.next(c.end()), const std::out_of_range&);
        CHECK_THROWS_AS(c.next(c.begin(), 10), const std::out_of_range&);
        CHECK_THROWS_AS(c.prev(c.begin()), const std::out_of_range&);

        titans::Container<int> other;
        other.push_back(10);
        CHECK_THROWS_AS(c.next(other.begin()), const std::invalid_argument&);
        CHECK_THROWS_AS(c.prev(other.begin()), const std::invalid_argument&);

        titans::Container<int>::Iterator singular;
        CHECK_THROWS_AS(c.next(singular), const std::invalid_argument&);
        CHECK_THROWS_AS(c.prev(singular), const std::invalid_argument&);
    }

    SUBCASE("ConstIterator overload") {
        const titans::Container<int>& const_c = c;
        auto cit = const_c.cbegin();
        CHECK(*const_c.next(cit) == 20);
        CHECK(*const_c.next(cit, 2) == 30);
        CHECK(const_c.next(cit, 0) == cit);
        CHECK(*const_c.prev(const_c.cend()) == 50);

        CHECK_THROWS_AS(const_c.next(const_c.cend()), const std::out_of_range&);
        CHECK_THROWS_AS(const_c.prev(const_c.cbegin()), const std::out_of_range&);

        titans::Container<int> other;
        CHECK_THROWS_AS(const_c.next(other.cbegin()), const std::invalid_argument&);
        CHECK_THROWS_AS(const_c.prev(other.cbegin()), const std::invalid_argument&);
    }
}

TEST_CASE("11. Standard library algorithms and utilities compatibility") {
    titans::Container<int> c;
    c.push_back(1);
    c.push_back(2);
    c.push_back(3);
    c.push_back(2);
    c.push_back(4);

    CHECK(std::distance(c.begin(), c.end()) == static_cast<std::ptrdiff_t>(c.size()));

    auto found = std::find(c.begin(), c.end(), 3);
    CHECK(found != c.end());
    CHECK(*found == 3);

    auto not_found = std::find(c.begin(), c.end(), 99);
    CHECK(not_found == c.end());

    CHECK(std::count(c.begin(), c.end(), 2) == 2);
    CHECK(std::accumulate(c.begin(), c.end(), 0) == 12);

    int sum_each = 0;
    std::for_each(c.begin(), c.end(), [&sum_each](int x) { sum_each += x; });
    CHECK(sum_each == 12);

    std::vector<int> vec;
    std::copy(c.begin(), c.end(), std::back_inserter(vec));
    const std::vector<int> expected = {1, 2, 3, 2, 4};
    CHECK(vec == expected);

    CHECK(*std::next(c.begin()) == 2);
    CHECK(*std::prev(c.end()) == 4);
    CHECK(*std::next(c.begin(), 2) == 3);
}

TEST_CASE("12. Reverse iteration with rbegin and rend") {
    titans::Container<int> c;
    const std::vector<int> values = {10, 20, 30, 40};
    for (int v : values) {
        c.push_back(v);
    }

    const std::vector<int> reversed = {40, 30, 20, 10};

    std::vector<int> rev_collected;
    for (auto rit = c.rbegin(); rit != c.rend(); ++rit) {
        rev_collected.push_back(*rit);
    }
    CHECK(rev_collected == reversed);

    const auto& const_c = c;
    std::vector<int> const_rev_collected;
    for (auto rit = const_c.rbegin(); rit != const_c.rend(); ++rit) {
        const_rev_collected.push_back(*rit);
    }
    CHECK(const_rev_collected == reversed);

    std::vector<int> crev_collected;
    for (auto rit = c.crbegin(); rit != c.crend(); ++rit) {
        crev_collected.push_back(*rit);
    }
    CHECK(crev_collected == reversed);
}

TEST_CASE("13. Iterator validity during container modifications") {
    titans::Container<int> c;
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);

    auto middle_it = ++c.begin();
    CHECK(*middle_it == 20);

    c.push_front(5);
    CHECK(*middle_it == 20);

    c.push_back(40);
    CHECK(*middle_it == 20);

    c.insert(1, 7);
    CHECK(*middle_it == 20);

    c.remove_at(0); // removes 5
    CHECK(*middle_it == 20);

    c.remove_value(40); // removes 40
    CHECK(*middle_it == 20);

    auto current_end = c.end();
    c.push_back(999);
    CHECK(c.end() == current_end); // end() iterator remains equal to container's end()
}

TEST_CASE("14. Iterator state after clear and copy independence") {
    titans::Container<std::string> c;
    c.push_back("apple");
    c.push_back("banana");

    c.clear();
    CHECK(c.begin() == c.end());

    c.push_back("cat");
    c.push_back("dog");

    titans::Container<std::string> copy = c;
    auto copy_it = copy.begin();
    *copy_it = "lion";

    CHECK(copy.front() == "lion");
    CHECK(c.front() == "cat"); // original is unchanged
}

TEST_CASE("15. Multi-type support: int, std::string, struct Point") {
    SUBCASE("int") {
        titans::Container<int> nums;
        nums.push_back(7);
        CHECK(*nums.begin() == 7);
    }

    SUBCASE("std::string") {
        titans::Container<std::string> text;
        text.push_back("Alpha");
        text.push_back("Omega");
        CHECK(*text.begin() == "Alpha");
        CHECK(*(--text.end()) == "Omega");
    }

    SUBCASE("Point") {
        titans::Container<Point> pts;
        pts.push_back({100, 200});
        pts.push_back({300, 400});
        CHECK(pts.begin()->x == 100);
        CHECK((--pts.end())->y == 400);
    }
}

TEST_CASE("16. Stress test: 1000 elements bidirectional traversal") {
    titans::Container<int> c;
    constexpr int total = 1000;
    for (int i = 1; i <= total; ++i) {
        c.push_back(i);
    }

    CHECK(std::distance(c.begin(), c.end()) == total);

    long long forward_sum = 0;
    for (int val : c) {
        forward_sum += val;
    }

    long long backward_sum = 0;
    int steps_backward = 0;
    auto it = c.end();
    while (it != c.begin()) {
        --it;
        backward_sum += *it;
        ++steps_backward;
    }

    CHECK(steps_backward == total);
    CHECK(forward_sum == backward_sum);
    constexpr long long expected_sum = static_cast<long long>(total) * (total + 1) / 2;
    CHECK(forward_sum == expected_sum);
}

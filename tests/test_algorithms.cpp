#include <doctest/doctest.h>
#include <core/Container.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename T>
std::vector<T> to_vector(const titans::Container<T>& c) {
    std::vector<T> vec;
    for (auto it = c.begin(); it != c.end(); ++it) {
        vec.push_back(*it);
    }
    return vec;
}

template <typename T>
std::vector<T> to_vector_backward(const titans::Container<T>& c) {
    std::vector<T> vec;
    if (c.empty()) {
        return vec;
    }
    auto it = c.end();
    while (it != c.begin()) {
        --it;
        vec.push_back(*it);
    }
    return vec;
}

template <typename T>
void check_integrity(const titans::Container<T>& c) {
    auto fwd = to_vector(c);
    auto bwd = to_vector_backward(c);

    CHECK(c.size() == fwd.size());
    CHECK(c.size() == bwd.size());

    std::vector<T> rev_fwd = fwd;
    std::reverse(rev_fwd.begin(), rev_fwd.end());
    CHECK(bwd == rev_fwd);

    if (!c.empty()) {
        CHECK(c.front() == fwd.front());
        CHECK(c.back() == fwd.back());
    } else {
        CHECK(c.empty());
    }
}

struct AlgoItem {
    int key{0};
    int id{0};

    bool operator==(const AlgoItem& other) const {
        return key == other.key && id == other.id;
    }

    bool operator<(const AlgoItem& other) const {
        if (key != other.key) {
            return key < other.key;
        }
        return id < other.id;
    }
};

struct AlgoStruct {
    std::string name;
    int score{0};

    bool operator==(const AlgoStruct& other) const {
        return name == other.name && score == other.score;
    }
};

struct AlgoTracker {
    static int live_instances;
    static int copy_count;
    static int move_count;
    static int copy_assign_count;
    static int move_assign_count;

    int value{0};

    static void reset_counts() {
        live_instances = 0;
        copy_count = 0;
        move_count = 0;
        copy_assign_count = 0;
        move_assign_count = 0;
    }

    AlgoTracker() : value(0) {
        ++live_instances;
    }

    explicit AlgoTracker(int val) : value(val) {
        ++live_instances;
    }

    AlgoTracker(const AlgoTracker& other) : value(other.value) {
        ++live_instances;
        ++copy_count;
    }

    AlgoTracker(AlgoTracker&& other) noexcept : value(other.value) {
        ++live_instances;
        ++move_count;
        other.value = -1;
    }

    AlgoTracker& operator=(const AlgoTracker& other) {
        if (this != &other) {
            value = other.value;
            ++copy_assign_count;
        }
        return *this;
    }

    AlgoTracker& operator=(AlgoTracker&& other) noexcept {
        if (this != &other) {
            value = other.value;
            ++move_assign_count;
            other.value = -1;
        }
        return *this;
    }

    ~AlgoTracker() {
        --live_instances;
    }

    bool operator==(const AlgoTracker& other) const {
        return value == other.value;
    }

    bool operator<(const AlgoTracker& other) const {
        return value < other.value;
    }
};

int AlgoTracker::live_instances = 0;
int AlgoTracker::copy_count = 0;
int AlgoTracker::move_count = 0;
int AlgoTracker::copy_assign_count = 0;
int AlgoTracker::move_assign_count = 0;

template <typename Compare = std::less<int>>
struct CountingComp {
    std::size_t count{0};
    Compare comp{};

    CountingComp() = default;
    explicit CountingComp(Compare c) : count(0), comp(c) {}

    template <typename A, typename B>
    bool operator()(const A& a, const B& b) {
        ++count;
        return comp(a, b);
    }
};

struct ThrowingComp {
    std::size_t current_call{0};
    std::size_t throw_at{0};

    explicit ThrowingComp(std::size_t throw_call) : current_call(0), throw_at(throw_call) {}

    template <typename T>
    bool operator()(const T& a, const T& b) {
        ++current_call;
        if (current_call == throw_at) {
            throw std::runtime_error("ThrowingComp intentional throw");
        }
        return a < b;
    }
};

struct SorterMerge {
    static const char* name() { return "MergeSort"; }
    template <typename T, typename Comp>
    void operator()(titans::Container<T>& c, Comp comp) const {
        c.sort(comp);
    }
    template <typename T>
    void operator()(titans::Container<T>& c) const {
        c.sort();
    }
};

struct SorterInsertion {
    static const char* name() { return "InsertionSort"; }
    template <typename T, typename Comp>
    void operator()(titans::Container<T>& c, Comp comp) const {
        c.insertion_sort(comp);
    }
    template <typename T>
    void operator()(titans::Container<T>& c) const {
        c.insertion_sort();
    }
};

} // namespace

TEST_CASE("1. find: present, absent, empty, duplicates, first, last") {
    titans::Container<int> empty_c;
    CHECK(empty_c.find(10) == empty_c.end());

    titans::Container<int> c;
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);
    c.push_back(20);
    c.push_back(40);

    // Absent
    CHECK(c.find(99) == c.end());

    // First element
    auto it_first = c.find(10);
    CHECK(it_first == c.begin());
    CHECK(*it_first == 10);

    // Last element
    auto it_last = c.find(40);
    CHECK(*it_last == 40);
    CHECK(std::distance(c.begin(), it_last) == 4);

    // Duplicates: returns the FIRST one
    auto it_dup = c.find(20);
    CHECK(it_dup != c.end());
    CHECK(*it_dup == 20);
    CHECK(std::distance(c.begin(), it_dup) == 1);
}

TEST_CASE("2. find on std::string, struct, and writing through iterator") {
    SUBCASE("std::string and modification") {
        titans::Container<std::string> words;
        words.push_back("apple");
        words.push_back("banana");
        words.push_back("cherry");

        auto it = words.find("banana");
        CHECK(it != words.end());
        CHECK(*it == "banana");

        *it = "blueberry";
        CHECK(words.at(1) == "blueberry");
    }

    SUBCASE("struct with operator==") {
        titans::Container<AlgoStruct> items;
        items.push_back({"Alice", 95});
        items.push_back({"Bob", 80});

        auto it = items.find({"Bob", 80});
        CHECK(it != items.end());
        CHECK(it->name == "Bob");
        CHECK(it->score == 80);

        it->score = 88;
        CHECK(items.back().score == 88);

        CHECK(items.find({"Charlie", 50}) == items.end());
    }
}

TEST_CASE("3. const overloads of find and find_if") {
    titans::Container<int> c;
    c.push_back(100);
    c.push_back(200);

    const titans::Container<int>& const_c = c;

    auto cit_find = const_c.find(200);
    static_assert(std::is_same_v<decltype(cit_find), titans::Container<int>::ConstIterator>);
    CHECK(cit_find != const_c.end());
    CHECK(*cit_find == 200);

    auto cit_find_if = const_c.find_if([](int x) { return x > 150; });
    static_assert(std::is_same_v<decltype(cit_find_if), titans::Container<int>::ConstIterator>);
    CHECK(cit_find_if != const_c.end());
    CHECK(*cit_find_if == 200);

    CHECK(const_c.find(999) == const_c.end());
    CHECK(const_c.find_if([](int x) { return x < 0; }) == const_c.end());
}

TEST_CASE("4. find_if: first match, no match, empty, captured state, and stop on first match") {
    titans::Container<int> empty_c;
    CHECK(empty_c.find_if([](int) { return true; }) == empty_c.end());

    titans::Container<int> c;
    for (int i = 1; i <= 10; ++i) {
        c.push_back(i);
    }

    // No match
    CHECK(c.find_if([](int x) { return x > 100; }) == c.end());

    // First match with call counter
    int call_count = 0;
    auto it = c.find_if([&call_count](int x) {
        ++call_count;
        return x == 4; // 4th element
    });
    CHECK(it != c.end());
    CHECK(*it == 4);
    CHECK(call_count == 4); // Stopped exactly at the 4th element

    // Lambda with captured state
    int threshold = 7;
    auto it_state = c.find_if([threshold](int x) { return x >= threshold; });
    CHECK(it_state != c.end());
    CHECK(*it_state == 7);
}

TEST_CASE("5. Iterator returned by find works with next, prev, ++, --") {
    titans::Container<int> c;
    for (int i = 1; i <= 5; ++i) {
        c.push_back(i * 10);
    }
    // [10, 20, 30, 40, 50]

    auto it = c.find(30);
    CHECK(it != c.end());
    CHECK(*it == 30);

    CHECK(*c.next(it) == 40);
    CHECK(*c.prev(it) == 20);

    auto next_it = it;
    ++next_it;
    CHECK(*next_it == 40);

    auto prev_it = it;
    --prev_it;
    CHECK(*prev_it == 20);
}

template <typename Sorter>
void run_sort_test_suite(Sorter sorter) {
    INFO("Running test suite for: ", Sorter::name());

    SUBCASE("6. Empty, single, two elements") {
        titans::Container<int> empty_c;
        sorter(empty_c);
        check_integrity(empty_c);
        CHECK(empty_c.empty());

        titans::Container<int> single_c;
        single_c.push_back(42);
        sorter(single_c);
        check_integrity(single_c);
        CHECK(single_c.size() == 1);
        CHECK(single_c.front() == 42);

        titans::Container<int> two_sorted;
        two_sorted.push_back(1);
        two_sorted.push_back(2);
        sorter(two_sorted);
        check_integrity(two_sorted);
        CHECK(two_sorted.front() == 1);
        CHECK(two_sorted.back() == 2);

        titans::Container<int> two_unsorted;
        two_unsorted.push_back(2);
        two_unsorted.push_back(1);
        sorter(two_unsorted);
        check_integrity(two_unsorted);
        CHECK(two_unsorted.front() == 1);
        CHECK(two_unsorted.back() == 2);
    }

    SUBCASE("7. Already sorted, strictly reverse sorted, all equal, duplicates") {
        // Already sorted
        titans::Container<int> sorted_c;
        for (int i = 1; i <= 20; ++i) sorted_c.push_back(i);
        sorter(sorted_c);
        check_integrity(sorted_c);
        for (int i = 0; i < 20; ++i) CHECK(sorted_c.at(i) == i + 1);

        // Strictly reverse sorted
        titans::Container<int> reverse_c;
        for (int i = 20; i >= 1; --i) reverse_c.push_back(i);
        sorter(reverse_c);
        check_integrity(reverse_c);
        for (int i = 0; i < 20; ++i) CHECK(reverse_c.at(i) == i + 1);

        // All elements equal
        titans::Container<int> equal_c;
        for (int i = 0; i < 15; ++i) equal_c.push_back(7);
        sorter(equal_c);
        check_integrity(equal_c);
        for (int i = 0; i < 15; ++i) CHECK(equal_c.at(i) == 7);

        // Many duplicates
        titans::Container<int> dups;
        std::vector<int> dup_vals = {5, 2, 8, 5, 2, 8, 1, 9, 1, 5, 2};
        for (int v : dup_vals) dups.push_back(v);
        sorter(dups);
        check_integrity(dups);
        std::sort(dup_vals.begin(), dup_vals.end());
        CHECK(to_vector(dups) == dup_vals);
    }

    SUBCASE("8. 500 pseudo-random ints matching std::sort") {
        std::mt19937 gen(1337);
        std::uniform_int_distribution<int> dist(-1000, 1000);

        titans::Container<int> c;
        std::vector<int> expected;
        for (int i = 0; i < 500; ++i) {
            int val = dist(gen);
            c.push_back(val);
            expected.push_back(val);
        }

        std::sort(expected.begin(), expected.end());
        sorter(c);
        check_integrity(c);
        CHECK(to_vector(c) == expected);
    }

    SUBCASE("9. Descending order and no-arg overloads on int and string") {
        titans::Container<int> desc_c;
        for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) desc_c.push_back(v);
        sorter(desc_c, std::greater<int>());
        check_integrity(desc_c);
        CHECK(std::is_sorted(desc_c.begin(), desc_c.end(), std::greater<int>()));

        titans::Container<int> lambda_c;
        for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) lambda_c.push_back(v);
        sorter(lambda_c, [](int a, int b) { return a > b; });
        check_integrity(lambda_c);
        CHECK(std::is_sorted(lambda_c.begin(), lambda_c.end(), std::greater<int>()));

        titans::Container<std::string> words;
        words.push_back("banana");
        words.push_back("apple");
        words.push_back("date");
        words.push_back("cherry");
        sorter(words);
        check_integrity(words);
        const std::vector<std::string> expected_words = {"apple", "banana", "cherry", "date"};
        CHECK(to_vector(words) == expected_words);
    }

    SUBCASE("10. Sorting a struct by one field with custom comparator") {
        titans::Container<AlgoStruct> items;
        items.push_back({"Bob", 80});
        items.push_back({"Alice", 95});
        items.push_back({"Charlie", 70});

        sorter(items, [](const AlgoStruct& a, const AlgoStruct& b) {
            return a.score < b.score;
        });
        check_integrity(items);
        CHECK(items.front().name == "Charlie");
        CHECK(items.back().name == "Alice");
    }

    SUBCASE("11. STABILITY: elements with equal keys preserve relative order") {
        titans::Container<AlgoItem> c;
        std::vector<AlgoItem> vec;
        std::mt19937 gen(42);
        std::uniform_int_distribution<int> key_dist(0, 9); // few keys -> many equal keys

        for (int i = 0; i < 200; ++i) {
            AlgoItem item{key_dist(gen), i};
            c.push_back(item);
            vec.push_back(item);
        }

        std::stable_sort(vec.begin(), vec.end(), [](const AlgoItem& a, const AlgoItem& b) {
            return a.key < b.key;
        });

        sorter(c, [](const AlgoItem& a, const AlgoItem& b) {
            return a.key < b.key;
        });

        check_integrity(c);
        CHECK(to_vector(c) == vec);
    }

    SUBCASE("12. Usability after sorting") {
        titans::Container<int> c;
        for (int v : {50, 20, 80, 10, 40}) c.push_back(v);
        sorter(c);
        check_integrity(c);

        c.push_back(100);
        c.push_front(0);
        c.insert(3, 25);
        c.remove_at(1); // removes 10
        CHECK(c.remove_value(80));

        check_integrity(c);
        CHECK(c.at(0) == 0);
        CHECK(c.at(1) == 20);
        CHECK(c.at(2) == 25);
        CHECK(c.at(3) == 40);
        CHECK(c.at(4) == 50);
        CHECK(c.at(5) == 100);

        titans::Container<int> copy = c;
        check_integrity(copy);
        CHECK(to_vector(copy) == to_vector(c));
    }

    SUBCASE("13. Iterator behavior across sort") {
        titans::Container<AlgoItem> c;
        c.push_back({3, 100});
        c.push_back({1, 200});
        c.push_back({2, 300});

        auto target_it = c.begin();
        ++target_it; // points to {1, 200}
        CHECK(target_it->id == 200);

        auto pre_end = c.end();

        sorter(c, [](const AlgoItem& a, const AlgoItem& b) {
            return a.key < b.key;
        });

        check_integrity(c);
        CHECK(target_it->id == 200);
        CHECK(target_it->key == 1);
        CHECK(std::distance(c.begin(), target_it) == 0); // moved to index 0
        CHECK(c.end() == pre_end);
    }

    SUBCASE("14. No element copies or moves during sort") {
        AlgoTracker::reset_counts();
        titans::Container<AlgoTracker> c;
        for (int v : {9, 2, 7, 1, 8, 3, 6, 4, 5}) {
            c.push_back(AlgoTracker(v));
        }

        const int copies_before = AlgoTracker::copy_count;
        const int moves_before = AlgoTracker::move_count;
        const int copy_assign_before = AlgoTracker::copy_assign_count;
        const int move_assign_before = AlgoTracker::move_assign_count;
        const int live_before = AlgoTracker::live_instances;

        sorter(c);

        CHECK(AlgoTracker::copy_count == copies_before);
        CHECK(AlgoTracker::move_count == moves_before);
        CHECK(AlgoTracker::copy_assign_count == copy_assign_before);
        CHECK(AlgoTracker::move_assign_count == move_assign_before);
        CHECK(AlgoTracker::live_instances == live_before);

        // Verify sorted order without extra copies of AlgoTracker
        int last_val = -1;
        for (auto it = c.begin(); it != c.end(); ++it) {
            CHECK(it->value >= last_val);
            last_val = it->value;
        }
    }
}

TEST_CASE("MergeSort functional test suite") {
    run_sort_test_suite(SorterMerge{});
}

TEST_CASE("InsertionSort functional test suite") {
    run_sort_test_suite(SorterInsertion{});
}

TEST_CASE("15. Complexity sanity comparison counts") {
    constexpr std::size_t n = 1000;
    std::mt19937 gen(777);
    std::uniform_int_distribution<int> dist(0, 100000);

    titans::Container<int> random_c;
    for (std::size_t i = 0; i < n; ++i) {
        random_c.push_back(dist(gen));
    }

    // Merge sort on 1000 random ints
    CountingComp<std::less<int>> merge_comp;
    random_c.sort(std::ref(merge_comp));
    check_integrity(random_c);

    const std::size_t ceil_log2_n = static_cast<std::size_t>(std::ceil(std::log2(n))); // 10
    const std::size_t max_merge_bound = n * ceil_log2_n; // 10000

    MESSAGE("Measured MergeSort comparisons on n=1000 random ints: ", merge_comp.count);
    CHECK(merge_comp.count <= max_merge_bound);

    // Insertion sort on already sorted container of size 1000
    titans::Container<int> sorted_for_insertion = random_c;
    CountingComp<std::less<int>> insertion_comp;
    sorted_for_insertion.insertion_sort(std::ref(insertion_comp));
    check_integrity(sorted_for_insertion);

    MESSAGE("Measured InsertionSort comparisons on n=1000 sorted ints: ", insertion_comp.count);
    CHECK(insertion_comp.count == n - 1);

    // Merge sort on already sorted container
    titans::Container<int> sorted_for_merge = random_c;
    CountingComp<std::less<int>> merge_sorted_comp;
    sorted_for_merge.sort(std::ref(merge_sorted_comp));
    check_integrity(sorted_for_merge);

    MESSAGE("Measured MergeSort comparisons on n=1000 sorted ints: ", merge_sorted_comp.count);
    CHECK(merge_sorted_comp.count <= max_merge_bound);
}

template <typename Sorter>
void test_exception_safety(Sorter sorter) {
    INFO("Exception safety for: ", Sorter::name());
    std::mt19937 gen(12345);
    std::uniform_int_distribution<int> dist(1, 1000);

    std::vector<int> original;
    constexpr std::size_t sample_size = 40;
    for (std::size_t i = 0; i < sample_size; ++i) {
        original.push_back(dist(gen));
    }

    std::vector<int> sorted_original = original;
    std::sort(sorted_original.begin(), sorted_original.end());

    std::vector<std::size_t> k_values;
    for (std::size_t k = 1; k <= 60; ++k) {
        k_values.push_back(k);
    }
    k_values.push_back(70);
    k_values.push_back(80);
    k_values.push_back(100);

    for (std::size_t k : k_values) {
        titans::Container<int> c;
        for (int v : original) {
            c.push_back(v);
        }

        ThrowingComp comp(k);
        bool threw = false;
        try {
            sorter(c, comp);
        } catch (const std::runtime_error&) {
            threw = true;
        }

        if (threw) {
            check_integrity(c);
            CHECK(c.size() == sample_size);

            auto after = to_vector(c);
            std::sort(after.begin(), after.end());
            CHECK(after == sorted_original);

            c.sort();
            check_integrity(c);
            CHECK(to_vector(c) == sorted_original);
        }
    }

    // Repeat with AlgoTracker to verify zero leaks
    for (std::size_t k : {1, 5, 10, 20, 30, 40, 50, 60}) {
        AlgoTracker::reset_counts();
        {
            titans::Container<AlgoTracker> tc;
            for (int v : original) {
                tc.push_back(AlgoTracker(v));
            }
            CHECK(AlgoTracker::live_instances == static_cast<int>(sample_size));

            ThrowingComp comp(k);
            try {
                sorter(tc, comp);
            } catch (const std::runtime_error&) {
                // Expected
            }
            check_integrity(tc);
            CHECK(tc.size() == sample_size);
        }
        CHECK(AlgoTracker::live_instances == 0);
    }
}

TEST_CASE("16. Exception safety: throwing comparator leaves valid container and leaks nothing") {
    test_exception_safety(SorterMerge{});
    test_exception_safety(SorterInsertion{});
}

TEST_CASE("17. Large input scale test") {
    SUBCASE("Merge sort on 100000 ints") {
        titans::Container<int> big_c;
        constexpr int count = 100000;
        std::mt19937 gen(999);
        std::uniform_int_distribution<int> dist(-500000, 500000);

        for (int i = 0; i < count; ++i) {
            big_c.push_back(dist(gen));
        }

        big_c.sort();
        CHECK(big_c.size() == count);
        CHECK(std::is_sorted(big_c.begin(), big_c.end()));
        CHECK(big_c.front() <= big_c.back());

        // Check front/back and backward traversal sample
        auto bwd = big_c.end();
        --bwd;
        CHECK(*bwd == big_c.back());
    }

    SUBCASE("Insertion sort on 2000 ints") {
        titans::Container<int> small_c;
        constexpr int count = 2000;
        std::mt19937 gen(888);
        std::uniform_int_distribution<int> dist(-10000, 10000);

        for (int i = 0; i < count; ++i) {
            small_c.push_back(dist(gen));
        }

        small_c.insertion_sort();
        check_integrity(small_c);
        CHECK(small_c.size() == count);
        CHECK(std::is_sorted(small_c.begin(), small_c.end()));
    }
}

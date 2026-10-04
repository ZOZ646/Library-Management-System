#include "core/Container.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <list>
#include <random>
#include <string>
#include <vector>

#ifndef TITANS_BUILD_TYPE
#define TITANS_BUILD_TYPE "unknown"
#endif

namespace {

volatile long long g_sink = 0;

double median_of_7(std::vector<double>& v) {
    std::sort(v.begin(), v.end());
    return v[3];
}

std::vector<int> generate_random_ints(std::size_t n, std::mt19937& rng) {
    std::vector<int> data(n);
    std::uniform_int_distribution<int> dist(-1000000, 1000000);
    for (std::size_t i = 0; i < n; ++i) {
        data[i] = dist(rng);
    }
    return data;
}

} // namespace

int main() {
#if defined(_MSC_VER)
    std::cout << "Compiler: MSVC " << _MSC_VER << "\n";
#elif defined(__clang__)
    std::cout << "Compiler: Clang " << __clang_version__ << "\n";
#elif defined(__GNUC__)
    std::cout << "Compiler: GCC " << __VERSION__ << "\n";
#else
    std::cout << "Compiler: Unknown\n";
#endif

    std::cout << "Build type: " << TITANS_BUILD_TYPE << "\n";
    std::cout << "n = 1000, 10000, 100000; median of 7 runs\n\n";

#if defined(__SANITIZE_ADDRESS__)
    std::cout << "WARNING: AddressSanitizer is enabled! Benchmark timings may be significantly affected.\n\n";
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer)
    std::cout << "WARNING: AddressSanitizer is enabled! Benchmark timings may be significantly affected.\n\n";
  #endif
#endif

    const std::vector<std::size_t> sizes = {1000, 10000, 100000};
    std::cout << std::fixed << std::setprecision(3);

    // (a) push_back n elements
    std::cout << "### push_back (ms)\n\n";
    std::cout << "| n | titans::Container<int> | std::list<int> |\n";
    std::cout << "|---|---|---|\n";

    for (std::size_t n : sizes) {
        std::vector<double> time_titans(7);
        std::vector<double> time_std(7);

        for (int run = 0; run < 7; ++run) {
            std::mt19937 rng(12345 + static_cast<unsigned int>(run));
            const std::vector<int> data = generate_random_ints(n, rng);

            {
                auto start = std::chrono::steady_clock::now();
                titans::Container<int> c;
                for (int val : data) {
                    c.push_back(val);
                }
                auto finish = std::chrono::steady_clock::now();
                time_titans[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                g_sink += static_cast<long long>(c.size());
            }

            {
                auto start = std::chrono::steady_clock::now();
                std::list<int> l;
                for (int val : data) {
                    l.push_back(val);
                }
                auto finish = std::chrono::steady_clock::now();
                time_std[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                g_sink += static_cast<long long>(l.size());
            }
        }

        std::cout << "| " << n << " | " << median_of_7(time_titans) << " | "
                  << median_of_7(time_std) << " |\n";
    }
    std::cout << "\n";

    // (b) iterate once and sum
    std::cout << "### Iterate and sum (ms)\n\n";
    std::cout << "| n | titans::Container<int> | std::list<int> |\n";
    std::cout << "|---|---|---|\n";

    for (std::size_t n : sizes) {
        std::vector<double> time_titans(7);
        std::vector<double> time_std(7);

        for (int run = 0; run < 7; ++run) {
            std::mt19937 rng(12345 + static_cast<unsigned int>(run));
            const std::vector<int> data = generate_random_ints(n, rng);

            titans::Container<int> c;
            for (int val : data) {
                c.push_back(val);
            }
            std::list<int> l(data.begin(), data.end());

            {
                auto start = std::chrono::steady_clock::now();
                long long sum = 0;
                for (auto it = c.begin(); it != c.end(); ++it) {
                    sum += *it;
                }
                auto finish = std::chrono::steady_clock::now();
                time_titans[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                g_sink += sum;
            }

            {
                auto start = std::chrono::steady_clock::now();
                long long sum = 0;
                for (auto it = l.begin(); it != l.end(); ++it) {
                    sum += *it;
                }
                auto finish = std::chrono::steady_clock::now();
                time_std[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                g_sink += sum;
            }
        }

        std::cout << "| " << n << " | " << median_of_7(time_titans) << " | "
                  << median_of_7(time_std) << " |\n";
    }
    std::cout << "\n";

    // (c) find of the LAST element (worst case, repeat enough times, report in us/call)
    std::cout << "### find last element (us/call)\n\n";
    std::cout << "| n | titans::Container<int> | std::list<int> |\n";
    std::cout << "|---|---|---|\n";

    for (std::size_t n : sizes) {
        std::vector<double> time_titans(7);
        std::vector<double> time_std(7);

        const int repeats = (n <= 1000) ? 500 : ((n <= 10000) ? 100 : 20);

        for (int run = 0; run < 7; ++run) {
            std::mt19937 rng(12345 + static_cast<unsigned int>(run));
            const std::vector<int> data = generate_random_ints(n, rng);
            const int target = data.back();

            titans::Container<int> c;
            for (int val : data) {
                c.push_back(val);
            }
            std::list<int> l(data.begin(), data.end());

            {
                auto start = std::chrono::steady_clock::now();
                for (int rep = 0; rep < repeats; ++rep) {
                    auto it = c.find(target);
                    if (it != c.end()) {
                        g_sink += *it;
                    }
                }
                auto finish = std::chrono::steady_clock::now();
                double total_us = std::chrono::duration<double, std::micro>(finish - start).count();
                time_titans[static_cast<std::size_t>(run)] = total_us / static_cast<double>(repeats);
            }

            {
                auto start = std::chrono::steady_clock::now();
                for (int rep = 0; rep < repeats; ++rep) {
                    auto it = std::find(l.begin(), l.end(), target);
                    if (it != l.end()) {
                        g_sink += *it;
                    }
                }
                auto finish = std::chrono::steady_clock::now();
                double total_us = std::chrono::duration<double, std::micro>(finish - start).count();
                time_std[static_cast<std::size_t>(run)] = total_us / static_cast<double>(repeats);
            }
        }

        std::cout << "| " << n << " | " << median_of_7(time_titans) << " | "
                  << median_of_7(time_std) << " |\n";
    }
    std::cout << "\n";

    // (d) sort a container of n random ints (Container::sort() vs std::list::sort())
    std::cout << "### Sort random elements (ms)\n\n";
    std::cout << "| n | titans::Container<int>::sort() | std::list<int>::sort() |\n";
    std::cout << "|---|---|---|\n";

    for (std::size_t n : sizes) {
        std::vector<double> time_titans(7);
        std::vector<double> time_std(7);

        for (int run = 0; run < 7; ++run) {
            std::mt19937 rng(12345 + static_cast<unsigned int>(run));
            const std::vector<int> data = generate_random_ints(n, rng);

            {
                titans::Container<int> c;
                for (int val : data) {
                    c.push_back(val);
                }
                auto start = std::chrono::steady_clock::now();
                c.sort();
                auto finish = std::chrono::steady_clock::now();
                time_titans[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                if (!c.empty()) {
                    g_sink += c.front();
                }
            }

            {
                std::list<int> l(data.begin(), data.end());
                auto start = std::chrono::steady_clock::now();
                l.sort();
                auto finish = std::chrono::steady_clock::now();
                time_std[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                if (!l.empty()) {
                    g_sink += l.front();
                }
            }
        }

        std::cout << "| " << n << " | " << median_of_7(time_titans) << " | "
                  << median_of_7(time_std) << " |\n";
    }
    std::cout << "\n";

    // (e) Container::insertion_sort() on 1000 and 10000 random elements, and 100000 sorted elements
    std::cout << "### titans::Container<int>::insertion_sort() (ms)\n\n";
    std::cout << "| Input condition | n | titans::Container<int>::insertion_sort() |\n";
    std::cout << "|---|---|---|\n";

    // 1000 random
    {
        std::vector<double> times(7);
        for (int run = 0; run < 7; ++run) {
            std::mt19937 rng(12345 + static_cast<unsigned int>(run));
            const std::vector<int> data = generate_random_ints(1000, rng);
            titans::Container<int> c;
            for (int val : data) {
                c.push_back(val);
            }
            auto start = std::chrono::steady_clock::now();
            c.insertion_sort();
            auto finish = std::chrono::steady_clock::now();
            times[static_cast<std::size_t>(run)] =
                std::chrono::duration<double, std::milli>(finish - start).count();
            if (!c.empty()) {
                g_sink += c.front();
            }
        }
        std::cout << "| Random (O(n^2)) | 1000 | " << median_of_7(times) << " |\n";
    }

    // 10000 random
    {
        std::vector<double> times(7);
        for (int run = 0; run < 7; ++run) {
            std::mt19937 rng(12345 + static_cast<unsigned int>(run));
            const std::vector<int> data = generate_random_ints(10000, rng);
            titans::Container<int> c;
            for (int val : data) {
                c.push_back(val);
            }
            auto start = std::chrono::steady_clock::now();
            c.insertion_sort();
            auto finish = std::chrono::steady_clock::now();
            times[static_cast<std::size_t>(run)] =
                std::chrono::duration<double, std::milli>(finish - start).count();
            if (!c.empty()) {
                g_sink += c.front();
            }
        }
        std::cout << "| Random (O(n^2)) | 10000 | " << median_of_7(times) << " |\n";
    }

    // 100000 already sorted (O(n) best case)
    {
        std::vector<double> times(7);
        for (int run = 0; run < 7; ++run) {
            titans::Container<int> c;
            for (int i = 0; i < 100000; ++i) {
                c.push_back(i);
            }
            auto start = std::chrono::steady_clock::now();
            c.insertion_sort();
            auto finish = std::chrono::steady_clock::now();
            times[static_cast<std::size_t>(run)] =
                std::chrono::duration<double, std::milli>(finish - start).count();
            if (!c.empty()) {
                g_sink += c.front();
            }
        }
        std::cout << "| Sorted (O(n) best case) | 100000 | " << median_of_7(times) << " |\n";
    }
    std::cout << "\n";

    // (f) remove_value of every 2nd value (loop) for n = 10000 only, for both
    std::cout << "### remove_value of every 2nd element (ms)\n\n";
    std::cout << "| n | titans::Container<int>::remove_value | std::list<int>::remove |\n";
    std::cout << "|---|---|---|\n";

    {
        const std::size_t n = 10000;
        std::vector<double> time_titans(7);
        std::vector<double> time_std(7);

        for (int run = 0; run < 7; ++run) {
            std::mt19937 rng(12345 + static_cast<unsigned int>(run));
            const std::vector<int> data = generate_random_ints(n, rng);

            std::vector<int> values_to_remove;
            values_to_remove.reserve(n / 2);
            for (std::size_t i = 0; i < n; i += 2) {
                values_to_remove.push_back(data[i]);
            }

            {
                titans::Container<int> c;
                for (int val : data) {
                    c.push_back(val);
                }
                auto start = std::chrono::steady_clock::now();
                for (int val : values_to_remove) {
                    c.remove_value(val);
                }
                auto finish = std::chrono::steady_clock::now();
                time_titans[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                g_sink += static_cast<long long>(c.size());
            }

            {
                std::list<int> l(data.begin(), data.end());
                auto start = std::chrono::steady_clock::now();
                for (int val : values_to_remove) {
                    l.remove(val);
                }
                auto finish = std::chrono::steady_clock::now();
                time_std[static_cast<std::size_t>(run)] =
                    std::chrono::duration<double, std::milli>(finish - start).count();
                g_sink += static_cast<long long>(l.size());
            }
        }

        std::cout << "| " << n << " | " << median_of_7(time_titans) << " | "
                  << median_of_7(time_std) << " |\n";
    }
    std::cout << "\n";

    return 0;
}

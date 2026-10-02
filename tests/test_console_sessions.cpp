#include <doctest/doctest.h>
#include "ui/ConsoleApp.h"
#include "service/LibraryService.h"
#include "tests/library_fixtures.h"

#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace titans;
using namespace titans::fixtures;

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string run_script(LibraryService& service, const std::string& input, int* exit_code = nullptr) {
    std::istringstream in(input);
    std::ostringstream out;
    ConsoleApp app(service, in, out);
    int code = app.run();
    if (exit_code) {
        *exit_code = code;
    }
    return out.str();
}

} // namespace

// ── 1. End-to-End Story (Demo Scenario) ──────────────────────────

TEST_CASE("console session: end-to-end full demo scenario") {
    LibraryService service;
    int code = -1;

    std::string script;
    // 10: Load sample data
    script += "10\n";
    // 1: List books
    script += "1\n";
    // 9 -> 1 -> 0: List members then back
    script += "9\n1\n0\n";
    // 4: Search books
    script += "4\n1\napple\n";          // by title
    script += "4\n2\nJane Doe\n";       // by author
    script += "4\n3\n9780134685991\n";  // by ISBN
    script += "4\n4\nFiction\n";        // by genre
    // 5: Borrow two books for member 1
    script += "5\n9780134685991\n1\n";
    script += "5\n9780201633610\n1\n";
    // Try forbidden borrow: already borrowed by member 1, member 2 tries
    script += "5\n9780134685991\n2\n";
    // 8: Statistics
    script += "8\n";
    // 7: Sort by title descending with insertion sort (1, 2, 2)
    script += "7\n1\n2\n2\n";
    // 6: Return book
    script += "6\n9780134685991\n";
    // 3: Remove book
    script += "3\n9780134685991\ny\n";
    // 8: Statistics again
    script += "8\n";
    // 0: Exit
    script += "0\n";

    std::string out = run_script(service, script, &code);

    CHECK(code == 0);
    CHECK(contains(out, "Sample data loaded"));
    CHECK(contains(out, "apple pie"));
    CHECK(contains(out, "Alice Smith"));
    CHECK(contains(out, "was borrowed by Alice Smith (id 1)."));
    CHECK(contains(out, "Error: book is not available"));
    CHECK(contains(out, "Catalog sorted by title (descending, insertion sort)."));
    CHECK(contains(out, "was returned by Alice Smith (id 1)."));
    CHECK(contains(out, "Book removed."));
    CHECK(contains(out, "Goodbye!"));

    // Book 9780134685991 was returned and removed
    CHECK(!service.has_book("9780134685991"));
    // Book 9780201633610 is still borrowed by member 1
    CHECK(service.get_member(1).has_borrowed("9780201633610"));

    check_consistency(service);
}

// ── 2. Fuzzing Test ──────────────────────────────────────────────

TEST_CASE("console session: randomized fuzzing") {
    const std::vector<std::string> fuzz_pool = {
        "-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
        "9780134685991", "9780201633610", "bad-isbn-123", "", "   ",
        "y", "n", "yes", "/cancel", "/CANCEL",
        "abc", "Some Random Book Title",
        "\xD9\x83\xD8\xAA\xD8\xA8",
        std::string(5000, 'x'),
        "test\tcontrol\r\x01" "data"
    };

    const std::vector<unsigned int> seeds = {12345, 67890, 99999};

    for (unsigned int seed : seeds) {
        // Fuzz on sample service
        {
            LibraryService svc = make_sample_service();
            std::mt19937 rng(seed);
            std::string input;
            for (int i = 0; i < 2000; ++i) {
                int idx = static_cast<int>(rng() % fuzz_pool.size());
                input += fuzz_pool[static_cast<std::size_t>(idx)];
                input += '\n';
            }

            int code = -1;
            std::string out = run_script(svc, input, &code);
            CHECK(code == 0);
            CHECK(!out.empty());
            check_consistency(svc);
        }

        // Fuzz on empty service
        {
            LibraryService svc;
            std::mt19937 rng(seed + 1);
            std::string input;
            for (int i = 0; i < 2000; ++i) {
                int idx = static_cast<int>(rng() % fuzz_pool.size());
                input += fuzz_pool[static_cast<std::size_t>(idx)];
                input += '\n';
            }

            int code = -1;
            std::string out = run_script(svc, input, &code);
            CHECK(code == 0);
            CHECK(!out.empty());
            check_consistency(svc);
        }
    }
}

// ── 3. All Garbage Session ───────────────────────────────────────

TEST_CASE("console session: all garbage inputs") {
    LibraryService svc = make_sample_service();
    auto before = snapshot(svc);

    std::string garbage_input;
    for (int i = 0; i < 50; ++i) {
        garbage_input += "gibberish_???_---\n";
    }

    int code = -1;
    std::string out = run_script(svc, garbage_input, &code);
    CHECK(code == 0);
    CHECK(contains(out, "Input closed."));
    CHECK(snapshot(svc) == before);
    check_consistency(svc);
}

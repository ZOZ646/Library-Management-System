#pragma once

#include "service/LibraryService.h"

#include <cstddef>

namespace titans {

struct DemoDataResult {
    std::size_t books_added{0};
    std::size_t members_added{0};
};

// Loads sample books and members into the service.
// Safe to call multiple times: skips items that already exist.
DemoDataResult load_demo_data(LibraryService& service);

} // namespace titans

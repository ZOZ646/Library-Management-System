#pragma once

#include "repository/ILibraryRepository.h"

namespace titans {

// Null Object implementation of ILibraryRepository:
// The default for a service that is not persisted (all in-memory uses and all earlier tests).
class NullRepository final : public ILibraryRepository {
public:
    NullRepository() = default;

    StoredLibrary load() override {
        return StoredLibrary{};
    }

    void add_book(const Book&) override {}
    void remove_book(const std::string&) override {}
    void add_member(const Member&) override {}
    void remove_member(int) override {}
    void record_borrow(const std::string&, int) override {}
    void record_return(const std::string&) override {}
};

} // namespace titans

#include "domain/Member.h"
#include "domain/Text.h"

#include <stdexcept>
#include <utility>

namespace titans {

namespace {

int validate_id(int id) {
    if (id <= 0) {
        throw std::invalid_argument("member id must be positive");
    }
    return id;
}

} // namespace

Member::Member(int id, std::string name)
    : id_(validate_id(id)),
      name_(text::require_non_blank(name, "name")),
      borrowed_() {}

int Member::id() const noexcept {
    return id_;
}

const std::string& Member::name() const noexcept {
    return name_;
}

void Member::set_name(std::string name) {
    std::string validated = text::require_non_blank(name, "name");
    name_ = std::move(validated);
}

std::size_t Member::borrowed_count() const noexcept {
    return borrowed_.size();
}

bool Member::can_borrow() const noexcept {
    return borrowed_.size() < kMaxBorrowed;
}

// O(k)
bool Member::has_borrowed(const std::string& isbn) const {
    if (borrowed_.find(isbn) != borrowed_.end()) {
        return true;
    }
    std::string trimmed = text::trim(isbn);
    if (trimmed != isbn) {
        return borrowed_.find(trimmed) != borrowed_.end();
    }
    return false;
}

// O(k)
void Member::add_borrowed(const std::string& isbn) {
    // (1) Validate non-blank
    std::string trimmed = text::require_non_blank(isbn, "isbn");
    // (2) Check duplicate
    if (has_borrowed(trimmed)) {
        throw std::logic_error("book already borrowed by this member");
    }
    // (3) Check limit
    if (borrowed_.size() >= kMaxBorrowed) {
        throw std::logic_error("borrow limit reached");
    }
    borrowed_.push_back(std::move(trimmed));
}

// O(k)
void Member::remove_borrowed(const std::string& isbn) {
    if (!borrowed_.remove_value(isbn)) {
        std::string trimmed = text::trim(isbn);
        if (trimmed == isbn || !borrowed_.remove_value(trimmed)) {
            throw std::logic_error("book not borrowed by this member");
        }
    }
}

const titans::Container<std::string>& Member::borrowed_isbns() const noexcept {
    return borrowed_;
}

} // namespace titans

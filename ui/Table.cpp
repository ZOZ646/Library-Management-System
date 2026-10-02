#include "ui/Table.h"
#include "ui/Utf8.h"

#include <algorithm>
#include <stdexcept>

namespace titans {

void Table::add_column(std::string header, std::size_t max_width) {
    if (!rows_.empty()) {
        throw std::logic_error("cannot add a column after rows were added");
    }
    if (max_width == 0) {
        throw std::invalid_argument("column max width must be at least 1");
    }
    columns_.push_back({std::move(header), max_width});
}

void Table::add_row(std::vector<std::string> cells) {
    if (cells.size() != columns_.size()) {
        throw std::invalid_argument("row has the wrong number of cells");
    }
    rows_.push_back(std::move(cells));
}

std::size_t Table::row_count() const noexcept {
    return rows_.size();
}

void Table::print(std::ostream& out) const {
    if (columns_.empty()) {
        return;
    }

    // Calculate width for each column
    std::vector<std::size_t> widths;
    widths.reserve(columns_.size());

    for (std::size_t i = 0; i < columns_.size(); ++i) {
        std::size_t widest = utf8::display_width(columns_[i].header);
        for (const auto& row : rows_) {
            widest = std::max(widest, utf8::display_width(row[i]));
        }
        std::size_t col_width = std::min(columns_[i].max_width, widest);
        widths.push_back(col_width);
    }

    // Helper to print a border line
    auto print_border = [&]() {
        out << '+';
        for (std::size_t w : widths) {
            out << std::string(w + 2, '-') << '+';
        }
        out << '\n';
    };

    // Helper to print a row of cells
    auto print_cells = [&](const std::vector<std::string>& cells) {
        out << '|';
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            std::string truncated = utf8::truncate_display(cells[i], widths[i]);
            std::string padded = utf8::pad_right(truncated, widths[i]);
            out << ' ' << padded << " |";
        }
        out << '\n';
    };

    // Border line
    print_border();

    // Header line
    std::vector<std::string> headers;
    headers.reserve(columns_.size());
    for (const auto& col : columns_) {
        headers.push_back(col.header);
    }
    print_cells(headers);

    // Border line
    print_border();

    // Data rows
    for (const auto& row : rows_) {
        print_cells(row);
    }

    // Final border line (even for empty rows, border, header, border, border)
    print_border();
}

} // namespace titans

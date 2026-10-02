#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

namespace titans {

class Table {
public:
    Table() = default;

    // Adds a column with a header title and maximum allowed width.
    // Throws std::logic_error if rows have already been added.
    // Throws std::invalid_argument if max_width == 0.
    void add_column(std::string header, std::size_t max_width);

    // Adds a row of cells.
    // Throws std::invalid_argument if cells.size() != number of columns.
    void add_row(std::vector<std::string> cells);

    // Returns the number of rows currently in the table.
    std::size_t row_count() const noexcept;

    // Prints the formatted table to out.
    // A table with zero columns prints nothing.
    void print(std::ostream& out) const;

private:
    struct Column {
        std::string header;
        std::size_t max_width;
    };

    std::vector<Column> columns_;
    std::vector<std::vector<std::string>> rows_;
};

} // namespace titans

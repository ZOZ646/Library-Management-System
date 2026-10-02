#pragma once

#include "service/LibraryService.h"
#include "ui/InputReader.h"
#include "core/Container.h"
#include "domain/Book.h"

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace titans {

// Menu entry using a lightweight Command pattern.
struct MenuEntry {
    std::string label;
    std::function<void()> action;
};

class ConsoleApp {
public:
    ConsoleApp(LibraryService& service, std::istream& in, std::ostream& out);

    ConsoleApp(const ConsoleApp&) = delete;
    ConsoleApp& operator=(const ConsoleApp&) = delete;
    ConsoleApp(ConsoleApp&&) = delete;
    ConsoleApp& operator=(ConsoleApp&&) = delete;

    // Runs the console application main loop. Returns 0 on exit or closed input.
    // Never throws on user input.
    int run();

private:
    LibraryService& service_;
    InputReader reader_;
    std::ostream& out_;

    std::vector<MenuEntry> main_entries_;
    std::vector<MenuEntry> member_entries_;

    // Builds the menu entries vectors with actions.
    void init_menus();

    // Runs a menu given title, entries, and exit label ("Exit" or "Back").
    void run_menu(const std::string& title,
                  const std::vector<MenuEntry>& entries,
                  const std::string& exit_label);

    // Executes a menu action, catching CancelledByUser and service exceptions.
    // Rethrows InputClosed so run() can exit cleanly.
    void execute(const std::function<void()>& action);

    // Helper to print a list of books as a formatted Table.
    void print_books(const Container<Book>& books, const std::string& empty_message);

    // Actions
    void list_books_action();
    void add_book_action();
    void remove_book_action();
    void search_books_action();
    void borrow_book_action();
    void return_book_action();
    void sort_books_action();
    void statistics_action();
    void load_sample_data_action();

    // Member actions
    void list_members_action();
    void add_member_action();
    void remove_member_action();
    void show_borrowed_action();
};

} // namespace titans

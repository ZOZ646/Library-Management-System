#include "ui/ConsoleApp.h"
#include "ui/Table.h"
#include "ui/DemoData.h"
#include "service/SortStrategy.h"
#include "service/SearchStrategy.h"
#include "domain/Book.h"
#include "domain/Member.h"

#include <limits>
#include <ostream>
#include <string>
#include <vector>

namespace titans {

ConsoleApp::ConsoleApp(LibraryService& service, std::istream& in, std::ostream& out)
    : service_(service), reader_(in, out), out_(out) {
    init_menus();
}

void ConsoleApp::init_menus() {
    main_entries_ = {
        {"List all books",     [this]() { list_books_action(); }},
        {"Add a book",          [this]() { add_book_action(); }},
        {"Remove a book",       [this]() { remove_book_action(); }},
        {"Search books",        [this]() { search_books_action(); }},
        {"Borrow a book",       [this]() { borrow_book_action(); }},
        {"Return a book",       [this]() { return_book_action(); }},
        {"Sort the catalog",    [this]() { sort_books_action(); }},
        {"Statistics",          [this]() { statistics_action(); }},
        {"Members",             [this]() { run_menu("Members", member_entries_, "Back"); }},
        {"Load sample data",    [this]() { load_sample_data_action(); }}
    };

    member_entries_ = {
        {"List members",                  [this]() { list_members_action(); }},
        {"Add a member",                  [this]() { add_member_action(); }},
        {"Remove a member",               [this]() { remove_member_action(); }},
        {"Show a member's borrowed books",[this]() { show_borrowed_action(); }}
    };
}

int ConsoleApp::run() {
    try {
        out_ << "=== Titans Library Management System ===\n";
        run_menu("Main menu", main_entries_, "Exit");
        out_ << "Goodbye!\n";
        return 0;
    } catch (const InputClosed&) {
        out_ << "\nInput closed. Goodbye.\n";
        return 0;
    }
}

void ConsoleApp::run_menu(const std::string& title,
                          const std::vector<MenuEntry>& entries,
                          const std::string& exit_label) {
    while (true) {
        out_ << '\n' << title << '\n';
        std::size_t num_width = std::to_string(entries.size()).size();
        for (std::size_t i = 0; i < entries.size(); ++i) {
            std::string num_str = std::to_string(i + 1);
            std::string pad(num_width > num_str.size() ? num_width - num_str.size() : 0, ' ');
            out_ << pad << num_str << " " << entries[i].label << '\n';
        }
        std::string zero_pad(num_width > 1 ? num_width - 1 : 0, ' ');
        out_ << zero_pad << "0 " << exit_label << '\n';

        int choice = reader_.read_int("Choose an option: ", 0, static_cast<int>(entries.size()), false);
        if (choice == 0) {
            return;
        }
        execute(entries[static_cast<std::size_t>(choice - 1)].action);
    }
}

void ConsoleApp::execute(const std::function<void()>& action) {
    try {
        action();
    } catch (const InputClosed&) {
        throw;
    } catch (const CancelledByUser&) {
        out_ << "Cancelled.\n";
    } catch (const std::exception& e) {
        out_ << "Error: " << e.what() << '\n';
    }
}

void ConsoleApp::print_books(const Container<Book>& books, const std::string& empty_message) {
    if (books.empty()) {
        out_ << empty_message << '\n';
        return;
    }
    Table t;
    t.add_column("#", 4);
    t.add_column("Title", 28);
    t.add_column("Author", 20);
    t.add_column("ISBN", 13);
    t.add_column("Genre", 14);
    t.add_column("Status", 9);

    std::size_t idx = 1;
    for (const auto& b : books) {
        t.add_row({
            std::to_string(idx++),
            b.title(),
            b.author(),
            b.isbn(),
            b.genre(),
            b.is_available() ? "Available" : "Borrowed"
        });
    }
    t.print(out_);
    out_ << books.size() << " book(s).\n";
}

void ConsoleApp::list_books_action() {
    print_books(service_.books(), "The catalog is empty.");
}

void ConsoleApp::add_book_action() {
    std::string title = reader_.read_text("Title: ");
    std::string author = reader_.read_text("Author: ");
    std::string isbn = reader_.read_isbn("ISBN: ");
    std::string genre = reader_.read_text("Genre: ");

    service_.add_book(Book(title, author, isbn, genre));
    out_ << "Book added.\n";
}

void ConsoleApp::remove_book_action() {
    std::string isbn = reader_.read_isbn("ISBN: ");
    const Book& book = service_.get_book(isbn);
    std::string title = book.title();

    if (reader_.confirm("Remove '" + title + "'? (y/n): ")) {
        service_.remove_book(isbn);
        out_ << "Book removed.\n";
    } else {
        out_ << "Cancelled.\n";
    }
}

void ConsoleApp::search_books_action() {
    out_ << "1. Title  2. Author  3. ISBN  4. Genre\n";
    int choice = reader_.read_int("Search by: ", 1, 4, true);

    std::string key;
    if (choice == 1) key = "title";
    else if (choice == 2) key = "author";
    else if (choice == 3) key = "isbn";
    else key = "genre";

    auto strategy = SearchStrategyFactory::create(key);
    std::string query = reader_.read_text("Query: ", true);
    auto results = service_.search(*strategy, query);

    if (results.empty()) {
        out_ << "No books found.\n";
    } else {
        print_books(results, "No books found.");
    }
}

void ConsoleApp::borrow_book_action() {
    std::string isbn = reader_.read_isbn("ISBN: ");
    int member_id = reader_.read_int("Member ID: ", 1, std::numeric_limits<int>::max(), true);

    service_.borrow_book(isbn, member_id);
    const Book& book = service_.get_book(isbn);
    const Member& member = service_.get_member(member_id);
    out_ << "'" << book.title() << "' was borrowed by " << member.name() << " (id " << member_id << ").\n";
}

void ConsoleApp::return_book_action() {
    std::string isbn = reader_.read_isbn("ISBN: ");
    const Book& book = service_.get_book(isbn);
    std::string title = book.title();

    int id = service_.return_book(isbn);
    const Member& member = service_.get_member(id);
    out_ << "'" << title << "' was returned by " << member.name() << " (id " << id << ").\n";
}

void ConsoleApp::sort_books_action() {
    out_ << "1. Title  2. Author  3. Genre\n";
    int crit = reader_.read_int("Sort by: ", 1, 3, true);

    out_ << "1. Ascending  2. Descending\n";
    int order_choice = reader_.read_int("Order: ", 1, 2, true);

    out_ << "1. Merge sort  2. Insertion sort\n";
    int algo_choice = reader_.read_int("Algorithm: ", 1, 2, true);

    std::string key = (crit == 1 ? "title" : (crit == 2 ? "author" : "genre"));
    auto strategy = SortStrategyFactory::create(key);
    SortOrder order = (order_choice == 1 ? SortOrder::Ascending : SortOrder::Descending);
    SortAlgorithm algo = (algo_choice == 1 ? SortAlgorithm::Merge : SortAlgorithm::Insertion);

    service_.sort_books(*strategy, order, algo);
    out_ << "Catalog sorted by " << strategy->name() << " ("
         << (order == SortOrder::Ascending ? "ascending" : "descending") << ", "
         << (algo == SortAlgorithm::Merge ? "merge" : "insertion") << " sort).\n";

    print_books(service_.books(), "The catalog is empty.");
}

void ConsoleApp::statistics_action() {
    out_ << "--- Library statistics ---\n";
    out_ << "Total books     : " << service_.total_books() << '\n';
    out_ << "Available books : " << service_.available_books() << '\n';
    out_ << "Borrowed books  : " << service_.borrowed_books() << '\n';
    out_ << "Members         : " << service_.member_count() << '\n';
}

void ConsoleApp::load_sample_data_action() {
    DemoDataResult res = load_demo_data(service_);
    out_ << "Sample data loaded: " << res.books_added << " book(s) and "
         << res.members_added << " member(s) added.\n";
}

void ConsoleApp::list_members_action() {
    if (service_.members().empty()) {
        out_ << "There are no members.\n";
        return;
    }
    Table t;
    t.add_column("ID", 6);
    t.add_column("Name", 24);
    t.add_column("Borrowed", 10);

    for (const auto& m : service_.members()) {
        std::string borrowed_str = std::to_string(m.borrowed_count()) + "/" + std::to_string(Member::kMaxBorrowed);
        t.add_row({std::to_string(m.id()), m.name(), borrowed_str});
    }
    t.print(out_);
    out_ << service_.member_count() << " member(s).\n";
}

void ConsoleApp::add_member_action() {
    int id = reader_.read_int("Member ID: ", 1, std::numeric_limits<int>::max(), true);
    std::string name = reader_.read_text("Name: ", true);

    service_.add_member(id, name);
    out_ << "Member added.\n";
}

void ConsoleApp::remove_member_action() {
    int id = reader_.read_int("Member ID: ", 1, std::numeric_limits<int>::max(), true);
    const Member& m = service_.get_member(id);
    std::string name = m.name();

    if (reader_.confirm("Remove member '" + name + "' (id " + std::to_string(id) + ")? (y/n): ")) {
        service_.remove_member(id);
        out_ << "Member removed.\n";
    } else {
        out_ << "Cancelled.\n";
    }
}

void ConsoleApp::show_borrowed_action() {
    int id = reader_.read_int("Member ID: ", 1, std::numeric_limits<int>::max(), true);
    const Member& m = service_.get_member(id);

    if (m.borrowed_count() == 0) {
        out_ << m.name() << " has no borrowed books.\n";
        return;
    }

    Container<Book> borrowed_books;
    for (const auto& isbn : m.borrowed_isbns()) {
        borrowed_books.push_back(service_.get_book(isbn));
    }
    print_books(borrowed_books, m.name() + " has no borrowed books.");
}

} // namespace titans

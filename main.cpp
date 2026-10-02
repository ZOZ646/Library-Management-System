#include <iostream>
#include <memory>
#include <string>

#include "repository/SqliteRepository.h"
#include "service/LibraryService.h"
#include "ui/ConsoleApp.h"

namespace {

enum class ParseResult {
    Ok,
    Help,
    Error
};

ParseResult parse_args(int argc, char* argv[], std::string& db_path, std::string& error_msg) {
    db_path = "library.db";
    bool db_specified = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            return ParseResult::Help;
        } else if (arg == "--db") {
            if (db_specified) {
                error_msg = "Error: --db option specified more than once";
                return ParseResult::Error;
            }
            if (i + 1 >= argc) {
                error_msg = "Error: missing argument for --db";
                return ParseResult::Error;
            }
            db_path = argv[++i];
            db_specified = true;
        } else {
            error_msg = "Error: unknown argument '" + arg + "'";
            return ParseResult::Error;
        }
    }
    return ParseResult::Ok;
}

void print_usage(std::ostream& out) {
    out << "Usage: titans [--db <file>]\n"
        << "  --db <file>   Path to SQLite database (default: library.db, ':memory:' for transient)\n"
        << "  -h, --help    Show this help message\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string db_path;
    std::string error_msg;
    ParseResult result = parse_args(argc, argv, db_path, error_msg);

    if (result == ParseResult::Help) {
        print_usage(std::cout);
        return 0;
    }
    if (result == ParseResult::Error) {
        std::cerr << error_msg << '\n';
        print_usage(std::cerr);
        return 2;
    }

    try {
        auto repository = std::make_shared<titans::SqliteRepository>(db_path);
        auto service = titans::LibraryService::open(repository);
        std::cout << "Database: " << db_path << '\n';
        titans::ConsoleApp app(service, std::cin, std::cout);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
}

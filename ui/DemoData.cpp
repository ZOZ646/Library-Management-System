#include "ui/DemoData.h"
#include "domain/Book.h"

#include <string>
#include <vector>

namespace titans {

DemoDataResult load_demo_data(LibraryService& service) {
    DemoDataResult result{0, 0};

    // Arabic strings using hex escapes, split to avoid hex-digit merging:
    // "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8" = "كتاب" (Book)
    // "\xD8\xA7\xD9\x84\xD8\xA3\xD9\x88\xD9\x84" = "الأول" (First)
    // "\xD9\x86\xD8\xAC\xD9\x8A\xD8\xA8" = "نجيب" (Naguib)
    // "\xD9\x85\xD8\xAD\xD9\x81\xD9\x88\xD8\xB8" = "محفوظ" (Mahfouz)
    // "\xD8\xA7\xD9\x84\xD8\xA3\xD9\x8A\xD8\xA7\xD9\x85" = "الأيام" (The Days)
    // "\xD8\xB7\xD9\x87" = "طه" (Taha)
    // "\xD8\xAD\xD8\xB3\xD9\x8A\xD9\x86" = "حسين" (Hussein)

    const std::vector<Book> books = {
        Book("apple pie", "Jane Doe", "9780134685991", "Fiction"),
        Book("Big apple pie", "Jane Doe", "9780201633610", "Fiction"),
        Book("Banana Split", "Arthur Conan Doyle", "9780141439600", "Fiction"),
        Book("A Brief History of Time", "Stephen Hawking", "9780553380163", "Science"),
        Book("The Selfish Gene", "Richard Dawkins", "9780199291151", "Science"),
        Book("Sapiens: A Brief History", "Yuval Noah Harari", "9780062316097", "History"),
        Book("Guns, Germs, and Steel", "Jared Diamond", "9780393317558", "History"),
        Book("The Art of Computer Programming", "Donald E. Knuth", "0201896834", "Technology"),
        Book("Introduction to Algorithms", "Thomas H. Cormen", "0262033844", "Technology"),
        Book("Clean Code", "Robert C. Martin", "9780132350884", "Non-Fiction"),
        Book("The Pragmatic Programmer", "Andrew Hunt", "9780201616224", "Non-Fiction"),
        Book("1984", "George Orwell", "9780451524935", "Fiction"),
        Book("\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8" " " "\xD8\xA7\xD9\x84\xD8\xA3\xD9\x88\xD9\x84",
             "\xD9\x86\xD8\xAC\xD9\x8A\xD8\xA8" " " "\xD9\x85\xD8\xAD\xD9\x81\xD9\x88\xD8\xB8",
             "9789770912345", "Fiction"),
        Book("\xD8\xA7\xD9\x84\xD8\xA3\xD9\x8A\xD8\xA7\xD9\x85",
             "\xD8\xB7\xD9\x87" " " "\xD8\xAD\xD8\xB3\xD9\x8A\xD9\x86",
             "9789770956789", "Non-Fiction")
    };

    struct MemberDef {
        int id;
        std::string name;
    };

    const std::vector<MemberDef> members = {
        {1, "Alice Smith"},
        {2, "Bob Jones"},
        {3, "Charlie Brown"},
        {4, "Diana Prince"},
        {5, "Edward Norton"}
    };

    for (const auto& b : books) {
        if (!service.has_book(b.isbn())) {
            service.add_book(b);
            ++result.books_added;
        }
    }

    for (const auto& m : members) {
        if (!service.has_member(m.id)) {
            service.add_member(m.id, m.name);
            ++result.members_added;
        }
    }

    return result;
}

} // namespace titans

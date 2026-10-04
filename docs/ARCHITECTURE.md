# Architecture and System Design

This document details the software architecture, component relationships, class structures, and runtime workflows of the Titans Library Management System.

## Architectural Layers

The following diagram shows the layered design and unidirectional dependencies between UI, service facade, domain models, custom container, and persistence abstractions.

```mermaid
flowchart TD
    UI[UI Layer\nConsoleApp, InputReader, Table] --> Service[Service Layer\nLibraryService Facade]
    Service --> Domain[Domain Layer\nBook, Member]
    Service --> Strategy[Strategy Layer\nSortStrategy, SearchStrategy]
    Service --> Core[Core Layer\ntitans::Container~T~]
    Service --> RepoIface[Repository Interface\nILibraryRepository]
    RepoIface --> RepoSqlite[Persistence Layer\nSqliteRepository]
    RepoIface --> RepoNull[Persistence Layer\nNullRepository]
    Domain --> Core
```

---

## Core Container Architecture

The following diagram shows `titans::Container~T~`, its internal node structure, and its bidirectional iterator hierarchy.

```mermaid
classDiagram
    class Node~T~ {
        +T data
        +Node~T~* prev
        +Node~T~* next
    }
    class Container~T~ {
        -Node~T~* head_
        -Node~T~* tail_
        -size_t size_
        +size() size_t
        +empty() bool
        +push_back(const T&) void
        +push_front(const T&) void
        +insert(size_t, const T&) void
        +remove_at(size_t) void
        +remove_value(const T&) bool
        +clear() void
        +begin() Iterator
        +end() Iterator
        +find(const T&) Iterator
        +sort() void
        +insertion_sort() void
    }
    class BasicIterator~IsConst~ {
        -Node~T~* current_
        -Container~T~* owner_
        +operator*() reference
        +operator++() BasicIterator&
        +operator--() BasicIterator&
        +operator==(BasicIterator) bool
        +operator!=(BasicIterator) bool
    }
    class Iterator
    class ConstIterator

    Container~T~ *-- Node~T~ : manages
    Container~T~ ..> BasicIterator~IsConst~ : creates
    BasicIterator~IsConst~ <|-- Iterator : specializes
    BasicIterator~IsConst~ <|-- ConstIterator : specializes
```

---

## Domain and Service Layer

The following diagram illustrates the domain models (`Book`, `Member`), the central `LibraryService` facade, and custom domain exceptions.

```mermaid
classDiagram
    class Book {
        -string title_
        -string author_
        -string isbn_
        -string genre_
        -bool available_
        +title() string
        +author() string
        +isbn() string
        +genre() string
        +is_available() bool
        +mark_borrowed() void
        +mark_returned() void
        +normalize_isbn(string)$ string
        +is_valid_isbn(string)$ bool
    }
    class Member {
        -int id_
        -string name_
        -Container~string~ borrowed_
        +id() int
        +name() string
        +borrowed_count() size_t
        +can_borrow() bool
        +has_borrowed(string) bool
        +add_borrowed(string) void
        +remove_borrowed(string) void
        +borrowed_isbns() Container~string~
    }
    class LibraryService {
        -Container~Book~ books_
        -Container~Member~ members_
        -shared_ptr~ILibraryRepository~ repository_
        +open(shared_ptr~ILibraryRepository~)$ LibraryService
        +add_book(Book) void
        +remove_book(string) void
        +get_book(string) Book
        +add_member(int, string) void
        +remove_member(int) void
        +get_member(int) Member
        +borrow_book(string, int) void
        +return_book(string) int
        +books() Container~Book~
        +members() Container~Member~
        +sort_books(ISortStrategy, SortOrder, SortAlgorithm) void
        +search(ISearchStrategy, string) Container~Book~
    }
    class NotFoundError
    class DuplicateError

    LibraryService *-- Book : contains catalog
    LibraryService *-- Member : contains members
    LibraryService ..> NotFoundError : throws
    LibraryService ..> DuplicateError : throws
```

---

## Strategy Hierarchy and Factories

The following diagram shows the extensible strategy hierarchies and corresponding factory objects for sorting and searching books.

```mermaid
classDiagram
    class ISortStrategy {
        <<interface>>
        +name()* const char*
        +less(const Book&, const Book&)* bool
    }
    class TitleSort
    class AuthorSort
    class GenreSort
    class SortStrategyFactory {
        +create(string)$ unique_ptr~ISortStrategy~
    }

    class ISearchStrategy {
        <<interface>>
        +name()* const char*
        +matches(const Book&, string)* bool
    }
    class TitleSearch
    class AuthorSearch
    class IsbnSearch
    class GenreSearch
    class SearchStrategyFactory {
        +create(string)$ unique_ptr~ISearchStrategy~
    }

    ISortStrategy <|-- TitleSort
    ISortStrategy <|-- AuthorSort
    ISortStrategy <|-- GenreSort
    SortStrategyFactory ..> ISortStrategy : instantiates

    ISearchStrategy <|-- TitleSearch
    ISearchStrategy <|-- AuthorSearch
    ISearchStrategy <|-- IsbnSearch
    ISearchStrategy <|-- GenreSearch
    SearchStrategyFactory ..> ISearchStrategy : instantiates
```

---

## Repository and UI Architecture

The following diagram depicts the repository persistence abstraction and the console user interface subsystem.

```mermaid
classDiagram
    class ILibraryRepository {
        <<interface>>
        +load()* StoredLibrary
        +add_book(const Book&)* void
        +remove_book(const string&)* void
        +add_member(const Member&)* void
        +remove_member(int)* void
        +record_borrow(const string&, int)* void
        +record_return(const string&)* void
    }
    class NullRepository
    class SqliteRepository {
        -sqlite3* db_
        -void init_schema()
        -void prepare_statements()
    }
    class ConsoleApp {
        -LibraryService& service_
        -InputReader reader_
        -ostream& out_
        +run() int
    }
    class InputReader {
        -istream& in_
        -ostream& out_
        +read_int(string, int, int, bool) int
        +read_text(string, bool) string
        +read_isbn(string, bool) string
        +confirm(string) bool
    }
    class Table {
        +add_column(string, size_t) void
        +add_row(vector~string~) void
        +print(ostream&) void
    }

    ILibraryRepository <|-- NullRepository
    ILibraryRepository <|-- SqliteRepository
    ConsoleApp *-- InputReader
    ConsoleApp ..> Table : formats output
    ConsoleApp --> ILibraryRepository : interacts via Service
```

---

## Sequence: Borrow Book Workflow

The following sequence diagram traces `borrow_book` through validation, persistent storage write-ahead, memory mutation, and exception handling.

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant UI as ConsoleApp
    participant Service as LibraryService
    participant Domain as Book / Member
    participant Repo as ILibraryRepository

    User->>UI: Enter ISBN and Member ID
    UI->>Service: borrow_book(isbn, member_id)
    Service->>Domain: Validate book availability and member limit
    alt Validation Failure
        Domain-->>Service: throws std::logic_error
        Service-->>UI: throws exception
        UI->>User: Display error message
    else Validation Successful
        Service->>Repo: record_borrow(isbn, member_id)
        alt Repository Failure (Disk / SQLite Error)
            Repo-->>Service: throws DatabaseError
            Note over Service,Domain: In-memory state remains untouched (Strong Exception Guarantee)
            Service-->>UI: propagates DatabaseError
            UI->>User: Display "Error: <message>"
        else Repository Write Succeeded
            Repo-->>Service: return success
            Service->>Domain: book.mark_borrowed()
            Service->>Domain: member.add_borrowed(isbn)
            Service-->>UI: return void
            UI->>User: Display "'<title>' was borrowed by <name> (id <id>)."
        end
    end
```

---

## Sequence: Application Start-up and Replay

The following sequence diagram illustrates application startup, opening an existing SQLite database, and rebuilding domain state via the public API.

```mermaid
sequenceDiagram
    autonumber
    participant Main as main()
    participant Repo as SqliteRepository
    participant Service as LibraryService
    participant UI as ConsoleApp

    Main->>Repo: make_shared<SqliteRepository>(db_path)
    Repo->>Repo: sqlite3_open_v2(), init_schema(), prepare_statements()
    Main->>Service: LibraryService::open(repo)
    Service->>Repo: repo->load()
    Repo-->>Service: StoredLibrary (books, members, open_loans)
    loop For each Book in StoredLibrary
        Service->>Service: add_book(b)
    end
    loop For each Member in StoredLibrary
        Service->>Service: add_member(m.id(), m.name())
    end
    loop For each StoredLoan in StoredLibrary
        Service->>Service: borrow_book(loan.isbn, loan.member_id)
    end
    Service-->>Main: return initialized LibraryService
    Main->>UI: ConsoleApp(service, cin, cout)
    Main->>UI: app.run()
```

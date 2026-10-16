# Titans — Library Management System
## Complete Technical Documentation & Defense Guide

> **What this document is.** A single, exhaustive reference that explains every part of the
> project: what it does, how it is built, why each decision was made, and how to explain it
> out loud. It is written so you can read one section the night before, and open any other
> section when a reviewer points at a line of code.
>
> **Project:** Titans Library Management System
> **Author:** Mazen Elsayed — AI & Data Science, Zagazig National University
> **Language:** C++17 · **Build:** CMake ≥ 3.16 · **Storage:** SQLite 3.53.4 (bundled) · **Tests:** doctest 2.4.11
> **Status:** 189 test cases / 209,475 assertions passing · tagged `v1.0.2`

---

## Table of contents

1. [Elevator pitch and talking points](#1-elevator-pitch-and-talking-points)
2. [Project at a glance (the numbers)](#2-project-at-a-glance-the-numbers)
3. [The problem and what this system solves](#3-the-problem-and-what-this-system-solves)
4. [What the application does (features)](#4-what-the-application-does-features)
5. [Architecture: the five layers](#5-architecture-the-five-layers)
6. [End-to-end data flow](#6-end-to-end-data-flow)
7. [File-by-file repository reference](#7-file-by-file-repository-reference)
8. [Core layer in depth: `titans::Container<T>`](#8-core-layer-in-depth-titanscontainert)
9. [Domain layer in depth: `Book`, `Member`, `Text`](#9-domain-layer-in-depth-book-member-text)
10. [Service layer in depth: `LibraryService`](#10-service-layer-in-depth-libraryservice)
11. [Strategies: search and sort](#11-strategies-search-and-sort)
12. [Repository layer: persistence](#12-repository-layer-persistence)
13. [The SQLite database schema](#13-the-sqlite-database-schema)
14. [UI layer: `main`, `ConsoleApp`, `InputReader`, `Table`, `DemoData`](#14-ui-layer-main-consoleapp-inputreader-table-demodata)
15. [Build system, options and targets](#15-build-system-options-and-targets)
16. [Testing and reliability](#16-testing-and-reliability)
17. [Continuous integration](#17-continuous-integration)
18. [Performance and benchmarks](#18-performance-and-benchmarks)
19. [Design patterns used, and why](#19-design-patterns-used-and-why)
20. [C++ techniques applied](#20-c-techniques-applied)
21. [Complexity cheat-sheet](#21-complexity-cheat-sheet)
22. [Design decisions (ADR summary)](#22-design-decisions-adr-summary)
23. [Limitations, known issues and documentation drift](#23-limitations-known-issues-and-documentation-drift)
24. [Presentation playbook](#24-presentation-playbook)
25. [Reviewer Q&A — rehearsed answers](#25-reviewer-qa--rehearsed-answers)
26. [Glossary](#26-glossary)

---

## 1. Elevator pitch and talking points

### 30-second pitch
> "Titans is a library management system I wrote from scratch in C++17. Instead of leaning on
> the standard library, it is built on my own generic doubly linked list, `Container<T>`, which
> provides bidirectional iterators and a stable, node-relinking merge sort. On top of that sits
> a layered application: a console UI, a service facade that enforces the business rules, domain
> entities that can never exist in an invalid state, and a SQLite repository that persists
> everything behind an interface. It is verified by 189 automated tests — over two hundred
> thousand assertions — and it is clean under AddressSanitizer and UndefinedBehaviorSanitizer."

### The five things to make sure you say
1. **It is genuinely layered** — UI, service, domain, core, repository — with one-way dependencies.
2. **The data structure is mine** — a real doubly linked list with iterators and two sorting algorithms.
3. **Consistency is guaranteed by design** — four explicit invariants and the strong exception guarantee.
4. **It is tested and sanitized** — 189 cases, 209,475 assertions, ASan + UBSan clean.
5. **It is extensible** — Strategy, Repository, Facade, Null Object patterns mean new behavior is added, not rewritten.

### Phrasings to avoid
- Do **not** call it "a CRUD app." It is a layered system with a custom container and explicit system invariants.
- Do **not** say "I used `std::list`." The point is that the container is hand-written (`core/Container.h`).
- Do **not** oversell concurrency or security. The README lists those as deliberate non-goals (see §23).

---

## 2. Project at a glance (the numbers)

| Fact | Value |
|---|---|
| Language standard | C++17 (`CMAKE_CXX_STANDARD 17`, required) |
| Source layout | 5 layers + `main.cpp` + `bench/` + `tests/` |
| Custom container | `titans::Container<T>` (doubly linked list), ~800 lines |
| Test suites | 18 files under `tests/` |
| Test cases / assertions | **189 / 209,475**, 0 failures |
| Test framework | doctest 2.4.11 (bundled single header, no download) |
| Database | SQLite 3.53.4 amalgamation (bundled, compiled in) |
| Sanitizers | ASan + UBSan default ON for GCC/Clang on Linux/macOS, OFF on Windows |
| Design patterns | Iterator, Strategy, Factory, Facade, Repository, Null Object, Command |
| Documentation | README, ARCHITECTURE, DESIGN_DECISIONS, BENCHMARKS, DEMO_SCRIPT, this file |
| Current tag | `v1.0.2` |

---

## 3. The problem and what this system solves

**Why library management systems exist.** A library is a system of records that must stay
consistent: which books exist, who they belong to, who holds them right now, and what has
happened to them over time. As the collection grows, doing this by hand stops working.

**What manual handling gets wrong.**
- Records live in ledgers or spreadsheets that get duplicated, lost, or edited inconsistently.
- Borrowing state drifts: two members appear to hold one copy, or a returned book is never marked returned.
- Simple questions — *what is available? who holds this book?* — take minutes instead of milliseconds.
- There is no reliable history and no single source of truth.

**What this system provides.**
- One consistent catalog of books and one registry of members.
- Enforced borrowing rules (uniqueness, availability, a five-book limit).
- Fast, flexible search and sort over the whole catalog.
- Durable storage (SQLite) so state survives restarts, plus a pure in-memory mode for tests/demos.
- A clean, layered design that stays maintainable as features are added.

**One-sentence framing for the defense:** *"I built the smallest complete system that still
honours the hard parts of the problem — consistency, a custom data structure, testability and
persistence — without hiding them behind a framework."*

---

## 4. What the application does (features)

| Feature | Where it is implemented |
|---|---|
| Add a book | `LibraryService::add_book` → `ConsoleApp::add_book_action` |
| Remove a book | `LibraryService::remove_book` → `ConsoleApp::remove_book_action` |
| List the catalog | `LibraryService::books` → `ConsoleApp::list_books_action` |
| Search by title / author / ISBN / genre | `TitleSearch` / `AuthorSearch` / `IsbnSearch` / `GenreSearch` |
| Sort by title / author / genre (asc/desc, merge/insertion) | `TitleSort` / `AuthorSort` / `GenreSort` + `Container::sort` / `insertion_sort` |
| Borrow a book | `LibraryService::borrow_book` |
| Return a book | `LibraryService::return_book` (returns the holder's id) |
| Add / remove / list members | `add_member`, `remove_member`, `members` |
| Show a member's borrowed books | `ConsoleApp::show_borrowed_action` |
| Statistics (total / available / borrowed / members) | `total_books`, `available_books`, `borrowed_books`, `member_count` |
| Load sample data (offline demo) | `load_demo_data` in `ui/DemoData.cpp` |

At any prompt, typing `/cancel` aborts the current operation without changing data.
End-of-input (Ctrl+D / Ctrl+Z) exits cleanly with code 0.

---

## 5. Architecture: the five layers

The system has **five layers** with strictly one-way dependencies. A layer only ever knows
about the layer(s) beneath it.

```
┌──────────────────────────────────────────────────────────────┐
│ UI layer          ConsoleApp · InputReader · Table · DemoData │  user interaction only
├──────────────────────────────────────────────────────────────┤
│ Service layer     LibraryService (Facade)                     │  business logic + invariants
│                   SearchStrategy · SortStrategy (Strategy)     │
├──────────────────────────────────────────────────────────────┤
│ Domain layer      Book · Member · Text                        │  entities + their own rules
├──────────────────────────────────────────────────────────────┤
│ Core layer        titans::Container<T>                         │  reusable data structure
├──────────────────────────────────────────────────────────────┤
│ Repository layer  ILibraryRepository → SqliteRepository /      │  persistence
│                   NullRepository                               │
└──────────────────────────────────────────────────────────────┘
```

### What each layer is responsible for

| Layer | Responsibility | Must NOT do |
|---|---|---|
| **UI** (`ui/`, `main.cpp`) | Menus, reading/validating input, formatting output tables | Contain business rules or touch the database |
| **Service** (`service/`) | The business rules; the single entry point; maintain invariants | Know about console or SQL |
| **Domain** (`domain/`) | Entities that validate themselves and hold their own rules | Know about storage or UI |
| **Core** (`core/`) | The generic data structure used everywhere | Know anything about books or members |
| **Repository** (`repository/`) | Persist and load state behind an interface | Contain business rules |

### Why separation of concerns was used
- **Change isolation:** replacing SQLite with another store touches only the repository layer.
- **Testability:** each layer can be tested alone; the UI is tested with in-memory streams, the service with a `NullRepository`.
- **Readability:** a reader can understand the business rules without reading SQL, and the SQL without reading menus.
- **Dependency inversion:** the service depends on the *interface* `ILibraryRepository`, not on SQLite.

> **Explain it like this:** "The UI is the mouth and ears, the service is the brain, the domain
> is the concepts, the container is the memory, and the repository is the filing cabinet. The
> brain never talks to the filing cabinet directly in a way that matters — it talks to the
> *idea* of a filing cabinet, which is the interface."

---

## 6. End-to-end data flow

### A write (borrow) — the important one to be able to draw
```
User types ISBN + member id
        │
        ▼
ConsoleApp (UI)  ── reads & validates input via InputReader (ISBN format, integer range)
        │  calls borrow_book(isbn, member_id)
        ▼
LibraryService (Service)  ── normalizes the ISBN, finds the member and the book,
        │                    checks availability and the borrow limit   [validation]
        │  repository_->record_borrow(isbn, member_id)                 [write-ahead: DB first]
        ▼
SqliteRepository (Repository) ── INSERT INTO loans (...)              [durable state]
        │  (if this throws, memory is untouched → nothing changes)
        ▼
LibraryService  ── member.add_borrowed(isbn); book.mark_borrowed()   [memory last]
        │
        ▼
ConsoleApp  ── prints "'1984' was borrowed by Alice Smith (id 1)."
```

### A read (search) — never touches storage
```
ConsoleApp → LibraryService::search(strategy, query)
          → iterates the in-memory Container<Book>, applies strategy.matches()
          → returns a new Container<Book> of matches (copies)
          → ConsoleApp::print_books() formats a table
```

The full order-of-operations rule is: **validate → write to disk → change memory.** That single
ordering is what gives the system its strong exception guarantee (§10.5).

---

## 7. File-by-file repository reference

```
.
├── CMakeLists.txt                  Build configuration; sanitizer + benchmark options
├── main.cpp                        Entry point, CLI parsing, wiring, exit codes
├── README.md                       User-facing overview, complexity tables, requirements coverage
├── core/
│   └── Container.h                 Custom doubly linked list + bidirectional iterators + sorts
├── domain/
│   ├── Book.h / Book.cpp           Book entity: validation, ISBN normalization, identity
│   ├── Member.h / Member.cpp       Member entity: id, name, borrowed list, 5-book limit
│   └── Text.h                      Header-only string helpers (trim, ASCII case folding)
├── service/
│   ├── LibraryService.h / .cpp     Facade: catalog + members + lending rules + invariants
│   ├── LibraryErrors.h             NotFoundError, DuplicateError
│   ├── SearchStrategy.h / .cpp     ISearchStrategy + 4 searches + factory
│   └── SortStrategy.h / .cpp       ISortStrategy + 3 sorts + factory
├── repository/
│   ├── ILibraryRepository.h        Abstract storage interface + StoredLibrary/StoredLoan
│   ├── RepositoryErrors.h          DatabaseError
│   ├── NullRepository.h            No-op (in-memory) implementation
│   └── SqliteRepository.h / .cpp   SQLite write-ahead implementation (schema, statements)
├── ui/
│   ├── ConsoleApp.h / .cpp         Interactive menu driver (Command-style dispatch)
│   ├── InputReader.h / .cpp        Validated input, /cancel, end-of-input handling
│   ├── Table.h / .cpp              ASCII table formatter with UTF-8-aware widths
│   ├── Utf8.h                      UTF-8 code-point width/truncate/pad helpers
│   └── DemoData.h / .cpp           Idempotent sample dataset (14 books, 5 members)
├── bench/
│   └── bench_container.cpp         Container<int> vs std::list<int>, median of 7 runs
├── docs/
│   ├── ARCHITECTURE.md             Diagrams: layers, classes, sequences
│   ├── DESIGN_DECISIONS.md         ADR-01..ADR-11 + reviewer Q&A
│   ├── BENCHMARKS.md               Measured performance and interpretation
│   ├── DEMO_SCRIPT.md              Timed 3-minute and 60-second demo scripts
│   └── DETAILED_DOCUMENTATION.md   This file
├── tests/                          18 doctest suites (see §16)
└── third_party/
    ├── sqlite/                     SQLite 3.53.4 amalgamation (sqlite3.c/.h)
    └── doctest/doctest/doctest.h   doctest 2.4.11 single header
```

**Dependency direction (who includes whom):**

```
main.cpp ──▶ ui/ConsoleApp ──▶ service/LibraryService ──▶ domain/{Book,Member}
                   │                    │                     │
                   │                    ├─▶ service/{SearchStrategy,SortStrategy}
                   │                    ├─▶ repository/ILibraryRepository ──▶ repository/SqliteRepository
                   │                    └─▶ core/Container.h ◀── domain uses it too
                   └─▶ ui/{InputReader,Table,DemoData}
```

`domain/Member.h` includes `core/Container.h` (a member *has-a* container of borrowed ISBNs).
Nothing in `core/` or `domain/` includes anything from `service/`, `repository/` or `ui/`.

---

## 8. Core layer in depth: `titans::Container<T>`

File: `core/Container.h` (a single header, ~808 lines). This is the foundation of the whole project.

### 8.1 What it is
A **generic doubly linked list**. Each element lives in a heap-allocated `Node` holding the value
and `prev`/`next` pointers. The container caches `head_`, `tail_`, and `size_`.

```cpp
template <typename T>
class Container {
    struct Node {
        T data;
        Node* prev{nullptr};
        Node* next{nullptr};
        template <typename... Args>
        explicit Node(Args&&... args) : data(std::forward<Args>(args)...) {}
    };
    Node* head_{nullptr};
    Node* tail_{nullptr};
    std::size_t size_{0};
    ...
};
```

The node uses a **perfect-forwarding constructor**, so elements are constructed in place — no
extra copies when a temporary is pushed.

### 8.2 Why a doubly linked list (and not `std::vector` or `std::list`)
- **Requirements:** head/tail/indexed insertion, bidirectional traversal, in-place removal, and a *custom* container — the project forbids delegating the core structure to the STL.
- **Two links** give O(1) insertion/removal at both ends and O(1) backward traversal — necessary for `--end()` and reverse iterators.
- **Stable node references:** because nodes are never relocated, a pointer/reference/iterator to an element stays valid when *other* elements are inserted or removed. This is what makes it safe for `LibraryService` to hand out `const Book&` references that survive sorting.

### 8.3 Public API (with complexity)

| Member | Complexity | Notes |
|---|---|---|
| `Container()` | O(1) | empty |
| `Container(const Container&)` | O(n) | deep copy; `clear()` on throw |
| `Container(Container&&)` | O(1) | steals pointers, `noexcept` |
| `operator=(const&)` / `operator=(&&)` | O(n) / O(1) | copy-and-swap; move clears first |
| `~Container()` | O(n) | deletes every node |
| `size()`, `empty()` | O(1) | |
| `push_back(const T&)` / `(T&&)` | O(1) | |
| `push_front(const T&)` / `(T&&)` | O(1) | |
| `insert(pos, value)` | O(n) | throws `std::out_of_range` if `pos > size()` |
| `remove_at(pos)` | O(n) | throws `std::out_of_range` if `pos >= size()` |
| `remove_value(value)` | O(n) | removes **first** equal element; returns `bool` |
| `clear()` | O(n) | `noexcept` |
| `front()` / `back()` | O(1) | throw `std::out_of_range` if empty |
| `at(pos)` | O(n) | throws `std::out_of_range` if out of range |
| `swap(other)` | O(1) | `noexcept` |
| `begin/end/cbegin/cend` | O(1) | forward iterators |
| `rbegin/rend/crbegin/crend` | O(1) | reverse iterators |
| `next(it, n)` / `prev(it, n)` | O(n) | throws `std::invalid_argument` if the iterator belongs to another container |
| `find(value)` / `find_if(pred)` | O(n) | returns first match or `end()` |
| `sort(comp)` / `sort()` | O(n log n) | stable merge sort, relinks nodes |
| `insertion_sort(comp)` / `insertion_sort()` | O(n²) worst, O(n) best | stable, relinks nodes |

**Internal helper `get_node_at(pos)`** walks from whichever end is nearer (`head_` if `pos < size_/2`,
otherwise `tail_`), halving the average traversal cost for indexed access — a nice detail to mention:
*"indexed access is still O(n), but it is O(min(pos, n-pos)), not O(pos)."*

### 8.4 Iterators (`BasicIterator<IsConst>`)

`Iterator` is `BasicIterator<false>`, `ConstIterator` is `BasicIterator<true>`. A const iterator
can be constructed from a non-const one, but not the reverse (enforced with `enable_if`).

Each iterator stores **two** members:
- `Node* node_` — the current node, or `nullptr` for `end()`/singular.
- `const Container* owner_` — the container it came from.

**Why the owner pointer matters:** this is a linked list *without* a circular dummy sentinel, so
`end()` has `node_ == nullptr`. To support `--end()` (going back to the last element), the iterator
needs the container to find `tail_`. See `core/Container.h:378-394`. The owner pointer also lets
`next`/`prev` detect that an iterator belongs to a different container and throw.

**Documented iterator semantics (from the header):**
- An iterator stays valid when other elements are inserted/removed; it is invalidated only when the element it points to is removed.
- `end()` stays equal to the container's current `end()` after modifications.
- Iterators refer to the container they were created from; they are not re-targeted by a move or swap.

**Safety:** dereferencing `end()`/singular, `++end()`, `--begin()`, and `--end()` on an empty
container all throw `std::out_of_range` with a descriptive message, rather than invoking UB.

### 8.5 The sort algorithms (the technical centrepiece)

**Merge sort — `Container::sort(comp)` → `merge_sort_chain` (`core/Container.h:574-747`)**

```
sort():
  chain = head_
  try   merge_sort_chain(chain, size_, comp)   // recursive top-down
  catch relink(chain); throw                   // basic guarantee on comparator throw
  relink(chain)
```

- `split_chain` cuts the node chain in half (by walking to the midpoint and null-terminating).
- `merge_sort_chain` recurses, then `merge_chains` merges two sorted chains by **relinking `next` pointers**.
- `relink` rebuilds all `prev`/`next` pointers and updates `head_`/`tail_` in one pass.

**Properties to state confidently:**
- **Stable** — `merge_chains` uses `if (comp(y, x))` (only takes from the right list when strictly smaller), so equal elements keep their relative order.
- **Zero allocations** — no temporary nodes, no auxiliary array; sorting is pure pointer rewiring.
- **No element is copied, moved or destroyed** — `relink`/`merge_chains` only touch `prev`/`next`.
- **O(n log n)** time, **O(log n)** call-stack space.
- **References/iterators stay valid** across a sort — the same element simply changes position.

**Insertion sort — `Container::insertion_sort(comp)` (`core/Container.h:596-619`)**
Maintains a sorted sub-chain and inserts each node into place using `detach` + `attach_after`.
Also stable, also pure relinking. Quadratic on random data, **linear on already-sorted data**
(verified in the benchmarks: 100,000 sorted elements in ~0.37 ms).

**Exception safety of sorting:** if the user comparator throws, `sort` catches, calls `relink`
to restore valid links across *all* nodes, and rethrows. The result is the **basic guarantee**:
nothing is leaked or lost, but the order is unspecified. The header states this explicitly
(`core/Container.h:536-546`), and `tests/test_algorithms.cpp` exercises it.

**The key invariant** (documented at `core/Container.h:729-731`): when `merge_sort_chain` returns
*or throws*, `head` refers to a null-terminated chain containing **exactly the same nodes** it was
given — nothing lost, nothing duplicated. The try/catch blocks exist precisely to maintain this.

### 8.6 Internal memory helpers
- `unlink_and_delete(node)` — unlinks a node, fixes `head_`/`tail_`, deletes it, decrements size.
- `detach` / `attach_after` — used by insertion sort to move a node without deleting it.
- `relink(head)` — rebuilds the whole chain's `prev` links and the cached `head_`/`tail_`.
- `swap` member and a free `swap(Container&, Container&)` — both `noexcept`, O(1).

### 8.7 What to say about it
> "The container is a node-based doubly linked list. Nodes are never relocated, so references
> stay valid; the two links give O(1) operations at both ends and allow bidirectional iterators,
> including decrementing `end()`. Sorting is a stable, in-place merge sort that rewires pointers
> instead of moving values — so a book reference you obtained before a sort is still valid after.
> If a comparator throws, the container is left valid thanks to a `relink` in the catch block."

---

## 9. Domain layer in depth: `Book`, `Member`, `Text`

### 9.1 `Book` (`domain/Book.h`, `domain/Book.cpp`)

**Fields (all private):** `title_`, `author_`, `isbn_`, `genre_` (strings), `available_` (bool).

**Invariants (documented at the top of the header):**
- `title_`, `author_`, `genre_` are **trimmed and non-empty**.
- `isbn_` is always in **normalized, valid form** (10 or 13 characters).
- *No `Book` can ever violate these* — because the only way to set them is through validating operations.

**Construction & setters**
- `Book(title, author, isbn, genre, available=true)` validates immediately: `text::require_non_blank` for the text fields, `normalize_isbn` for the ISBN. Throws `std::invalid_argument` on failure.
- `set_title` / `set_author` / `set_genre` — validate and trim first; **on throw the old value is unchanged**.
- There is **no default constructor** (a book cannot be "blank" — this is a deliberate design point, see §25 Q9).

**ISBN handling** (`Book::normalize_isbn`, `Book::is_valid_isbn`)
- Normalization strips all whitespace and hyphens, and uppercases a trailing `'x'` to `'X'`.
- Accepts **13 characters, all digits**, or **10 characters: nine digits + a final digit or `X`**.
- `normalize_isbn` throws `std::invalid_argument("invalid ISBN: " + raw)`; `is_valid_isbn` is the `noexcept` predicate.
- **Limitation:** it checks length and character classes but **not the ISBN checksum** — state this honestly if asked.

**Borrowing state**
- `mark_borrowed()` throws `std::logic_error("book is already borrowed")` if already borrowed.
- `mark_returned()` throws `std::logic_error("book is not borrowed")` if already available.
- So availability can only flip through these two guarded methods.

**Identity**
```cpp
friend bool operator==(const Book& lhs, const Book& rhs) noexcept {
    return lhs.isbn_ == rhs.isbn_;      // identity is the ISBN alone
}
```
The header explains why: `Container::find` and `Container::remove_value` use `operator==`, so making
identity the ISBN means lookups work regardless of title/author/genre/availability changes.

### 9.2 `Member` (`domain/Member.h`, `domain/Member.cpp`)

**Fields:** `id_` (int), `name_` (string), `borrowed_` (`Container<std::string>` of ISBNs).

**Invariants:**
- `id_ > 0`.
- `name_` is trimmed and non-empty.
- `borrowed_` never contains duplicates and never holds more than `kMaxBorrowed` (= 5) entries.

**Key members**
- `static constexpr std::size_t kMaxBorrowed = 5;` — the single constant that defines the borrow limit.
- `Member(int id, std::string name)` — throws `std::invalid_argument` if `id <= 0` or name is blank.
- `borrowed_count()` — O(1) (the container caches its size).
- `can_borrow()` — `noexcept`; true iff `borrowed_count() < kMaxBorrowed`.
- `has_borrowed(isbn)` — O(k); checks the borrowed list, retrying with a trimmed form if needed.
- `add_borrowed(isbn)` — O(k); (1) validates non-blank, (2) throws `std::logic_error` on duplicate, (3) throws `std::logic_error` if the limit is reached, then pushes.
- `remove_borrowed(isbn)` — O(k); throws `std::logic_error` if the member did not borrow it.
- `borrowed_isbns()` — returns a `const Container<std::string>&`.
- `operator==` compares **id** (a member's identity is its id).

### 9.3 `Text` (`domain/Text.h`) — header-only string helpers

| Function | Behaviour |
|---|---|
| `trim(s)` | removes leading/trailing whitespace, keeps inner whitespace |
| `require_non_blank(value, field)` | returns trimmed value; throws `std::invalid_argument(field + " must not be empty")` if empty |
| `to_lower(s)` | ASCII-only lowercasing |
| `compare_ignore_case(a, b)` | lexicographic comparison of ASCII-folded bytes, no allocation; shorter prefix is smaller |
| `equals_ignore_case(a, b)` | convenience wrapper over `compare_ignore_case == 0` |
| `contains_ignore_case(text, part)` | case-insensitive substring search; empty `part` matches everything |

**Important limitation (state it):** case folding is **ASCII-only**. Bytes ≥ 128 (e.g. Arabic UTF-8
sequences) are left untouched and compared by raw byte value. The program never changes the locale.
This is why Arabic text is preserved exactly, but is not case-folded.

---

## 10. Service layer in depth: `LibraryService`

File: `service/LibraryService.{h,cpp}`. This is the **Facade** and the heart of the business logic.

### 10.1 What it holds
```cpp
Container<Book>   books_;        // the catalog
Container<Member> members_;      // the member registry
std::shared_ptr<ILibraryRepository> repository_ = std::make_shared<NullRepository>();
```
A default-constructed service is purely in-memory (Null Object). A service built by `open()` shares
its repository with the loaded data.

### 10.2 The four invariants (verbatim from `LibraryService.h:26-30`)
> **I1.** ISBNs are unique in the catalog and member ids are unique.
> **I2.** A book is unavailable iff exactly one member holds its ISBN.
> **I3.** Every ISBN in a member's borrowed list belongs to a catalog book.
> **I4.** No member holds more than `Member::kMaxBorrowed` books.

These are re-established after *every* public call — including calls that throw.

> ⚠️ **Drift to be aware of:** `docs/DESIGN_DECISIONS.md` (ADR-06) paraphrases I3/I4 incorrectly
> ("valid loan relationships", "non-negative counts"). The **authoritative** definition is the one
> above from the header and README. If a reviewer quotes the ADR wording, point them at the header.

### 10.3 Public API

**Members**
| Method | Complexity | Throws |
|---|---|---|
| `add_member(id, name)` | O(m) | `invalid_argument` (bad id/name), `DuplicateError` |
| `remove_member(id)` | O(m) | `NotFoundError`, `logic_error` if the member still holds books |
| `has_member(id)` | O(m) | — |
| `get_member(id)` | O(m) | `NotFoundError` |
| `member_count()` | O(1) | — |
| `members()` | O(1) | returns `const Container<Member>&` |

**Books**
| Method | Complexity | Throws |
|---|---|---|
| `add_book(Book)` | O(n) | `invalid_argument` if the book is not available; `DuplicateError` |
| `remove_book(isbn)` | O(n) | `invalid_argument` (bad ISBN), `NotFoundError`, `logic_error` if borrowed |
| `has_book(isbn)` | O(n) | returns `false` for an invalid ISBN (never throws) |
| `get_book(isbn)` | O(n) | `invalid_argument` (bad ISBN), `NotFoundError` |
| `total_books()` | O(1) | — |
| `books()` | O(1) | returns `const Container<Book>&` |

**Borrowing**
| Method | Complexity | Returns / Throws |
|---|---|---|
| `borrow_book(isbn, member_id)` | O(n + m + k) | `NotFoundError` (member/book), `logic_error` (not available / limit) |
| `return_book(isbn)` | O(n + m·k) | returns the holder's id; `NotFoundError`, `logic_error` (not borrowed) |

**Statistics:** `available_books()` O(n), `borrowed_books()` O(n) (= total − available).

**Queries & sorting:** `search(strategy, query)` O(n) → new `Container<Book>`; `sort_books(strategy, order, algorithm)` O(b log b) merge / O(b²) insertion, **in place**. Convenience wrappers exist for each field.

### 10.4 Borrow / return, step by step

**`borrow_book(isbn, member_id)` — `service/LibraryService.cpp:139-164`**
1. Normalize the ISBN → `normalized`.
2. Find the member; if missing → `NotFoundError`.
3. Find the book; if missing → `NotFoundError`.
4. If the book is not available → `logic_error("book is not available")`.
5. If the member cannot borrow (limit) → `logic_error("borrow limit reached")`.
6. **Apply:** `repository_->record_borrow(normalized, member_id)` first (DB), then `member.add_borrowed(normalized)`, then `book.mark_borrowed()`.

**`return_book(isbn)` — `service/LibraryService.cpp:166-193`**
1. Normalize the ISBN. 2. Find the book (`NotFoundError`). 3. If it is available → `logic_error("book is not borrowed")`. 4. Find the member who holds it (an invariant guard throws if none — unreachable via the public API). 5. **Apply:** `record_return` (DB), then `member.remove_borrowed`, then `book.mark_returned`. 6. Return the holder's id.

### 10.5 The strong exception guarantee, and the write-ahead idea
Every mutating method follows the same discipline: **validate everything, then write to the
repository, then change memory.** If the repository throws (disk full, DB locked, constraint),
memory has not been touched, so the object is exactly as it was — the **strong guarantee**.

The single documented corner case (from the header, `LibraryService.h:150-151`): if a
`std::bad_alloc` occurs during the *in-memory* update *after* a successful repository write, the
database could be one step ahead of memory. This is stated openly rather than hidden.

> **This is the single best thing to explain in the defense.** Draw the three-step order
> (validate → disk → memory) on the board and say: *"That ordering is the whole trick."*

### 10.6 Opening a persisted library — `LibraryService::open(repository)` (`service/LibraryService.cpp:278-299`)
1. Null check → `std::invalid_argument("repository must not be null")`.
2. `repository->load()` → a `StoredLibrary`.
3. Rebuild state **through the public API**: add every member, then every book, then replay every open loan with `borrow_book`.
4. If any of that throws (corrupt/hand-edited data), wrap it as `DatabaseError("database contents are inconsistent: " + what)`.
5. Attach the repository so subsequent writes go through.

> Why replay through the public API? It re-uses all the validation and invariants for free — if the
> stored data is inconsistent, loading fails loudly instead of producing a broken service.

---

## 11. Strategies: search and sort

### 11.1 Search — `service/SearchStrategy.{h,cpp}`
Interface:
```cpp
class ISearchStrategy {
    virtual bool matches(const Book& book, const std::string& query) const = 0;
    virtual const char* name() const noexcept = 0;
};
```
| Strategy | Matching rule |
|---|---|
| `TitleSearch` | case-insensitive **substring** of the title |
| `AuthorSearch` | case-insensitive **substring** of the author |
| `GenreSearch` | case-insensitive **whole-word equality** ("fiction" does **not** match "Non-Fiction") |
| `IsbnSearch` | normalized ISBN **equality**; an invalid/partial query matches nothing (never throws) |

`SearchStrategyFactory::create(key)` accepts `"title" | "author" | "isbn" | "genre"`
(trimmed, case-insensitive) and throws `std::invalid_argument("unknown search criterion: " + key)` otherwise.

Precondition (documented): the service passes a **trimmed, non-blank** query; strategies themselves
never trim and never throw.

### 11.2 Sort — `service/SortStrategy.{h,cpp}`
Interface:
```cpp
class ISortStrategy {
    virtual bool before(const Book& a, const Book& b) const noexcept = 0;  // strict weak ordering
    virtual const char* name() const noexcept = 0;
};
```
| Strategy | Ordering |
|---|---|
| `TitleSort` | by title, case-insensitive |
| `AuthorSort` | by author, case-insensitive |
| `GenreSort` | by genre, case-insensitive |

`SortStrategyFactory::create(key)` accepts `"title" | "author" | "genre"`.

`LibraryService::sort_books(strategy, order, algorithm)` builds an ascending or descending
comparator from the strategy and hands it to `Container::sort` (merge) or `Container::insertion_sort`.
Enums: `SortOrder { Ascending, Descending }`, `SortAlgorithm { Merge, Insertion }`.

> **Why Strategy here:** the service never contains a `switch` over "title/author/genre". New
> criteria are added by writing a new strategy and registering it in the factory — Open/Closed.
> A reviewer may ask "isn't virtual dispatch slower?" Answer: the cost is negligible next to the
> O(n) scan, and it buys extensibility.

---

## 12. Repository layer: persistence

### 12.1 `ILibraryRepository` (`repository/ILibraryRepository.h`)
The abstraction the service depends on. Every method **throws `DatabaseError` on failure and leaves
stored data unchanged** on a failed call.

```cpp
struct StoredLoan   { std::string isbn; int member_id; };
struct StoredLibrary {
    std::vector<Book>       books;       // catalog order; every book available
    std::vector<Member>     members;     // insertion order; borrowed lists empty
    std::vector<StoredLoan> open_loans;  // oldest first
};

class ILibraryRepository {
    virtual StoredLibrary load() = 0;
    virtual void add_book(const Book&) = 0;
    virtual void remove_book(const std::string& isbn) = 0;
    virtual void add_member(const Member&) = 0;
    virtual void remove_member(int id) = 0;
    virtual void record_borrow(const std::string& isbn, int member_id) = 0;
    virtual void record_return(const std::string& isbn) = 0;
};
```
**Crucial contract:** *Availability is NOT stored.* A book is borrowed exactly when it has an open
loan — the loan table is the single source of truth. `add_book` is only ever called for available books.

### 12.2 `RepositoryErrors.h`
`DatabaseError : std::runtime_error` — used for SQL errors, constraint violations, missing records, corruption.

### 12.3 `NullRepository` (`repository/NullRepository.h`)
The **Null Object** implementation: `load()` returns an empty `StoredLibrary`; all writes are no-ops.
It is the default repository, so all in-memory uses and early tests run without a database.

### 12.4 `SqliteRepository` (`repository/SqliteRepository.{h,cpp}`)
**RAII:** the `sqlite3*` handle lives in `std::unique_ptr<sqlite3, SqliteCloser>`; `SqliteCloser`
calls `sqlite3_close_v2`. A local `Statement` class wraps `sqlite3_stmt*` and finalizes it in its
destructor, so prepared statements can never leak even when exceptions are thrown mid-operation.

**Constructor sequence:**
1. Reject an empty path (`std::invalid_argument`).
2. `sqlite3_open_v2(path, …, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)`.
3. `sqlite3_busy_timeout(db, 2000)` — wait up to 2 s on a busy database.
4. `PRAGMA foreign_keys = ON` and **verify** it actually read back as `1`.
5. Read `PRAGMA user_version`: `0` → create the schema in a `BEGIN IMMEDIATE` … `COMMIT` (with `ROLLBACK` on error) and set `user_version = 1`; `1` → ok; anything else → `DatabaseError("unsupported database version: …")`.

**Prepared statements for every query** (`sqlite3_prepare_v2` + `sqlite3_bind_text` / `sqlite3_bind_int`)
mean **no SQL is ever built by string concatenation** — SQL injection is impossible.

**Change detection:** `remove_book`, `remove_member` and `record_return` check `sqlite3_changes()`
and throw if zero rows were affected (e.g. "book not found in database" / "no open loan for book").

**`load()`** runs inside `BEGIN` … `COMMIT` (with `ROLLBACK` on error) and reads books, members and
open loans `ORDER BY seq` so insertion order is preserved. If a row violates a domain invariant, it
is reported as `DatabaseError("corrupt data in database: …")`.

---

## 13. The SQLite database schema

Created with `user_version = 1`:

```sql
CREATE TABLE books (
  seq    INTEGER PRIMARY KEY AUTOINCREMENT,
  isbn   TEXT NOT NULL UNIQUE,
  title  TEXT NOT NULL CHECK (length(trim(title))  > 0),
  author TEXT NOT NULL CHECK (length(trim(author)) > 0),
  genre  TEXT NOT NULL CHECK (length(trim(genre))  > 0)
);

CREATE TABLE members (
  seq  INTEGER PRIMARY KEY AUTOINCREMENT,
  id   INTEGER NOT NULL UNIQUE CHECK (id > 0),
  name TEXT NOT NULL CHECK (length(trim(name)) > 0)
);

CREATE TABLE loans (
  seq         INTEGER PRIMARY KEY AUTOINCREMENT,
  book_isbn   TEXT    NOT NULL REFERENCES books(isbn)  ON DELETE CASCADE,
  member_id   INTEGER NOT NULL REFERENCES members(id)  ON DELETE CASCADE,
  borrowed_at TEXT    NOT NULL DEFAULT (datetime('now')),
  returned_at TEXT
);

-- at most one OPEN loan per book, enforced by the engine itself
CREATE UNIQUE INDEX one_open_loan_per_book
  ON loans(book_isbn) WHERE returned_at IS NULL;
```

**Integrity rules to explain:**
1. **Availability is derived**, never stored: a book is borrowed iff a row in `loans` has `returned_at IS NULL`.
2. **One open loan per book** is enforced by a *partial unique index* — the database itself refuses a double-borrow.
3. **Foreign keys with `ON DELETE CASCADE`** mean deleting a book/member also removes its (closed) history.
4. **`CHECK` constraints mirror the domain invariants** (non-blank text, positive ids) so bad data cannot be inserted even by hand.

> ⚠️ **Drift:** the ER diagram in `README.md` labels the loans primary key `id` and names the index
> `idx_open_loans`. The actual code uses `seq` and `one_open_loan_per_book`. Trust the code.

**Restart behaviour:** at startup, `LibraryService::open` replays books, members and open loans
through the public API. In-memory **sort order is transient** and not persisted — the catalog comes
back in insertion order.

---

## 14. UI layer: `main`, `ConsoleApp`, `InputReader`, `Table`, `DemoData`

### 14.1 `main.cpp` — entry point and CLI
- `parse_args` accepts `--db <file>` (default `library.db`) and `-h`/`--help`. A repeated `--db` or an unknown argument is an **error**; a missing value for `--db` is an **error**.
- **Exit codes:** `0` clean shutdown or `--help`; `2` invalid command-line usage; `1` unhandled runtime exception.
- Wiring: `SqliteRepository` → `LibraryService::open` → `ConsoleApp(service, std::cin, std::cout)` → `app.run()`.

### 14.2 `ConsoleApp` (`ui/ConsoleApp.{h,cpp}`)
- Holds a `LibraryService&`, an `InputReader`, and an `std::ostream&` (all **injected** → testable).
- `struct MenuEntry { std::string label; std::function<void()> action; }` — a lightweight **Command** pattern. Two dispatch tables: the main menu (10 entries) and the members submenu (4 entries).
- `run()` prints the banner, runs the main menu, prints "Goodbye!", and returns 0; it catches `InputClosed` to exit cleanly when the stream ends.
- `run_menu(title, entries, exit_label)` prints a numbered menu, reads a choice, and calls `execute`.
- `execute(action)` is the **error boundary**: it catches `CancelledByUser` (prints "Cancelled.") and `std::exception` (prints "Error: …"), but **rethrows `InputClosed`** so the program can exit.
- `print_books(books, empty_message)` renders a six-column `Table` (#, Title, Author, ISBN, Genre, Status) and prints the count.
- Actions: list/add/remove book, search, borrow, return, sort, statistics, members submenu, load sample data.

### 14.3 `InputReader` (`ui/InputReader.{h,cpp}`)
Two control-flow exceptions: `InputClosed` (stream ended) and `CancelledByUser` (user typed `/cancel`).
- `read_line(prompt)` — prints the prompt, flushes, `getline`, strips a trailing `'\r'` (Windows CRLF), trims.
- `read_text(prompt, cancellable)` — rejects empty; throws `CancelledByUser` on `/cancel`.
- `read_int(prompt, min, max, cancellable)` — parses with `std::from_chars` (strict, no exceptions), requires the whole token to be consumed and the value to be within `[min, max]`, otherwise re-prompts. Throws `std::invalid_argument` if `min > max`.
- `confirm(prompt)` — accepts `y`/`yes`/`n`/`no` (case-insensitive), re-prompts otherwise.
- `read_isbn(prompt)` — loops until `Book::is_valid_isbn`, then returns the **normalized** ISBN.

### 14.4 `Table` (`ui/Table.{h,cpp}`)
A small ASCII table builder. `add_column(header, max_width)` (throws if rows already exist or width 0),
`add_row(cells)` (throws if the cell count mismatches the column count), `row_count()`, `print(out)`.
Column width = `min(max_width, widest cell/header)`, using `utf8::display_width`. Cells are
truncated with `…`→`...` (`truncate_display`) and padded (`pad_right`).

### 14.5 `Utf8` (`ui/Utf8.h`)
- `display_width(s)` = count of UTF-8 **code points** = count of bytes where `(b & 0xC0) != 0x80`.
- `truncate_display(s, max_width)` cuts on code-point boundaries, appending `...` when `max_width ≥ 4`.
- `pad_right(s, width)` pads with spaces.
- **Documented limitation:** it counts code points, not terminal cells — combining marks and double-width characters are not handled (see §23).

### 14.6 `DemoData` (`ui/DemoData.{h,cpp}`)
`load_demo_data(service)` inserts **14 books** (including two with Arabic titles/authors via `\x..`
UTF-8 escapes) and **5 members**. It is **idempotent**: it skips anything that already exists
(`service.has_book` / `service.has_member`) and returns counts of what it actually added. This makes
"Load sample data" safe to run repeatedly, even on a persisted database.

---

## 15. Build system, options and targets

`CMakeLists.txt` (CMake ≥ 3.16, `CMAKE_CXX_STANDARD 17`, required).

**Targets**
| Target | Type | Contents |
|---|---|---|
| `sqlite3_c` | static lib | `third_party/sqlite/sqlite3.c` (with `SQLITE_THREADSAFE=0`, `SQLITE_OMIT_LOAD_EXTENSION`, `SQLITE_DQS=0`) |
| `titans_lib` | static lib | all of `domain/`, `service/`, `ui/`, `repository/` |
| `titans` | executable | `main.cpp` |
| `doctest` | INTERFACE | header-only include path for the test framework |
| `titans_tests` | executable | all 18 test translation units |
| `titans_bench` | executable | built only when `TITANS_BUILD_BENCH=ON` |

**Options**
- `TITANS_SANITIZE` — default **ON for GCC/Clang on Linux and macOS**, **OFF elsewhere**. On Windows
  the option is forced OFF with a warning, because Windows toolchains ship no ASan/UBSan runtime.
  When ON, `-fsanitize=address,undefined` is added to both compile and link.
- `TITANS_BUILD_BENCH` — default OFF; builds the benchmark.

**Testing hook:** `enable_testing()` + `add_test(NAME titans_tests COMMAND titans_tests)`.

**Build commands**
```bash
# Linux / macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/titans_tests && ./build/titans

# Windows (Developer PowerShell)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\titans_tests.exe ; .\build\titans.exe

# multi-config runners must pass the config to ctest:
ctest --test-dir build -C Release --output-on-failure
```

---

## 16. Testing and reliability

**Framework:** doctest 2.4.11, bundled (`tests/test_main.cpp` provides `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`).
**Result:** **189 test cases / 209,475 assertions, 0 failures**, identical on MSVC and GCC.
**Why so many assertions:** several suites are randomized/stress tests that perform thousands of
operations and assert after each one, which is where the six-figure assertion count comes from.

### Per-suite inventory
| File | Cases | Focus |
|---|---|---|
| `test_container.cpp` | 12 | insert/remove at head/middle/tail, edge cases, stress |
| `test_iterator.cpp` | 16 | `begin`/`end`, `++`/`--`, reverse iterators, bounds, cross-container checks |
| `test_algorithms.cpp` | 10 | merge/insertion sort (random/sorted/reversed), find, stability, comparator exceptions |
| `test_book.cpp` | 10 | Book invariants, trimming, ISBN normalization, equality |
| `test_member.cpp` | 10 | Member invariants, 5-book limit, duplicate ISBN checks |
| `test_text.cpp` | 4 | UTF-8 preservation, ASCII case folding, trimming |
| `test_strategies.cpp` | 13 | all search/sort strategies and the factories |
| `test_library_service.cpp` | 29 | facade mutations, invariant maintenance, strong guarantee |
| `test_library_queries.cpp` | 24 | statistics, searches, member queries |
| `test_library_scenarios.cpp` | 3 | multi-step workflows + a 3,000-operation randomized stress test |
| `test_ui_text.cpp` | 8 | UTF-8 table column formatting and widths |
| `test_input_reader.cpp` | 4 | input parsing, bounds, `/cancel` |
| `test_console_app.cpp` | 12 | menu navigation, actions, error formatting |
| `test_console_sessions.cpp` | 3 | piped sessions + randomized console fuzzing |
| `test_sqlite_repository.cpp` | 10 | schema, prepared statements, constraints |
| `test_service_repository.cpp` | 10 | service write-ahead integration with SQLite |
| `test_persistence.cpp` | 10 | persistence round-trips + randomized loan replays |
| `test_main.cpp` | 1 | doctest entry point (`DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`) |

### What reliability beyond logic covers
- **Memory & undefined behaviour:** the suite is built and run under **AddressSanitizer + UBSan** on Linux/macOS. ASan catches leaks/buffer issues; UBSan catches signed overflow, bad shifts, etc.
- **Exception safety:** the tests deliberately throw from comparators and repositories and check that invariants and container links survive.
- **Randomized/property-based style:** scenario tests drive thousands of random operations and validate invariants throughout.

> **Say this:** "I treat testing as a design tool, not an afterthought. The invariant checks that
> the stress tests assert are the same invariants the service promises — so a failure points at a
> specific rule being broken."

---

## 17. Continuous integration

`.github/workflows/ci.yml` runs on every push and pull request:

| OS | `TITANS_SANITIZE` |
|---|---|
| `ubuntu-latest` | `ON` |
| `macos-latest` | `OFF` |
| `windows-latest` | `OFF` |

Steps: checkout → (Linux only) `sysctl -w vm.mmap_rnd_bits=28` to let ASan map its shadow memory on
newer kernels → configure with `-DTITANS_SANITIZE=…` → build `--config Release` → `ctest -C Release --output-on-failure`.

**Why sanitizers only on Ubuntu:** the sanitizer runtime is guaranteed present on Linux runners;
macOS is used as a plain second clang build; Windows toolchains have no compatible runtime (and
even would fail to link `-lasan`), so the option is off there by design.

---

## 18. Performance and benchmarks

`bench/bench_container.cpp` compares `titans::Container<int>` against `std::list<int>` at
n = 1,000 / 10,000 / 100,000, median of 7 runs, RSS-seeded, sink-guarded against dead-code
elimination. Full data in `docs/BENCHMARKS.md`. (Measured on Windows 11, i5-12400F, MSVC 19.50, Release.)

Highlights:
- **push_back / iterate:** essentially identical to `std::list` (both do per-node heap allocation and pointer chasing).
- **find last element:** linear and comparable to `std::find` on a list (~283 µs vs ~281 µs at n=100,000).
- **sort:** `Container::sort()` scales as expected O(n log n); at n=100,000 it is 11.42 ms vs `std::list::sort()` 8.44 ms (MSVC uses an iterative binning merge sort with better cache behaviour).
- **insertion_sort:** demonstrates the textbook curve — 0.30 ms at n=1,000 rising to 88.23 ms at n=10,000 (≈290× for 10× data), and **0.37 ms at n=100,000 when already sorted** (linear best case).
- **remove_value every 2nd element, n=10,000:** 37.85 ms vs `std::list::remove` 91.63 ms — but note the APIs differ (`remove_value` removes the **first** match; `std::list::remove` removes **all** matches), so this is not an apples-to-apples comparison.

**Run it:**
```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DTITANS_SANITIZE=OFF -DTITANS_BUILD_BENCH=ON
cmake --build build-bench --config Release --target titans_bench
./build-bench/titans_bench
```

> **Be honest here:** the point of the benchmark is to show the container behaves like a real
> linked list (linear, allocation-bound) and that the sorts have the right asymptotics — not to
> beat the STL.

---

## 19. Design patterns used, and why

| Pattern | Where | Why it was used |
|---|---|---|
| **Iterator** | `Container::BasicIterator`, `Iterator`, `ConstIterator` | Traverse the list without exposing its internals; support range-`for` and reverse iteration |
| **Strategy** | `ISearchStrategy`, `ISortStrategy` | Add or swap search/sort behaviour without editing the service |
| **Factory** | `SearchStrategyFactory`, `SortStrategyFactory` | Turn a user-facing key ("title") into the right object; one place to extend |
| **Facade** | `LibraryService` | One simple API (the only entry point) hiding container + strategies + rules |
| **Repository** | `ILibraryRepository` / `SqliteRepository` | Keep business logic independent of the storage engine |
| **Null Object** | `NullRepository` | In-memory mode with no null checks (`if (repository_) …`) sprinkled around |
| **Command (lightweight)** | `MenuEntry { label; std::function<void()> action; }` | Data-driven menu dispatch instead of a big `switch` |

---

## 20. C++ techniques applied

- **RAII** — `unique_ptr<sqlite3, SqliteCloser>` and the `Statement` wrapper release database resources deterministically, even on exceptions.
- **Rule of Five** — `Container<T>` defines copy ctor/assign, move ctor/assign (`noexcept`), and destructor; copy uses **copy-and-swap** for correctness.
- **Templates** — `Container<T>` is generic; `Node` uses a perfect-forwarding constructor.
- **Const-correctness** — read paths return `const Container&`/`const Book&`; iterators have const and non-const forms with a one-way conversion.
- **Move semantics** — `std::move` used for strings/books/members; containers move in O(1).
- **Exception safety** — basic guarantee in the container's sort, strong guarantee in the service.
- **Dependency injection** — streams into the UI classes, repository pointer into the service.
- **`std::from_chars`** — exception-free, locale-independent integer parsing in `InputReader`.
- **`noexcept` discipline** — used on operations that truly cannot throw (getters, size, swap, move).

---

## 21. Complexity cheat-sheet

Let **b** = books, **m** = members, **k** = ISBNs held by one member (≤ 5), **r** = matches returned.

| Operation | Time | Space |
|---|---|---|
| `Container::push_front` / `push_back` | O(1) | O(1) |
| `Container::insert(pos)` / `remove_at(pos)` | O(n) | O(1) |
| `Container::remove_value` | O(n) | O(1) |
| `Container::find` / `find_if` | O(n) | O(1) |
| `Container::sort` (merge) | O(n log n) | O(log n) stack, 0 heap |
| `Container::insertion_sort` | O(n²) worst / O(n) best | O(1) |
| `LibraryService::add_book` / `remove_book` | O(b) | O(1) |
| `LibraryService::has_book` / `get_book` | O(b) | O(1) |
| `LibraryService::add_member` / `remove_member` | O(m) | O(1) |
| `LibraryService::borrow_book` | O(b + m + k) | O(1) |
| `LibraryService::return_book` | O(b + m·k) | O(1) |
| `LibraryService::search` | O(b) | O(r) |
| `LibraryService::sort_books` (merge) | O(b log b) | O(log b) stack |
| `LibraryService::total_books` | O(1) | O(1) |
| `LibraryService::available_books` / `borrowed_books` | O(b) | O(1) |

The database-write cost is not included in these figures.

---

## 22. Design decisions (ADR summary)

Full detail in `docs/DESIGN_DECISIONS.md`. Condensed:

| ADR | Decision | Benefit / cost |
|---|---|---|
| 01 | Custom doubly linked list | O(1) ends, stable node refs; two pointers/node overhead |
| 02 | Iterators store an owner pointer | Safe `--end()`, cross-container detection; 16-byte iterators |
| 03 | In-place node-relinking merge sort | Stable, O(n log n), zero allocation; references survive |
| 04 | Strategy + factories | Open/Closed; negligible virtual-call cost |
| 05 | ISBN is a book's identity | Lookups survive attribute changes |
| 06 | Four invariants + strong guarantee | Consistency even when disk fails; documented `bad_alloc` gap |
| 07 | Write-ahead + derived availability | No dual-state bugs; single source of truth |
| 08 | Null Object repository | No null checks; fast in-memory tests |
| 09 | Injected streams in UI | Fully headless-testable UI |
| 10 | Bundled SQLite amalgamation | Deterministic, offline builds |
| 11 | Explicit scope boundaries | No threads/auth/fines → auditable, no UB |

---

## 23. Limitations, known issues and documentation drift

Knowing these makes you look *more* credible, not less. If a reviewer finds one, you want to have
found it first.

### Deliberate limitations (documented, by design)
- In-memory sort order is transient (not persisted).
- Copies of a persistent `LibraryService` share the same repository handle.
- Single-threaded; no mutexes.
- ASCII-only case folding; non-ASCII compared by raw bytes.
- No right-to-left shaping (relies on the terminal).
- No due dates, renewals or fines.
- ISBN validation does **not** check checksum digits.
- Not safe for several instances editing the same DB file simultaneously (state is loaded once at startup).

### Real issues / drift you should be aware of
1. **Doc drift in `docs/ARCHITECTURE.md`:** it references `SqliteRepository::init_schema()`,
   `prepare_statements()`, `ISortStrategy::less(...)`, and `InputReader::read_isbn(string, bool)` —
   none of which exist. The code has no such methods and uses `before(...)` and `read_isbn(string)`.
   Treat the code as authoritative.
2. **README ER-diagram drift:** loans key/index names (`id`, `idx_open_loans`) differ from the code (`seq`, `one_open_loan_per_book`).
3. **`DESIGN_DECISIONS.md` ADR-06 invariant wording** (I3 "valid loan relationships", I4 "non-negative counts") disagrees with the authoritative I1–I4 in the header/README.
4. **`ctest` without `-C Release` fails on multi-config (Visual Studio) generators** — a CMake/CTest behaviour, not a code bug. CI and the correct command use `-C Release`.
5. **Build warnings under GCC `-Wall -Wextra`:** an unused helper `book_isbns_to_vector` in `tests/test_book.cpp` and `-Wreorder` in `tests/repository_fixtures.h`. MSVC does not emit them. They are in test code only.

### Ownership note (if asked about `v1.0.0`)
Tag `v1.0.0` points to a commit that is not an ancestor of `master` (a prior history was amended).
`v1.0.1` and `v1.0.2` are the current line. This is a git-history artefact, not a code issue.

---

## 24. Presentation playbook

### 24.1 Slide deck
`docs/presentation.html` — a self-contained (offline) 16-slide deck. Controls: `←` `→` navigate,
`F` fullscreen, `N` speaker notes, `Ctrl+P` to export a PDF handout.

Slide order and the one sentence each slide must land:

| # | Slide | The sentence to land |
|---|---|---|
| 1 | Title | "A layered, test-driven C++17 library system built on my own container." |
| 2 | Problem | "Manual tracking loses consistency; this system makes consistency structural." |
| 3 | Objectives | "Five objectives, each owned by exactly one layer." |
| 4 | System overview | "User → service → storage, one direction, five layers." |
| 5 | Architecture | "Each layer has one job and does not know the others' internals." |
| 6 | Main components | "These are the classes and which layer each belongs to." |
| 7 | Custom container | "My own doubly linked list: O(1) ends, stable nodes, in-place stable sort." |
| 8 | Design patterns | "Problems I recognised and the standard answers I applied." |
| 9 | Domain model | "Entities validate themselves; invalid objects cannot exist." |
| 10 | Persistence | "SQLite behind an interface; availability is derived, never stored twice." |
| 11 | Features | "Everything the user can do." |
| 12 | Workflow | "Borrow: validate → disk → memory. That order is the trick." |
| 13 | Testing | "189 cases, 209,475 assertions, clean under ASan and UBSan." |
| 14 | Challenges | "Each challenge has a specific, named solution." |
| 15 | Conclusion | "What I built and what I learned." |
| 16 | Future work | "GUI, richer search, reporting, auth, DB features — all behind existing seams." |

### 24.2 Live demo (safe, rehearsed)
1. **In-memory session:** `./build/titans --db :memory:` → menu `10` (load sample data) → `1` (list). *"14 books, 5 members, two with Arabic titles."*
2. **Search:** `4` → `1` → `apple` (title substring), then `4` → `4` → `fiction` (genre equality — note it does not match "Non-Fiction").
3. **UTF-8:** `4` → `1` → `كتاب الأول` — the table stays aligned.
4. **Borrow + invariant:** `5` → `9780451524935` → `1` (borrow 1984 for Alice), then the same borrow again → *"book is not available"*.
5. **Statistics:** `8` → shows available vs borrowed.
6. **Sort:** `7` → `1` → `2` → `2` (title, descending, insertion sort).
7. **Persistence:** exit, relaunch `--db demo.db`, list → the loan is still there.
8. **Tests:** `ctest --test-dir build -C Release --output-on-failure` (use `-C Release` on multi-config).

Prepared input file: `docs/demo_input.txt`. Terminal ≥ 110 columns, UTF-8 enabled (`chcp 65001`).

### 24.3 Timeline for a 10–12 minute defense
- 0:00–1:00 Pitch + problem + objectives (slides 1–3)
- 1:00–3:30 Architecture, components, container (slides 4–7) — spend the most time on the container
- 3:30–5:30 Patterns, domain model, persistence (slides 8–10)
- 5:30–7:00 Features + the borrow workflow (slides 11–12) — draw the three-step order
- 7:00–8:30 Testing, challenges, conclusion (slides 13–15)
- 8:30–10:00 Live demo
- 10:00+ Future work + Q&A (slide 16)

### 24.4 Board sketches worth rehearsing
- **The layer stack** (five boxes, arrows down).
- **The borrow sequence** (User → UI → Service → Repo → memory → UI).
- **The three-step order**: `validate → disk → memory`.
- **A list node** (`prev | data | next`) with head/tail.

---

## 25. Reviewer Q&A — rehearsed answers

**Q: Why your own container instead of `std::vector` or `std::list`?**
The requirement was a custom generic container with head/tail/indexed insertion, bidirectional
iteration and in-place removal. `Container<T>` gives O(1) operations at both ends and — crucially —
**stable node references**, meaning a `const Book&` handed out by the service stays valid even after
other elements are inserted, removed, or the list is sorted.

**Q: Why merge sort and not quicksort?**
Linked lists have no O(1) random access, so quicksort's partitioning is unnatural and hard to make
stable. My top-down merge sort splits and merges by rewiring pointers only — guaranteed O(n log n),
**zero allocations**, and **stable**.

**Q: What does "stable" buy the library?**
Multi-level sorting. Sort by title, then sort by genre, and within each genre the titles remain
alphabetical. That is exactly what a librarian expects and it is relied on in `test_library_scenarios.cpp`.

**Q: How do iterators survive insertions/removals?**
Because nodes are never relocated. Insertion adds a new node and rewires a few pointers; an existing
iterator only becomes invalid when *its own* node is removed (`unlink_and_delete`).

**Q: Why does the iterator need the container pointer?**
There is no circular dummy sentinel, so `end()` has a null node. To implement `--end()` (going back to
the last element) the iterator needs `owner_->tail_`. It also lets `next`/`prev` reject iterators from
a different container with `std::invalid_argument`.

**Q: What happens if a comparator throws during a sort?**
`Container::sort` catches it, calls `relink` to restore valid `prev`/`next` links across all nodes,
and rethrows. Basic guarantee: nothing lost, no leak, order unspecified. It is tested.

**Q: Why is a `Book`'s identity just its ISBN?**
`Container::find`/`remove_value` use `operator==`. Defining identity as the ISBN means lookups work
even after a book's title, author, genre or availability changes — and duplicates are detected by ISBN.

**Q: Why no default constructor for `Book`?**
A book cannot exist in an uninitialised, invalid state. The constructor validates and trims
immediately, so a `Book` always satisfies its invariants.

**Q: Why is availability not a column in the `books` table?**
Storing `is_available` next to `loans` would create two copies of the same fact and let them drift.
Instead availability is *derived*: a book is borrowed iff there is an open loan (`returned_at IS NULL`).
The database also enforces at most one open loan per book with a partial unique index.

**Q: How do you prevent SQL injection?**
Every statement is prepared with `sqlite3_prepare_v2` and every value is bound via
`sqlite3_bind_text`/`sqlite3_bind_int`. SQL is never built by concatenation.

**Q: What happens if someone edits the database by hand and corrupts it?**
On startup `LibraryService::open` replays the data through the public API. Duplicate ISBNs, bad ids or
conflicting loans cause validation to throw, and the error is wrapped as
`DatabaseError("database contents are inconsistent: …")` — the app refuses to start broken.

**Q: Why write to the database before changing memory?**
That ordering gives the strong exception guarantee. If the disk write fails, memory is untouched, so
the object is exactly as it was. If you mutated memory first and the disk then failed, the two would
disagree.

**Q: Why inject streams into the UI?**
So the entire interactive layer can be tested headlessly with `std::istringstream`/`std::ostringstream` —
menu navigation, validation loops and `/cancel` are all covered by automated tests.

**Q: How do you handle Arabic / UTF-8?**
Strings are UTF-8. Case folding is ASCII-only, so multi-byte sequences are preserved byte-for-byte.
The table formatter measures widths in **code points** so column borders stay aligned; right-to-left
shaping is left to the terminal. This is covered by `test_ui_text.cpp`.

**Q: What did you deliberately leave out?**
Threading, authentication/roles, renewals and fines, and checksum-level ISBN validation. Leaving them
out kept the design verifiable and free of undefined behaviour; the layering means they are natural
extensions (see `slide 16`).

**Q: Is your container faster than `std::list`?**
No, and it does not need to be. The benchmark shows equivalent linear/allocation-bound behaviour and
correct sort asymptotics. The value is that it is mine, it is exercised, and its guarantees are
documented.

---

## 26. Glossary

| Term | Meaning in this project |
|---|---|
| **Container** | `titans::Container<T>` — the custom doubly linked list |
| **Node** | A heap object holding one element plus `prev`/`next` links |
| **Invariant** | A rule that is true after every public call (I1–I4) |
| **Strong exception guarantee** | On failure, the object is unchanged |
| **Basic exception guarantee** | On failure, no resources leak and the object stays valid (order may differ) |
| **Write-ahead** | Persist to the repository before mutating in-memory state |
| **Facade** | `LibraryService` — one API hiding many parts |
| **Strategy** | Pluggable search/sort behaviour behind an interface |
| **Repository** | Storage abstraction (`ILibraryRepository`) |
| **Null Object** | `NullRepository` — a real object that does nothing |
| **Open loan** | A `loans` row with `returned_at IS NULL`; the source of truth for availability |
| **Stable sort** | Equal keys keep their previous relative order |
| **Optimistic vs derived state** | Availability is *derived* from loans, never stored redundantly |

---

*End of documentation. For the project overview see `README.md`; for diagrams see
`docs/ARCHITECTURE.md`; for rationale see `docs/DESIGN_DECISIONS.md`; for the slide deck see
`docs/presentation.html`.*

# Architecture Decision Records and Reviewer FAQ

This document captures the design rationale, architecture decision records (ADRs), trade-offs, and reviewer questions for the Titans Library Management System.

---

## Part 1: Architecture Decision Records (ADRs)

### ADR-01: Custom Doubly Linked List (`titans::Container<T>`)
- **Context:** The system required a custom generic data structure supporting element insertion at head, tail, and arbitrary indices, bidirectional traversal, and in-place element removal without utilizing standard library sequence containers.
- **Decision:** Implemented a doubly linked list with heap-allocated `Node` structures containing `prev` and `next` raw pointers, alongside cached `head_`, `tail_`, and `size_` members.
- **Alternatives Considered:** Contiguous dynamic array (`std::vector`-like buffer with doubling reallocation) or singly linked list.
- **Consequence:** Yields $O(1)$ push/pop at both ends and node pointer stability across insertions and deletions. Increases per-element heap overhead (two pointers per node) and forfeits CPU cache spatial locality compared to contiguous arrays.

### ADR-02: Iterators Storing Container Owner Pointer
- **Context:** Forward and bidirectional iterators must allow decrementing from past-the-end (`end()`) and detect invalid cross-container iterator operations.
- **Decision:** Iterators store both a pointer to the current `Node*` and a pointer to the owning `const Container*`.
- **Alternatives Considered:** Storing only a `Node*` and a separate sentinel dummy node at list end, or raw node pointers without container validation.
- **Consequence:** Decrementing `end()` safely retrieves `owner_->tail_` in $O(1)$ time without requiring a permanent sentinel node allocation. Comparing or operating on iterators from differing containers throws `std::invalid_argument`. Iterator object footprint is 16 bytes rather than 8 bytes.

### ADR-03: In-Place Node-Relinking Merge Sort and Stability
- **Context:** The catalog requires sorting by various criteria (title, author, genre) while maintaining stable relative ordering among equal keys.
- **Decision:** Implemented a recursive top-down merge sort (`Container::sort`) that splits and merges linked list nodes solely by relinking pointers (`next` and `prev`), never copying, moving, allocating, or destroying element values.
- **Alternatives Considered:** Copying elements to an auxiliary array, sorting via `std::sort` (quicksort/introsort), and writing back; or list quicksort.
- **Consequence:** Guarantees stable $O(n \log n)$ time with $O(\log n)$ call stack space and zero dynamic allocations. Node memory addresses, external references, and active iterators remain strictly valid across sorts.

### ADR-04: Strategy Pattern and Parameterized Factories
- **Context:** Book sorting and searching require multiple distinct criteria (title, author, ISBN, genre) while remaining open for extension without modifying core service logic.
- **Decision:** Created `ISortStrategy` and `ISearchStrategy` abstract interfaces. Concrete strategies are instantiated via static factory methods (`SortStrategyFactory::create` and `SearchStrategyFactory::create`) using string keys.
- **Alternatives Considered:** Hardcoded `switch` statements and conditional branching inside `LibraryService`, or passing raw lambda comparators directly from the UI.
- **Consequence:** Decouples user-facing criteria names from algorithm execution. Adheres strictly to the Open-Closed Principle (OCP), incurring a negligible virtual function dispatch overhead.

### ADR-05: ISBN as the Singular Identity of a Book
- **Context:** Container search and removal operations (`Container::find`, `Container::remove_value`) rely on `operator==` to identify elements.
- **Decision:** Defined `Book::operator==` to compare normalized ISBNs exclusively. Other attributes (title, author, genre, availability) are ignored for identity checks.
- **Alternatives Considered:** Value equality comparing all member fields, or introducing an artificial surrogate integer primary key.
- **Consequence:** Matches the real-world library domain where distinct book editions or copies share canonical ISBN identities. Updating a book's title or borrowing status does not invalidate identity lookups.

### ADR-06: Service Invariants and Strong Exception Safety
- **Context:** Service methods mutating state (`borrow_book`, `return_book`, `add_book`, `remove_book`) must guarantee consistency even when disk writes or memory allocations fail.
- **Decision:** Enforced four formal invariants (I1: unique ISBNs; I2: unique member IDs; I3: valid loan relationships; I4: non-negative counts). Mutations follow the write-ahead idiom: validate domain rules, write to repository, then mutate in-memory state.
- **Alternatives Considered:** Mutating in-memory data first and attempting rollback if persistent storage throws.
- **Consequence:** Provides the Strong Exception Guarantee: if repository operations or validation throw, the in-memory state remains completely unchanged. (The single documented corner-case is `std::bad_alloc` occurring during the subsequent in-memory container update).

### ADR-07: Write-Ahead Persistence and Derived Availability
- **Context:** Persistent storage in SQLite must mirror in-memory loan states without data redundancy or contradictory states.
- **Decision:** Availability is never persisted as an independent column in the `books` table. A book is considered borrowed if and only if an open record exists in the `loans` table. Mutations write through to SQLite before updating memory.
- **Alternatives Considered:** Persisting an `is_available` boolean column in `books` alongside `loans` records, or periodic batched flushes.
- **Consequence:** Eliminates dual-state synchronization bugs by maintaining a single source of truth in SQLite. Foreign keys and partial unique indices (`WHERE returned_at IS NULL`) guarantee at the database engine level that no book can be concurrently loaned twice.

### ADR-08: Null Object Pattern for Repository
- **Context:** Unit tests and temporary in-memory sessions require running without creating disk files or touching SQLite.
- **Decision:** Implemented `NullRepository` derived from `ILibraryRepository`, where write operations are no-ops and `load()` returns an empty dataset.
- **Alternatives Considered:** Allowing `nullptr` for the repository pointer inside `LibraryService` guarded by conditional checks (`if (repository_)`).
- **Consequence:** Simplifies service code by eliminating null checks across all persistence call sites. Unit tests run purely in-memory at high speed.

### ADR-09: Dependency Injection of Streams in UI
- **Context:** The console user interface must be fully testable without manual user keyboard input or hardware terminal hooks.
- **Decision:** Injected `std::istream&` and `std::ostream&` into `ConsoleApp` and `InputReader` constructors.
- **Alternatives Considered:** Directly invoking `std::cin` and `std::cout` throughout UI methods.
- **Consequence:** Enables 100% automated headless testing of user sessions, invalid input loops, and edge cases using `std::istringstream` and `std::ostringstream`.

### ADR-10: Bundled SQLite Amalgamation
- **Context:** Cross-platform database support was required without burdening users with external library installations or package managers.
- **Decision:** Integrated SQLite 3.53.4 amalgamation (`sqlite3.c` and `sqlite3.h`) under `third_party/sqlite/` compiled directly into `sqlite3_c` static library target.
- **Alternatives Considered:** Linking against system SQLite packages or fetching via CMake FetchContent/vcpkg.
- **Consequence:** Ensures 100% deterministic builds across Windows, Linux, and macOS without requiring package managers or external internet access for the database engine.

### ADR-11: Explicit Scope Boundaries (Deliberately Omitted Features)
- **Context:** Maintaining code clarity, verifiable correctness, and zero memory leaks within a clean academic C++17 architecture.
- **Decision:** Deliberately excluded multi-threading, asynchronous workers, network sockets, user authentication/roles, and automated overdue fine calculations.
- **Alternatives Considered:** Introducing mutex locks, thread pools, and complex enterprise accounting frameworks.
- **Consequence:** Guarantees clean, deterministic execution, zero data races, zero undefined behavior, and an easily auditable codebase.

---

## Part 2: Questions a Reviewer May Ask

#### Q1: Why use a custom doubly linked list instead of `std::vector`?
**A:** The project requirements explicitly demanded a custom generic sequence container implementing head/tail/indexed insertions, deletions, and custom bidirectional iterators. A doubly linked list (`titans::Container<T>`, `core/Container.h`) provides $O(1)$ push/pop operations at both ends and guarantees node reference stability when other nodes are added or removed, as verified in `tests/test_container.cpp`.

#### Q2: Why use merge sort instead of quicksort for the container?
**A:** Linked lists lack $O(1)$ random access, making pivot partitioning in quicksort inefficient and non-trivial to execute stably. Merge sort (`Container::sort`, `core/Container.h:L575-L594`) splits and merges chains purely by pointer manipulation in guaranteed $O(n \log n)$ worst-case time without allocating auxiliary memory, as proven in `tests/test_algorithms.cpp`.

#### Q3: What is the operational difference between the two sorting algorithms in Container?
**A:** `sort()` is a recursive top-down merge sort with guaranteed $O(n \log n)$ time and $O(\log n)$ stack space. `insertion_sort()` (`core/Container.h:L596-L635`) maintains a sorted sub-chain and inserts nodes one by one; it runs in quadratic $O(n^2)$ time on random data but achieves linear $O(n)$ time on already-sorted or nearly-sorted data. Both algorithms are stable and operate strictly by relinking pointers.

#### Q4: What does "stable sort" mean, and why is it critical for the library catalog?
**A:** A sort is stable if it preserves the relative order of elements that have equal sorting keys. In `LibraryService::sort_books`, stability allows multi-level sorting: a librarian can first sort the catalog by title ascending, and then sort by genre. All books within the same genre remain sorted alphabetically by title (`tests/test_library_scenarios.cpp`).

#### Q5: How do `Container` iterators survive element insertions and removals?
**A:** Because `Container` is a node-based linked list, inserting a new element allocates a new node and relinks surrounding pointers without relocating existing nodes in memory (`core/Container.h:L97-L114`). Consequently, existing iterators pointing to other nodes remain completely valid; an iterator only becomes invalid if the specific node it points to is destroyed (`unlink_and_delete`).

#### Q6: Why does `end()` need an owner pointer in `BasicIterator`?
**A:** In a doubly linked list without a circular dummy sentinel, the `end()` iterator has `current_ == nullptr`. To support bidirectional iteration, decrementing `end()` (`--it`) must navigate to the list's last node (`tail_`), which requires access to the container instance (`owner_->tail_`), as shown in `core/Container.h:L388-L395` and tested in `tests/test_iterator.cpp`.

#### Q7: What guarantee is provided if a sort comparator throws an exception during execution?
**A:** Both `Container::sort` and `Container::insertion_sort` offer the basic exception guarantee (`core/Container.h:L537-L546`). If the user-supplied comparator throws, `Container::sort` catches the exception, invokes `relink(chain)` to restore valid bidirectional links across all nodes, and rethrows. No nodes or elements are leaked or lost, though the elements remain in an unspecified partial order (`tests/test_algorithms.cpp`).

#### Q8: Why does `Book::operator==` compare only the ISBN?
**A:** In domain-driven design, an entity's identity is distinguished from its attributes. A book's identity is uniquely determined by its ISBN (`domain/Book.h:L55-L60`). Comparing only ISBN allows `Container::find` and `Container::remove_value` to locate books in the catalog regardless of changes to their title, author, genre, or borrowing status (`tests/test_book.cpp`).

#### Q9: Why is there no default constructor for `Book`?
**A:** A `Book` cannot exist in an uninitialized, invalid state. The constructor `Book(title, author, isbn, genre, available)` enforces domain invariants immediately upon instantiation by trimming fields and validating that title, author, and genre are non-blank and that the ISBN is valid (`domain/Book.h:L8-L18`). There are no "blank" books allowed.

#### Q10: How does the system prevent SQL injection?
**A:** `SqliteRepository` (`repository/SqliteRepository.cpp`) uses prepared statements (`sqlite3_prepare_v2`) for every database query. All user inputs (titles, authors, ISBNs, member names, and IDs) are bound using `sqlite3_bind_text` and `sqlite3_bind_int`. No string concatenation is ever used to construct SQL statements, completely preventing SQL injection.

#### Q11: Why is book availability not stored as a database column in `books`?
**A:** Storing an `is_available` flag in `books` alongside open loan records in `loans` would create redundant state and the potential for synchronization divergence. In `SqliteRepository`, availability is derived solely from the presence of an open loan (`returned_at IS NULL`) in `loans` (`repository/SqliteRepository.cpp:L25-L35`). This guarantees a single source of truth.

#### Q12: What happens if an external process modifies the SQLite database file by hand?
**A:** At startup, `LibraryService::open` replays the database state through the public API (`LibraryService::open`, `service/LibraryService.cpp`). If external tampering introduced duplicate ISBNs, invalid member IDs, or conflicting open loans, the validation rules in `add_book`, `add_member`, or `borrow_book` will immediately throw, aborting startup and preserving system integrity.

#### Q13: Why does `LibraryService` write to the repository before modifying in-memory state?
**A:** This implements the write-ahead pattern to provide the strong exception guarantee (`service/LibraryService.cpp:L166-L185`). If disk write fails (e.g. SQLite database locked, disk full, or constraint violation), an exception is raised while in-memory structures remain completely untouched. If memory were modified first, a disk failure would leave memory desynchronized from persistent storage.

#### Q14: Why does the user interface accept injected streams instead of using `std::cin` and `std::cout`?
**A:** Injecting `std::istream&` and `std::ostream&` into `ConsoleApp` and `InputReader` (`ui/ConsoleApp.h`, `ui/InputReader.h`) decouples the presentation logic from physical terminal hardware. This allows the entire interactive UI—including menu navigation, input validation loops, and cancel commands—to be thoroughly tested with automated in-memory streams (`tests/test_console_app.cpp`).

#### Q15: How does the system handle Arabic and UTF-8 multi-byte text?
**A:** Strings are stored as UTF-8 `std::string` buffers. Text utilities (`domain/Text.h`) perform case folding exclusively on ASCII bytes ($< 128$), leaving multi-byte UTF-8 sequences intact. Table formatting (`ui/Table.cpp`) calculates display widths based on UTF-8 code points (`ui/Utf8.h`), ensuring aligned column borders for Arabic titles such as "كتاب الأول" (`tests/test_ui_text.cpp`).

#### Q16: What features are deliberately not supported by the system?
**A:** The system intentionally does not implement multi-threaded concurrent access, network protocols, user authentication/passwords, loan renewal limits, or automated overdue fine calculation. These omissions keep the architecture focused, clean, verifiable, and free of unnecessary third-party dependencies.

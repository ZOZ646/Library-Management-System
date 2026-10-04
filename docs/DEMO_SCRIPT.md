# Demonstration Video Script

This document provides a timed script for recording a walkthrough video of the Titans Library Management System. Two timelines are provided: a full 3-minute presentation and a condensed 60-second lightning overview.

## Recording Tips and Setup

1. **Terminal Configuration:**
   - Width: At least 110 columns (recommended: $120 \times 35$) to ensure ASCII tables render without word wrapping.
   - Font: Clear monospaced font (Cascadia Code, JetBrains Mono, or Consolas) at 16pt or larger.
   - Character Encoding: Ensure the terminal is configured for UTF-8 (`chcp 65001` on Windows CMD, or modern Windows Terminal / Linux / macOS terminal).
2. **Environment Cleanliness:**
   - Clear terminal scrollback and command history (`clear` or `cls`).
   - Remove any previous `demo.db` file so persistence demonstrations begin fresh.
   - Rehearse the typed inputs once using `docs/demo_input.txt` before recording.
3. **Execution Mode:**
   - Type commands interactively or paste them smoothly. Pacing should remain deliberate and easy to follow.

---

## 3-Minute Video Script

| Time | On Screen / What to Type | What to Say |
|---|---|---|
| **0:00 - 0:15** (15s) | Show repository root, terminal banner, and project title. Run `git status`. | "Welcome to Titans, a C++17 library management system built around a custom generic doubly linked list container and persistent SQLite storage." |
| **0:15 - 0:35** (20s) | Display directory tree (`tree` or `ls core/ domain/ service/ repository/ ui/`). Briefly open `core/Container.h`. | "The architecture is structured in clean layers. At the foundation is `titans::Container<T>`, providing bidirectional iterators, custom merge sort, and insertion sort without standard container dependencies." |
| **0:35 - 0:50** (15s) | Launch transient session:<br>`./build/titans --db :memory:`<br>Type `10` to load demo data, then `1` to list all books. | "We start by launching an in-memory session. Loading the sample dataset populates 14 books and 5 members into our custom container." |
| **0:50 - 1:10** (20s) | Type `4` -> `1` -> `apple` (title search).<br>Then type `4` -> `4` -> `fiction` (genre search). | "Searching by title performs case-insensitive substring matching. Searching by genre uses exact matching, correctly isolating fiction from non-fiction titles." |
| **1:10 - 1:25** (15s) | Type `4` -> `1` -> `كتاب الأول` (search Arabic book). | "UTF-8 multi-byte characters such as Arabic are supported natively throughout the container and table formatting." |
| **1:25 - 1:45** (20s) | Type `5` -> `9780451524935` -> `1` (borrow '1984').<br>Type `5` -> `9780451524935` -> `2` (attempt double borrow). | "Borrowing book 1984 assigns it to member 1. Attempting to borrow the same book again fails immediately with a domain invariant exception." |
| **1:45 - 2:05** (20s) | Type `8` (check statistics).<br>Type `7` -> `1` -> `2` -> `2` (sort by title descending with insertion sort).<br>Type `0` (exit). | "Library statistics reflect current availability. Sorting relinks node pointers stably in-place using our insertion sort algorithm." |
| **2:05 - 2:30** (25s) | Persistence demo:<br>`./build/titans --db demo.db`<br>Type `10` (load data), `5` -> `9780451524935` -> `1` (borrow), then `0` (exit).<br>Relaunch: `./build/titans --db demo.db`<br>Type `1` (list books). | "Now we test persistence. We borrow a book in a file-backed database and exit. Relaunching the application reloads the exact loan state directly through the repository." |
| **2:30 - 2:50** (20s) | Run test suite:<br>`ctest --test-dir build --output-on-failure`<br>Show test results passing with AddressSanitizer. | "Every feature is backed by comprehensive unit tests, including randomized scenarios and leak checks under AddressSanitizer and UndefinedBehaviorSanitizer." |
| **2:50 - 3:00** (10s) | Return to project architecture summary. | "Titans demonstrates robust modern C++ design, strict separation of concerns, and guaranteed data integrity throughout." |

---

## 60-Second Lightning Script

| Time | On Screen / What to Type | What to Say |
|---|---|---|
| **0:00 - 0:10** (10s) | Terminal banner, short view of `core/Container.h`. | "Titans is a C++17 library management system built upon a custom generic doubly linked list and SQLite persistence." |
| **0:10 - 0:25** (15s) | `./build/titans --db :memory:`<br>Type `10` (load data), `1` (list books), `4` -> `1` -> `apple` (search). | "Data is organized in our custom container, supporting fast tabular display, case-insensitive search, and UTF-8 Arabic text." |
| **0:25 - 0:40** (15s) | Type `5` -> `9780451524935` -> `1` (borrow).<br>Type `7` -> `1` -> `1` -> `1` (merge sort).<br>Type `0` (exit). | "Borrowing operations enforce strict domain invariants, while sorting relinks nodes with stable $O(n \log n)$ merge sort." |
| **0:40 - 0:55** (15s) | `./build/titans --db demo.db` (borrow, exit, relaunch, show persistent loan status). | "The repository pattern ensures write-ahead persistence to SQLite, keeping disk and memory perfectly synchronized." |
| **0:55 - 1:00** (5s) | Run `ctest --test-dir build`. Show 100% pass under sanitizers. | "Fully tested under AddressSanitizer with zero memory leaks." |

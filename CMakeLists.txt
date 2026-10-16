cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
project(Titans LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})

# SQLite is created before our warning and sanitizer flags because add_compile_options only affects targets created after it
add_library(sqlite3_c STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3_c PUBLIC ${CMAKE_SOURCE_DIR}/third_party/sqlite)
target_compile_definitions(sqlite3_c PRIVATE SQLITE_THREADSAFE=0 SQLITE_OMIT_LOAD_EXTENSION SQLITE_DQS=0)

# Sanitizers are enabled by default only where the runtime exists: GCC and Clang on Linux and macOS.
# MSVC and MinGW/MSYS2 on Windows ship no ASan/UBSan runtime, so there the option defaults to OFF.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
  set(TITANS_SANITIZE_DEFAULT ON)
else()
  set(TITANS_SANITIZE_DEFAULT OFF)
endif()
option(TITANS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" ${TITANS_SANITIZE_DEFAULT})

if(TITANS_SANITIZE AND WIN32)
  message(WARNING "TITANS_SANITIZE is not supported on Windows toolchains; disabling it")
  set(TITANS_SANITIZE OFF CACHE BOOL "" FORCE)
endif()
if(NOT MSVC)
  add_compile_options(-Wall -Wextra)
  if(TITANS_SANITIZE)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

# MinGW/MSYS2 binaries otherwise depend on libstdc++/libgcc/libwinpthread DLLs
# that are not on PATH outside a developer shell, so the .exe fails to start.
if(MINGW)
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

add_library(titans_lib STATIC domain/Book.cpp domain/Member.cpp service/SortStrategy.cpp service/SearchStrategy.cpp service/LibraryService.cpp ui/Table.cpp ui/InputReader.cpp ui/DemoData.cpp ui/ConsoleApp.cpp repository/SqliteRepository.cpp)
target_link_libraries(titans_lib PUBLIC sqlite3_c)
add_executable(titans main.cpp)
target_link_libraries(titans PRIVATE titans_lib)

# doctest v2.4.11 — bundled single header (no network dependency)
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party/doctest)
add_library(doctest::doctest ALIAS doctest)

add_executable(titans_tests tests/test_main.cpp tests/test_container.cpp tests/test_iterator.cpp tests/test_algorithms.cpp tests/test_book.cpp tests/test_member.cpp tests/test_text.cpp tests/test_strategies.cpp tests/test_library_service.cpp tests/test_library_queries.cpp tests/test_library_scenarios.cpp tests/test_ui_text.cpp tests/test_input_reader.cpp tests/test_console_app.cpp tests/test_console_sessions.cpp tests/test_sqlite_repository.cpp tests/test_service_repository.cpp tests/test_persistence.cpp)
target_link_libraries(titans_tests PRIVATE titans_lib doctest::doctest)
option(TITANS_BUILD_BENCH "Build the container benchmark" OFF)
if(TITANS_BUILD_BENCH)
  add_executable(titans_bench bench/bench_container.cpp)
  target_compile_definitions(titans_bench PRIVATE TITANS_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
endif()

enable_testing()
add_test(NAME titans_tests COMMAND titans_tests)


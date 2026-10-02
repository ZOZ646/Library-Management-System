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

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(titans_lib STATIC domain/Book.cpp domain/Member.cpp service/SortStrategy.cpp service/SearchStrategy.cpp service/LibraryService.cpp ui/Table.cpp ui/InputReader.cpp ui/DemoData.cpp ui/ConsoleApp.cpp repository/SqliteRepository.cpp)
target_link_libraries(titans_lib PUBLIC sqlite3_c)
add_executable(titans main.cpp)
target_link_libraries(titans PRIVATE titans_lib)

include(FetchContent)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11)
FetchContent_MakeAvailable(doctest)

add_executable(titans_tests tests/test_main.cpp tests/test_container.cpp tests/test_iterator.cpp tests/test_algorithms.cpp tests/test_book.cpp tests/test_member.cpp tests/test_text.cpp tests/test_strategies.cpp tests/test_library_service.cpp tests/test_library_queries.cpp tests/test_library_scenarios.cpp tests/test_ui_text.cpp tests/test_input_reader.cpp tests/test_console_app.cpp tests/test_console_sessions.cpp tests/test_sqlite_repository.cpp tests/test_service_repository.cpp tests/test_persistence.cpp)
target_link_libraries(titans_tests PRIVATE titans_lib doctest::doctest)

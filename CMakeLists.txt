cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
project(Titans LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

add_executable(titans main.cpp)

include(FetchContent)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11)
FetchContent_MakeAvailable(doctest)

add_executable(titans_tests tests/test_main.cpp tests/test_container.cpp tests/test_iterator.cpp tests/test_algorithms.cpp)
target_link_libraries(titans_tests doctest::doctest)

cmake_minimum_required(VERSION 3.20)
project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

include_directories(include)

# Build the core into a separate library
add_library(minidb_core
    src/Database.cpp 
    src/Repl.cpp
    src/Pager.cpp
    src/Page.cpp
)

# Main executable
add_executable(minidb_cli src/main.cpp)
target_link_libraries(minidb_cli PRIVATE minidb_core)

# --- GoogleTest setup ---
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- Tests ---
add_executable(test_pager tests/test_pager.cpp)
target_link_libraries(test_pager PRIVATE minidb_core GTest::gtest_main)
add_test(NAME PagerTest COMMAND test_pager)

add_executable(test_page tests/test_page.cpp)
target_link_libraries(test_page PRIVATE minidb_core GTest::gtest_main)
add_test(NAME PageTest COMMAND test_page)

add_executable(test_database tests/test_database.cpp)
target_link_libraries(test_database PRIVATE minidb_core GTest::gtest_main)
add_test(NAME DatabaseTest COMMAND test_database)
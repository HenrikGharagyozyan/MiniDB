cmake_minimum_required(VERSION 3.20)
project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# --- Core Library ---
# Renamed to minidb_core to match the other targets
add_library(minidb_core
    src/storage/Page.cpp
    src/storage/Pager.cpp
    src/storage/Wal.cpp
    src/buffer/BufferPoolManager.cpp
    src/buffer/LRUReplacer.cpp
    src/index/BTree.cpp
    src/index/BTreeIterator.cpp
    src/concurrency/LockManager.cpp
    src/concurrency/ReadView.cpp
    src/concurrency/TransactionManager.cpp
    src/sql/Tokenizer.cpp
    src/sql/Parser.cpp
    src/core/Database.cpp
    src/sql/Executor.cpp
)

target_include_directories(minidb_core PUBLIC include)

# --- CLI Executable ---
add_executable(minidb_cli src/cli/Repl.cpp src/cli/main.cpp)
target_link_libraries(minidb_cli PRIVATE minidb_core)

# --- Testing Setup ---
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(tests)
endif()
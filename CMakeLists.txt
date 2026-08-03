cmake_minimum_required(VERSION 3.20)
project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compile options for safety and warnings
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

include_directories(include)

# Executable for future CLI/REPL
add_executable(MiniDB_cli src/main.cpp)
cmake_minimum_required(VERSION 3.20)
project(MiniDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

include_directories(include)

add_executable(minidb_cli 
    src/main.cpp 
    src/Database.cpp
    src/Repl.cpp
)
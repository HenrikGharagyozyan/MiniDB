# --- Unit Tests Target Definitions ---

add_executable(test_pager test_pager.cpp)
target_link_libraries(test_pager PRIVATE minidb_core GTest::gtest_main)
add_test(NAME PagerTest COMMAND test_pager)

add_executable(test_page test_page.cpp)
target_link_libraries(test_page PRIVATE minidb_core GTest::gtest_main)
add_test(NAME PageTest COMMAND test_page)

add_executable(test_database test_database.cpp)
target_link_libraries(test_database PRIVATE minidb_core GTest::gtest_main)
add_test(NAME DatabaseTest COMMAND test_database)

add_executable(test_wal test_wal.cpp)
target_link_libraries(test_wal PRIVATE minidb_core GTest::gtest_main)
add_test(NAME WalTest COMMAND test_wal)

add_executable(test_btree test_btree.cpp)
target_link_libraries(test_btree PRIVATE minidb_core GTest::gtest_main)
add_test(NAME BTreeTest COMMAND test_btree)

add_executable(test_lru_replacer test_lru_replacer.cpp)
target_link_libraries(test_lru_replacer PRIVATE minidb_core GTest::gtest_main)
add_test(NAME LRUReplacerTest COMMAND test_lru_replacer)
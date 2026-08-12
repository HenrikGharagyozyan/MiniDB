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

add_executable(test_buffer_pool_manager test_buffer_pool_manager.cpp)
target_link_libraries(test_buffer_pool_manager PRIVATE minidb_core gtest_main)
add_test(NAME BufferPoolManagerTest COMMAND test_buffer_pool_manager)

add_executable(test_lock_manager test_lock_manager.cpp)
target_link_libraries(test_lock_manager PRIVATE minidb_core gtest gtest_main)
add_test(NAME LockManager COMMAND test_lock_manager)

add_executable(test_2pl test_2pl.cpp)
target_link_libraries(test_2pl PRIVATE minidb_core gtest gtest_main)
add_test(NAME 2PL COMMAND test_2pl)

add_executable(test_read_view test_read_view.cpp)
target_link_libraries(test_read_view PRIVATE minidb_core GTest::gtest_main)
add_test(NAME ReadViewTest COMMAND test_read_view)

add_executable(test_mvcc test_mvcc.cpp)
target_link_libraries(test_mvcc PRIVATE minidb_core GTest::gtest_main)
add_test(NAME MVCCTest COMMAND test_mvcc)

add_executable(test_sql_parser test_sql_parser.cpp)
target_link_libraries(test_sql_parser PRIVATE minidb_core gtest gtest_main)
add_test(NAME test_sql_parser COMMAND test_sql_parser)

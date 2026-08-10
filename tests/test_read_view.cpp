#include <gtest/gtest.h>
#include "minidb/ReadView.h"

using namespace minidb;

TEST(ReadViewTest, VisibilityRules) 
{
    // Model the situation:
    // Our transaction has ID = 10 (creator_txn_id)
    // At the moment we start, transactions 5 and 8 are active.
    // The next free ID that TransactionManager will assign is 12 (max_txn_id)
    //
    // Inside ReadView, the minimum ID (min_txn_id) should be computed as 5.
    
    txn_id_t my_txn_id = 10;
    std::unordered_set<txn_id_t> active_txns = {5, 8};
    txn_id_t next_txn_id = 12;

    ReadView view(my_txn_id, active_txns, next_txn_id);

    // Rule 1: We always see our own changes
    EXPECT_TRUE(view.is_visible(10));

    // Rule 2: We see everything that was committed before the oldest active transaction (ID < 5)
    // (They finished long before we started)
    EXPECT_TRUE(view.is_visible(2));
    EXPECT_TRUE(view.is_visible(4));

    // Rule 3: We do NOT see changes from the future (ID >= 12)
    EXPECT_FALSE(view.is_visible(12));
    EXPECT_FALSE(view.is_visible(20));

    // Rule 4a: We do NOT see changes from transactions that were active when we started
    EXPECT_FALSE(view.is_visible(5));
    EXPECT_FALSE(view.is_visible(8));

    // Rule 4b: We DO see changes from transactions in the range 5 to 12
    // that are not in the active set. That means they already committed.
    // (for example: 6, 7, 9, 11)
    EXPECT_TRUE(view.is_visible(6));
    EXPECT_TRUE(view.is_visible(7));
    EXPECT_TRUE(view.is_visible(9));
    EXPECT_TRUE(view.is_visible(11));
}
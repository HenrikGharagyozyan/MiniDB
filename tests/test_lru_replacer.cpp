#include <gtest/gtest.h>
#include "minidb/buffer/LRUReplacer.h"

TEST(LRUReplacerTest, BasicTest) 
{
    minidb::LRUReplacer lru(3);

    // Add (unpin) frames 1, 2, 3. They become eviction candidates.
    lru.unpin(1);
    lru.unpin(2);
    lru.unpin(3);
    EXPECT_EQ(lru.size(), 3);

    minidb::FrameId victim_id;

    // Frame 1 was added first, so it is the oldest. Evict it.
    EXPECT_TRUE(lru.victim(&victim_id));
    EXPECT_EQ(victim_id, 1);
    EXPECT_EQ(lru.size(), 2);

    // Now somebody requested frame 2. We pin it.
    lru.pin(2);
    EXPECT_EQ(lru.size(), 1);

    // Try evicting the next one. It should be frame 3 because 2 is pinned.
    EXPECT_TRUE(lru.victim(&victim_id));
    EXPECT_EQ(victim_id, 3);
    EXPECT_EQ(lru.size(), 0);

    // There is nothing left to evict.
    EXPECT_FALSE(lru.victim(&victim_id));

    // Return frame 2 to the cache
    lru.unpin(2);
    EXPECT_EQ(lru.size(), 1);
    EXPECT_TRUE(lru.victim(&victim_id));
    EXPECT_EQ(victim_id, 2);
}
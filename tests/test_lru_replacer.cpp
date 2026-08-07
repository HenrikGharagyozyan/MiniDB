#include <gtest/gtest.h>
#include "minidb/LRUReplacer.h"

TEST(LRUReplacerTest, BasicTest) 
{
    minidb::LRUReplacer lru(3);

    // Добавляем (unpin) фреймы 1, 2, 3. Они становятся кандидатами на удаление.
    lru.unpin(1);
    lru.unpin(2);
    lru.unpin(3);
    EXPECT_EQ(lru.size(), 3);

    minidb::FrameId victim_id;

    // Фрейм 1 был добавлен первым, значит он самый "старый". Выкидываем его.
    EXPECT_TRUE(lru.victim(&victim_id));
    EXPECT_EQ(victim_id, 1);
    EXPECT_EQ(lru.size(), 2);

    // Теперь кто-то запросил фрейм 2. Мы делаем ему pin.
    lru.pin(2);
    EXPECT_EQ(lru.size(), 1);

    // Пробуем выкинуть следующий. Это должен быть фрейм 3, так как 2 закреплен (pinned).
    EXPECT_TRUE(lru.victim(&victim_id));
    EXPECT_EQ(victim_id, 3);
    EXPECT_EQ(lru.size(), 0);

    // Выкидывать больше нечего.
    EXPECT_FALSE(lru.victim(&victim_id));

    // Возвращаем фрейм 2 в кэш
    lru.unpin(2);
    EXPECT_EQ(lru.size(), 1);
    EXPECT_TRUE(lru.victim(&victim_id));
    EXPECT_EQ(victim_id, 2);
}
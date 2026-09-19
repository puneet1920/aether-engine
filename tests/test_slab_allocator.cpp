#include "aether/PriceLevel.hpp"
#include "aether/SlabAllocator.hpp"
#include <gtest/gtest.h>

struct TestNode {
    uint64_t a{0};
    uint64_t b{0};
    TestNode(uint64_t x, uint64_t y) : a(x), b(y) {}
};

TEST(SlabAllocatorTest, InitialState) {
    aether::SlabAllocator<TestNode, 16> pool;
    EXPECT_EQ(pool.capacity(), 16u);
    EXPECT_EQ(pool.available(), 16u);
    EXPECT_EQ(pool.inUse(), 0u);
}

TEST(SlabAllocatorTest, AllocateAndDeallocate) {
    aether::SlabAllocator<TestNode, 4> pool;

    TestNode* n1 = pool.allocate(10, 20);
    ASSERT_NE(n1, nullptr);
    EXPECT_EQ(n1->a, 10u);
    EXPECT_EQ(n1->b, 20u);
    EXPECT_EQ(pool.inUse(), 1u);
    EXPECT_EQ(pool.available(), 3u);
    EXPECT_TRUE(pool.owns(n1));

    pool.deallocate(n1);
    EXPECT_EQ(pool.inUse(), 0u);
    EXPECT_EQ(pool.available(), 4u);
}

TEST(SlabAllocatorTest, PoolExhaustion) {
    constexpr size_t kCap = 4;
    aether::SlabAllocator<TestNode, kCap> pool;

    std::vector<TestNode*> nodes;
    for (size_t i = 0; i < kCap; ++i) {
        TestNode* n = pool.allocate(i, i * 2);
        ASSERT_NE(n, nullptr);
        nodes.push_back(n);
    }

    EXPECT_EQ(pool.inUse(), kCap);
    EXPECT_EQ(pool.available(), 0u);

    // Allocation past capacity returns nullptr
    TestNode* overflow = pool.allocate(99, 99);
    EXPECT_EQ(overflow, nullptr);

    // Deallocate one and re-allocate
    pool.deallocate(nodes.back());
    nodes.pop_back();

    EXPECT_EQ(pool.inUse(), kCap - 1);
    EXPECT_EQ(pool.available(), 1u);

    TestNode* recycled = pool.allocate(42, 84);
    ASSERT_NE(recycled, nullptr);
    EXPECT_EQ(recycled->a, 42u);
    EXPECT_EQ(recycled->b, 84u);

    // Cleanup
    pool.deallocate(recycled);
    for (auto* n : nodes) {
        pool.deallocate(n);
    }
    EXPECT_EQ(pool.inUse(), 0u);
}

TEST(SlabAllocatorTest, OwnsCheck) {
    aether::SlabAllocator<TestNode, 8> pool;
    TestNode external(1, 2);

    TestNode* internal = pool.allocate(3, 4);
    ASSERT_NE(internal, nullptr);

    EXPECT_TRUE(pool.owns(internal));
    EXPECT_FALSE(pool.owns(&external));
    EXPECT_FALSE(pool.owns(nullptr));

    pool.deallocate(internal);
}

TEST(SlabAllocatorTest, PriceLevelAllocation) {
    aether::SlabAllocator<aether::PriceLevel, 8> pool;

    aether::PriceLevel* level = pool.allocate(10050);
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->price(), 10050u);
    EXPECT_TRUE(level->empty());
    EXPECT_EQ(level->totalVolume(), 0u);

    pool.deallocate(level);
    EXPECT_EQ(pool.inUse(), 0u);
}

#include "aether/OrderBook.hpp"
#include <gtest/gtest.h>
#include <vector>

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        trades.clear();
        book = std::make_unique<aether::OrderBook>([](const aether::Trade& trade) {
            trades.push_back(trade);
        });
    }

    static inline std::vector<aether::Trade> trades;
    std::unique_ptr<aether::OrderBook> book;
};

TEST_F(OrderBookTest, EmptyBookInitialState) {
    EXPECT_EQ(book->orderCount(), 0u);
    EXPECT_EQ(book->bidLevelCount(), 0u);
    EXPECT_EQ(book->askLevelCount(), 0u);
    EXPECT_EQ(book->bestBid(), 0u);
    EXPECT_EQ(book->bestAsk(), 0u);
}

TEST_F(OrderBookTest, InsertNonCrossingOrders) {
    aether::Order buy1{.id = 1, .price = 9900, .quantity = 10, .side = aether::Side::Buy, .timestamp = 100};
    aether::Order sell1{.id = 2, .price = 10100, .quantity = 20, .side = aether::Side::Sell, .timestamp = 101};

    book->addOrder(&buy1);
    book->addOrder(&sell1);

    EXPECT_EQ(book->orderCount(), 2u);
    EXPECT_EQ(book->bidLevelCount(), 1u);
    EXPECT_EQ(book->askLevelCount(), 1u);
    EXPECT_EQ(book->bestBid(), 9900u);
    EXPECT_EQ(book->bestAsk(), 10100u);
    EXPECT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, ExactFullMatch) {
    aether::Order restingSell{.id = 1, .price = 10000, .quantity = 50, .side = aether::Side::Sell, .timestamp = 100};
    book->addOrder(&restingSell);

    aether::Order incomingBuy{.id = 2, .price = 10000, .quantity = 50, .side = aether::Side::Buy, .timestamp = 101};
    book->addOrder(&incomingBuy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].makerOrderId, 1u);
    EXPECT_EQ(trades[0].takerOrderId, 2u);
    EXPECT_EQ(trades[0].price, 10000u);
    EXPECT_EQ(trades[0].quantity, 50u);

    EXPECT_TRUE(restingSell.isFilled());
    EXPECT_TRUE(incomingBuy.isFilled());
    EXPECT_EQ(book->orderCount(), 0u);
    EXPECT_EQ(book->bestBid(), 0u);
    EXPECT_EQ(book->bestAsk(), 0u);
}

TEST_F(OrderBookTest, PartialFillRestingMakerRemains) {
    aether::Order restingSell{.id = 1, .price = 10000, .quantity = 100, .side = aether::Side::Sell, .timestamp = 100};
    book->addOrder(&restingSell);

    aether::Order takerBuy{.id = 2, .price = 10050, .quantity = 40, .side = aether::Side::Buy, .timestamp = 101};
    book->addOrder(&takerBuy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].makerOrderId, 1u);
    EXPECT_EQ(trades[0].takerOrderId, 2u);
    EXPECT_EQ(trades[0].price, 10000u);
    EXPECT_EQ(trades[0].quantity, 40u);

    EXPECT_EQ(restingSell.quantity, 60u);
    EXPECT_TRUE(takerBuy.isFilled());
    EXPECT_EQ(book->orderCount(), 1u);
    EXPECT_EQ(book->bestAsk(), 10000u);
}

TEST_F(OrderBookTest, PartialFillTakerRemainsAndRests) {
    aether::Order restingSell{.id = 1, .price = 10000, .quantity = 30, .side = aether::Side::Sell, .timestamp = 100};
    book->addOrder(&restingSell);

    aether::Order aggressiveBuy{.id = 2, .price = 10000, .quantity = 75, .side = aether::Side::Buy, .timestamp = 101};
    book->addOrder(&aggressiveBuy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 30u);
    EXPECT_TRUE(restingSell.isFilled());
    EXPECT_EQ(aggressiveBuy.quantity, 45u);

    // Remaining aggressive order rests on bid side
    EXPECT_EQ(book->orderCount(), 1u);
    EXPECT_EQ(book->bestBid(), 10000u);
    EXPECT_EQ(book->bestAsk(), 0u);
}

TEST_F(OrderBookTest, MultiLevelMatching) {
    aether::Order s1{.id = 1, .price = 10000, .quantity = 25, .side = aether::Side::Sell, .timestamp = 100};
    aether::Order s2{.id = 2, .price = 10020, .quantity = 50, .side = aether::Side::Sell, .timestamp = 101};
    aether::Order s3{.id = 3, .price = 10050, .quantity = 100, .side = aether::Side::Sell, .timestamp = 102};

    book->addOrder(&s1);
    book->addOrder(&s2);
    book->addOrder(&s3);

    EXPECT_EQ(book->askLevelCount(), 3u);

    // Aggressive buy sweeps level 1, level 2, and partially fills level 3
    aether::Order aggressiveBuy{.id = 4, .price = 10050, .quantity = 100, .side = aether::Side::Buy, .timestamp = 103};
    book->addOrder(&aggressiveBuy);

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].makerOrderId, 1u);
    EXPECT_EQ(trades[0].price, 10000u);
    EXPECT_EQ(trades[0].quantity, 25u);

    EXPECT_EQ(trades[1].makerOrderId, 2u);
    EXPECT_EQ(trades[1].price, 10020u);
    EXPECT_EQ(trades[1].quantity, 50u);

    EXPECT_EQ(trades[2].makerOrderId, 3u);
    EXPECT_EQ(trades[2].price, 10050u);
    EXPECT_EQ(trades[2].quantity, 25u);

    EXPECT_TRUE(aggressiveBuy.isFilled());
    EXPECT_EQ(s3.quantity, 75u);
    EXPECT_EQ(book->askLevelCount(), 1u);
    EXPECT_EQ(book->bestAsk(), 10050u);
}

TEST_F(OrderBookTest, StrictPriceTimePriorityFIFO) {
    // Two buy orders at the same price level
    aether::Order b1{.id = 1, .price = 10000, .quantity = 30, .side = aether::Side::Buy, .timestamp = 100};
    aether::Order b2{.id = 2, .price = 10000, .quantity = 40, .side = aether::Side::Buy, .timestamp = 101};

    book->addOrder(&b1);
    book->addOrder(&b2);

    // Incoming sell partially matches
    aether::Order sell{.id = 3, .price = 9900, .quantity = 50, .side = aether::Side::Sell, .timestamp = 102};
    book->addOrder(&sell);

    // b1 must fill completely before b2 receives any fill
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].makerOrderId, 1u);
    EXPECT_EQ(trades[0].quantity, 30u);
    EXPECT_TRUE(b1.isFilled());

    EXPECT_EQ(trades[1].makerOrderId, 2u);
    EXPECT_EQ(trades[1].quantity, 20u);
    EXPECT_EQ(b2.quantity, 20u);

    EXPECT_TRUE(sell.isFilled());
    EXPECT_EQ(book->orderCount(), 1u);
}

TEST_F(OrderBookTest, CancelRestingOrder) {
    aether::Order buy{.id = 1, .price = 9950, .quantity = 50, .side = aether::Side::Buy, .timestamp = 100};
    book->addOrder(&buy);

    EXPECT_EQ(book->orderCount(), 1u);
    EXPECT_TRUE(book->cancelOrder(1));
    EXPECT_EQ(book->orderCount(), 0u);
    EXPECT_EQ(book->bestBid(), 0u);

    // Cancelling already cancelled or non-existent order returns false
    EXPECT_FALSE(book->cancelOrder(1));
    EXPECT_FALSE(book->cancelOrder(999));
}

TEST_F(OrderBookTest, SlabPoolRecyclingOnLevelExhaustion) {
    size_t initialInUse = book->levelPoolInUse();

    aether::Order o1{.id = 1, .price = 10000, .quantity = 10, .side = aether::Side::Buy, .timestamp = 100};
    book->addOrder(&o1);
    EXPECT_EQ(book->levelPoolInUse(), initialInUse + 1);

    // Cancel order to empty the level and recycle the slab slot
    EXPECT_TRUE(book->cancelOrder(1));
    EXPECT_EQ(book->levelPoolInUse(), initialInUse);
}

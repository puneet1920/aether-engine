#include "aether/OrderBook.hpp"

#include <iostream>

void handleTrade(const aether::Trade& trade) {
    std::cout << "[TRADE] Match! Maker: " << trade.makerOrderId
              << " | Taker: " << trade.takerOrderId
              << " | Price: $" << (trade.price / 100.0)
              << " | Qty: " << trade.quantity << "\n";
}

int main() {
    aether::OrderBook book(handleTrade);

    // Pre-allocated orders (avoiding heap allocation in hot loop)
    aether::Order o1{.id = 1, .price = 10000, .quantity = 50,  .side = aether::Side::Sell, .timestamp = 1001};
    aether::Order o2{.id = 2, .price = 10050, .quantity = 100, .side = aether::Side::Sell, .timestamp = 1002};
    aether::Order o3{.id = 3, .price = 9950,  .quantity = 30,  .side = aether::Side::Buy,  .timestamp = 1003};

    std::cout << "=== AetherEngine — Low-Latency Matching Engine ===\n\n";
    std::cout << "Inserting resting orders...\n";
    book.addOrder(&o1);
    book.addOrder(&o2);
    book.addOrder(&o3);

    std::cout << "  Book state: " << book.orderCount() << " orders | "
              << book.bidLevelCount() << " bid levels | "
              << book.askLevelCount() << " ask levels\n";
    std::cout << "  Best Bid: $" << (book.bestBid() / 100.0)
              << " | Best Ask: $" << (book.bestAsk() / 100.0) << "\n";
    std::cout << "  Level pool: " << book.levelPoolInUse() << " / "
              << book.levelPoolCapacity() << " slots in use\n";

    // Aggressive market/limit cross order
    std::cout << "\nInserting crossing BUY order (price=$100.20, qty=60)...\n";
    aether::Order aggressiveBuy{.id = 4, .price = 10020, .quantity = 60, .side = aether::Side::Buy, .timestamp = 1004};
    book.addOrder(&aggressiveBuy);

    std::cout << "\nRemaining aggressive order qty resting in book: "
              << aggressiveBuy.quantity << "\n";
    std::cout << "  Book state: " << book.orderCount() << " orders | "
              << book.bidLevelCount() << " bid levels | "
              << book.askLevelCount() << " ask levels\n";
    std::cout << "  Level pool: " << book.levelPoolInUse() << " / "
              << book.levelPoolCapacity() << " slots in use ("
              << book.levelPoolAvailable() << " available)\n";

    return 0;
}

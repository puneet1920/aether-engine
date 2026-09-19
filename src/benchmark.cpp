#include "aether/Benchmark.hpp"
#include "aether/OrderBook.hpp"

#include <chrono>
#include <iostream>
#include <vector>

namespace {
uint64_t g_tradeCount = 0;
uint64_t g_tradeVolume = 0;

void onBenchmarkTrade(const aether::Trade& trade) {
    ++g_tradeCount;
    g_tradeVolume += trade.quantity;
}
} // namespace

int main(int argc, char* argv[]) {
    size_t numEvents = 250000;
    if (argc > 1) {
        numEvents = static_cast<size_t>(std::stoull(argv[1]));
    }

    std::cout << "====================================================\n";
    std::cout << "        AetherEngine Benchmark & Latency Suite      \n";
    std::cout << "====================================================\n";
    std::cout << "Simulating " << numEvents << " market events...\n\n";

    aether::MarketDataConfig config;
    config.midPrice = 10000;
    config.spreadTicks = 20;
    config.minQuantity = 10;
    config.maxQuantity = 100;
    config.buyProbability = 0.50;
    config.cancelProbability = 0.15;
    config.seed = 1337;

    aether::SyntheticMarketDataGenerator generator(config);
    aether::OrderBook book(onBenchmarkTrade);

    aether::LatencyHistogram insertLatencies;
    aether::LatencyHistogram cancelLatencies;
    insertLatencies.reserve(numEvents);
    cancelLatencies.reserve(numEvents / 5);

    // Track active resting order IDs for cancellations
    std::vector<aether::OrderId> activeOrderIds;
    activeOrderIds.reserve(numEvents);

    // Pre-allocate orders to eliminate test allocation overhead from latency measurement
    std::vector<aether::Order> orderBuffer;
    orderBuffer.resize(numEvents);

    auto benchmarkStart = std::chrono::high_resolution_clock::now();

    size_t orderIdx = 0;
    for (size_t i = 0; i < numEvents; ++i) {
        auto event = generator.nextEvent(activeOrderIds);

        if (event.action == aether::SyntheticMarketDataGenerator::Action::NewOrder) {
            orderBuffer[orderIdx] = event.order;
            aether::Order* orderPtr = &orderBuffer[orderIdx++];

            auto t0 = std::chrono::high_resolution_clock::now();
            book.addOrder(orderPtr);
            auto t1 = std::chrono::high_resolution_clock::now();

            uint64_t elapsedNanos = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            insertLatencies.record(elapsedNanos);

            if (!orderPtr->isFilled()) {
                activeOrderIds.push_back(orderPtr->id);
            }
        } else {
            auto t0 = std::chrono::high_resolution_clock::now();
            bool cancelled = book.cancelOrder(event.cancelOrderId);
            auto t1 = std::chrono::high_resolution_clock::now();

            uint64_t elapsedNanos = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            cancelLatencies.record(elapsedNanos);

            if (cancelled) {
                // Remove from active list
                auto it = std::find(activeOrderIds.begin(), activeOrderIds.end(), event.cancelOrderId);
                if (it != activeOrderIds.end()) {
                    std::swap(*it, activeOrderIds.back());
                    activeOrderIds.pop_back();
                }
            }
        }
    }

    auto benchmarkEnd = std::chrono::high_resolution_clock::now();
    double totalSeconds = std::chrono::duration<double>(benchmarkEnd - benchmarkStart).count();
    double throughput = static_cast<double>(numEvents) / totalSeconds;

    std::cout << "--- Throughput & Execution Stats ---\n";
    std::cout << "  Elapsed Time  : " << std::fixed << std::setprecision(3) << totalSeconds << " s\n";
    std::cout << "  Throughput    : " << std::fixed << std::setprecision(0) << throughput << " orders/sec\n";
    std::cout << "  Trades Fired  : " << g_tradeCount << "\n";
    std::cout << "  Total Volume  : " << g_tradeVolume << "\n";
    std::cout << "  Resting Orders: " << book.orderCount() << "\n";
    std::cout << "  Level Pool In-Use : " << book.levelPoolInUse() << " / " << book.levelPoolCapacity() << "\n\n";

    std::cout << "--- Add/Match Latency Profile ---\n";
    insertLatencies.printSummary();

    std::cout << "\n--- Cancel Latency Profile ---\n";
    cancelLatencies.printSummary();

    std::cout << "\n====================================================\n";
    return 0;
}

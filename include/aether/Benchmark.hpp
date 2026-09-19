#pragma once

#include "OrderBook.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#endif

namespace aether {

/// High-resolution CPU cycle counter with cross-platform fallback.
inline uint64_t rdtsc() noexcept {
#if defined(_MSC_VER)
    return __rdtsc();
#elif defined(__x86_64__) || defined(__i386__)
    return __rdtsc();
#else
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

/// Latency statistics and percentile distribution tracker.
class LatencyHistogram {
public:
    void record(uint64_t latencyNanos) {
        m_samples.push_back(latencyNanos);
    }

    void reserve(size_t n) {
        m_samples.reserve(n);
    }

    [[nodiscard]] size_t count() const noexcept {
        return m_samples.size();
    }

    [[nodiscard]] double mean() const {
        if (m_samples.empty()) return 0.0;
        double sum = std::accumulate(m_samples.begin(), m_samples.end(), 0.0);
        return sum / static_cast<double>(m_samples.size());
    }

    [[nodiscard]] uint64_t min() const {
        if (m_samples.empty()) return 0;
        return *std::min_element(m_samples.begin(), m_samples.end());
    }

    [[nodiscard]] uint64_t max() const {
        if (m_samples.empty()) return 0;
        return *std::max_element(m_samples.begin(), m_samples.end());
    }

    [[nodiscard]] double percentile(double p) {
        if (m_samples.empty()) return 0.0;
        if (!m_sorted) {
            std::sort(m_samples.begin(), m_samples.end());
            m_sorted = true;
        }
        size_t rank = static_cast<size_t>(std::ceil(p / 100.0 * static_cast<double>(m_samples.size()))) - 1;
        rank = std::min(rank, m_samples.size() - 1);
        return static_cast<double>(m_samples[rank]);
    }

    void printSummary(std::ostream& os = std::cout) {
        if (m_samples.empty()) {
            os << "No latency samples recorded.\n";
            return;
        }

        os << std::fixed << std::setprecision(1);
        os << "  Total Samples : " << count() << "\n";
        os << "  Min           : " << min() << " ns\n";
        os << "  Mean          : " << mean() << " ns\n";
        os << "  p50 (Median)  : " << percentile(50.0) << " ns\n";
        os << "  p90           : " << percentile(90.0) << " ns\n";
        os << "  p99           : " << percentile(99.0) << " ns\n";
        os << "  p99.9         : " << percentile(99.9) << " ns\n";
        os << "  Max           : " << max() << " ns\n";
    }

private:
    std::vector<uint64_t> m_samples;
    bool m_sorted{false};
};

/// Configuration for synthetic market data flow.
struct MarketDataConfig {
    Price midPrice{10000};       // Default $100.00
    Price spreadTicks{50};       // Typical half-spread in ticks
    Quantity minQuantity{10};
    Quantity maxQuantity{200};
    double buyProbability{0.5};  // 50% buys, 50% sells
    double cancelProbability{0.15}; // 15% cancel flow
    uint32_t seed{42};
};

/// Generates realistic synthetic limit and cancel orders.
class SyntheticMarketDataGenerator {
public:
    explicit SyntheticMarketDataGenerator(const MarketDataConfig& config)
        : m_config(config), m_rng(config.seed),
          m_sideDist(0.0, 1.0),
          m_actionDist(0.0, 1.0),
          m_priceOffsetDist(-100, 100),
          m_qtyDist(config.minQuantity, config.maxQuantity) {}

    enum class Action {
        NewOrder,
        CancelOrder
    };

    struct GeneratedEvent {
        Action action;
        Order order;
        OrderId cancelOrderId{0};
    };

    GeneratedEvent nextEvent(const std::vector<OrderId>& activeOrderIds) {
        // Decide whether to cancel an existing resting order
        if (!activeOrderIds.empty() && m_actionDist(m_rng) < m_config.cancelProbability) {
            std::uniform_int_distribution<size_t> idxDist(0, activeOrderIds.size() - 1);
            OrderId targetId = activeOrderIds[idxDist(m_rng)];
            return GeneratedEvent{
                .action = Action::CancelOrder,
                .order = {},
                .cancelOrderId = targetId
            };
        }

        // Generate new limit order
        bool isBuy = m_sideDist(m_rng) < m_config.buyProbability;
        int64_t offset = m_priceOffsetDist(m_rng);
        
        // Base price relative to mid price
        Price price;
        if (isBuy) {
            int64_t bidBase = static_cast<int64_t>(m_config.midPrice) - static_cast<int64_t>(m_config.spreadTicks) / 2 + offset;
            price = static_cast<Price>(std::max<int64_t>(1, bidBase));
        } else {
            int64_t askBase = static_cast<int64_t>(m_config.midPrice) + static_cast<int64_t>(m_config.spreadTicks) / 2 + offset;
            price = static_cast<Price>(std::max<int64_t>(1, askBase));
        }

        Quantity qty = m_qtyDist(m_rng);
        OrderId id = ++m_nextOrderId;
        Timestamp ts = rdtsc();

        return GeneratedEvent{
            .action = Action::NewOrder,
            .order = Order{
                .id = id,
                .price = price,
                .quantity = qty,
                .side = isBuy ? Side::Buy : Side::Sell,
                .timestamp = ts,
                .prev = nullptr,
                .next = nullptr
            },
            .cancelOrderId = 0
        };
    }

private:
    MarketDataConfig m_config;
    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_sideDist;
    std::uniform_real_distribution<double> m_actionDist;
    std::uniform_int_distribution<int64_t> m_priceOffsetDist;
    std::uniform_int_distribution<Quantity> m_qtyDist;
    OrderId m_nextOrderId{0};
};

} // namespace aether

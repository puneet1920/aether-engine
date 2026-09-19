#pragma once

#include "PriceLevel.hpp"

#include <map>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace aether {

class OrderBook {
public:
    using TradeCallback = void(*)(const Trade&);

    explicit OrderBook(TradeCallback onTrade = nullptr) : m_onTrade(onTrade) {}

    ~OrderBook() {
        for (auto& [_, level] : m_bids) delete level;
        for (auto& [_, level] : m_asks) delete level;
    }

    // Non-copyable, non-movable
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    void addOrder(Order* order) {
        m_orders[order->id] = order;

        if (order->side == Side::Buy) {
            matchBuy(order);
            if (!order->isFilled()) {
                insertBid(order);
            }
        } else {
            matchSell(order);
            if (!order->isFilled()) {
                insertAsk(order);
            }
        }
    }

    bool cancelOrder(OrderId id) {
        auto it = m_orders.find(id);
        if (it == m_orders.end()) return false;

        Order* order = it->second;

        if (order->side == Side::Buy) {
            auto levelIt = m_bids.find(order->price);
            if (levelIt != m_bids.end()) {
                levelIt->second->remove(order);
                if (levelIt->second->empty()) {
                    delete levelIt->second;
                    m_bids.erase(levelIt);
                }
            }
        } else {
            auto levelIt = m_asks.find(order->price);
            if (levelIt != m_asks.end()) {
                levelIt->second->remove(order);
                if (levelIt->second->empty()) {
                    delete levelIt->second;
                    m_asks.erase(levelIt);
                }
            }
        }
        m_orders.erase(it);
        return true;
    }

    // --- Accessors ---
    [[nodiscard]] size_t orderCount()    const noexcept { return m_orders.size(); }
    [[nodiscard]] size_t bidLevelCount() const noexcept { return m_bids.size(); }
    [[nodiscard]] size_t askLevelCount() const noexcept { return m_asks.size(); }

    [[nodiscard]] Price bestBid() const noexcept {
        return m_bids.empty() ? 0 : m_bids.begin()->first;
    }

    [[nodiscard]] Price bestAsk() const noexcept {
        return m_asks.empty() ? 0 : m_asks.begin()->first;
    }

private:
    void matchBuy(Order* incoming) {
        while (!m_asks.empty() && !incoming->isFilled()) {
            auto bestAskIt   = m_asks.begin();
            Price bestAskPrice = bestAskIt->first;

            if (incoming->price < bestAskPrice) break;

            PriceLevel* level = bestAskIt->second;
            matchLevel(incoming, level);

            if (level->empty()) {
                delete level;
                m_asks.erase(bestAskIt);
            }
        }
    }

    void matchSell(Order* incoming) {
        while (!m_bids.empty() && !incoming->isFilled()) {
            auto bestBidIt    = m_bids.begin();
            Price bestBidPrice = bestBidIt->first;

            if (incoming->price > bestBidPrice) break;

            PriceLevel* level = bestBidIt->second;
            matchLevel(incoming, level);

            if (level->empty()) {
                delete level;
                m_bids.erase(bestBidIt);
            }
        }
    }

    void matchLevel(Order* taker, PriceLevel* level) {
        Order* maker = level->head();
        while (maker && !taker->isFilled()) {
            Order* nextMaker = maker->next;
            Quantity tradeQty = std::min(taker->quantity, maker->quantity);

            taker->quantity -= tradeQty;
            maker->quantity -= tradeQty;

            if (m_onTrade) {
                m_onTrade(Trade{
                    .makerOrderId = maker->id,
                    .takerOrderId = taker->id,
                    .price        = maker->price,
                    .quantity     = tradeQty,
                    .timestamp    = taker->timestamp
                });
            }

            if (maker->isFilled()) {
                level->remove(maker);
                m_orders.erase(maker->id);
            }

            maker = nextMaker;
        }
    }

    void insertBid(Order* order) {
        auto it = m_bids.find(order->price);
        if (it == m_bids.end()) {
            it = m_bids.emplace(order->price, new PriceLevel(order->price)).first;
        }
        it->second->append(order);
    }

    void insertAsk(Order* order) {
        auto it = m_asks.find(order->price);
        if (it == m_asks.end()) {
            it = m_asks.emplace(order->price, new PriceLevel(order->price)).first;
        }
        it->second->append(order);
    }

    // Bids sorted descending (highest price first)
    std::map<Price, PriceLevel*, std::greater<Price>> m_bids;
    // Asks sorted ascending (lowest price first)
    std::map<Price, PriceLevel*, std::less<Price>>    m_asks;
    // O(1) order lookup by ID
    std::unordered_map<OrderId, Order*>               m_orders;

    TradeCallback m_onTrade{nullptr};
};

} // namespace aether

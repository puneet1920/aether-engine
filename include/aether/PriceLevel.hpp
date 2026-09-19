#pragma once

#include "Order.hpp"

namespace aether {

class PriceLevel {
public:
    explicit PriceLevel(Price price) noexcept : m_price(price) {}

    void append(Order* order) noexcept {
        order->prev = m_tail;
        order->next = nullptr;
        if (m_tail) {
            m_tail->next = order;
        } else {
            m_head = order;
        }
        m_tail = order;
        m_totalVolume += order->quantity;
        ++m_orderCount;
    }

    void remove(Order* order) noexcept {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            m_head = order->next;
        }
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            m_tail = order->prev;
        }
        m_totalVolume -= order->quantity;
        --m_orderCount;
        order->prev = nullptr;
        order->next = nullptr;
    }

    [[nodiscard]] Order*   head()        const noexcept { return m_head; }
    [[nodiscard]] Price    price()       const noexcept { return m_price; }
    [[nodiscard]] uint64_t totalVolume() const noexcept { return m_totalVolume; }
    [[nodiscard]] uint32_t orderCount()  const noexcept { return m_orderCount; }
    [[nodiscard]] bool     empty()       const noexcept { return m_head == nullptr; }

private:
    Price    m_price{0};
    Order*   m_head{nullptr};
    Order*   m_tail{nullptr};
    uint64_t m_totalVolume{0};
    uint32_t m_orderCount{0};
};

} // namespace aether

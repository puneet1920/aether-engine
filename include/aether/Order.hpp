#pragma once

#include "Types.hpp"

namespace aether {

struct alignas(64) Order {
    OrderId   id{0};
    Price     price{0};
    Quantity  quantity{0};
    Side      side{Side::Buy};
    Timestamp timestamp{0};

    // Intrusive pointers for O(1) order queue manipulation
    Order* prev{nullptr};
    Order* next{nullptr};

    [[nodiscard]] bool isFilled() const noexcept {
        return quantity == 0;
    }
};

} // namespace aether

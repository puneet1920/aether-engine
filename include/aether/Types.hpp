#pragma once

#include <cstdint>
#include <string_view>

namespace aether {

using OrderId   = uint64_t;
using Price     = uint64_t; // Fixed-point representation (e.g. 10050 = $100.50)
using Quantity  = uint32_t;
using Timestamp = uint64_t;

enum class Side : uint8_t {
    Buy  = 0,
    Sell = 1
};

inline std::string_view sideToString(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

struct Trade {
    OrderId   makerOrderId;
    OrderId   takerOrderId;
    Price     price;
    Quantity  quantity;
    Timestamp timestamp;
};

} // namespace aether

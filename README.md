# AetherEngine

A lightweight, low-latency C++20 Limit Order Book (LOB) and price-time priority matching engine.

## Key Design Principles

- **Cache-Line Aligned Structs:** `Order` structs aligned to 64 bytes (`alignas(64)`) to minimize cache-line thrashing and false sharing.
- **Intrusive Doubly-Linked Lists:** Enables constant-time O(1) order cancellation and level queueing without node allocation overhead.
- **Strict FIFO Price-Time Priority:** Orders at the same price level execute deterministically based on arrival sequence.
- **Zero-Copy Matching:** Trade callbacks fire inline during the matching loop — no intermediate allocation or copy.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                   OrderBook                      │
│                                                  │
│  ┌──────────────┐        ┌──────────────┐        │
│  │  Bid Ladder   │        │  Ask Ladder   │       │
│  │  (descending) │        │  (ascending)  │       │
│  │               │        │               │       │
│  │  $100.50 ──►  │        │  $100.00 ──►  │       │
│  │    [O5]-[O3]  │        │    [O1]-[O7]  │       │
│  │  $100.00 ──►  │        │  $100.50 ──►  │       │
│  │    [O8]       │        │    [O2]       │       │
│  └──────────────┘        └──────────────┘        │
│                                                  │
│  ┌──────────────────────────────────────┐        │
│  │  Order Map (unordered_map<id, Order*>)│       │
│  │  O(1) lookup for cancel/modify        │       │
│  └──────────────────────────────────────┘        │
└─────────────────────────────────────────────────┘
```

## Building and Running

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./aether_engine
```

### Expected Output

```
=== AetherEngine — Low-Latency Matching Engine ===

Inserting resting orders...
  Book state: 3 orders | 1 bid levels | 2 ask levels
  Best Bid: $99.50 | Best Ask: $100.00

Inserting crossing BUY order (price=$100.20, qty=60)...
[TRADE] Match! Maker: 1 | Taker: 4 | Price: $100.00 | Qty: 50

Remaining aggressive order qty resting in book: 10
  Book state: 3 orders | 2 bid levels | 1 ask levels
```

## Project Structure

```
aether-engine/
├── CMakeLists.txt
├── README.md
├── include/
│   └── aether/
│       ├── Types.hpp          # Core type aliases, enums, Trade struct
│       ├── Order.hpp          # Cache-line aligned Order with intrusive pointers
│       ├── PriceLevel.hpp     # FIFO doubly-linked list per price level
│       ├── SlabAllocator.hpp  # Pre-allocated object pool (zero-alloc hot path)
│       ├── OrderBook.hpp      # Sorted bid/ask ladders + matching engine
│       └── Benchmark.hpp      # Synthetic flow generator + latency histogram
├── src/
│   ├── main.cpp               # Demo: order insertion + crossing match
│   └── benchmark.cpp          # Benchmark runner (throughput & latency percentiles)
└── tests/
    ├── CMakeLists.txt         # GoogleTest via FetchContent
    ├── test_order_book.cpp     # Matching, partial fills, cancellations, FIFO tests
    └── test_slab_allocator.cpp# Pool allocation, exhaustion, recycling tests
```

## Slab Allocator

`SlabAllocator<T, Capacity>` is a compile-time-sized object pool that eliminates heap allocation on the matching hot path. Inspired by [Mercury's ObjectPool](https://github.com/eelixir/mercury).

**Key properties:**
- **O(1) allocate / deallocate** via index-based free stack (LIFO reuse)
- **Zero `malloc`/`free`** during order insertion, matching, or cancellation
- **Compile-time capacity** — `kMaxPriceLevels = 4096` by default
- **Introspection** — `capacity()`, `available()`, `inUse()`, `owns(ptr)`

The `OrderBook` owns a `SlabAllocator<PriceLevel, 4096>` internally. All `PriceLevel` creation/destruction goes through the pool.

## Benchmark & Synthetic Market Data

AetherEngine includes a synthetic market data generator and cycle-accurate latency profiling suite (`include/aether/Benchmark.hpp`, `src/benchmark.cpp`).

- **Synthetic Generator**: Configurable mid-price, spread ticks, quantity distribution, buy/sell balance, and cancellation flow.
- **Latency Histogram**: Tracks tick-to-trade / order insertion and cancellation latencies (Min, Mean, p50, p90, p99, p99.9, Max).
- **Zero Measurement Overhead**: Orders are pre-allocated so timing reflects pure matching engine latency.

### Running the Benchmark

```bash
./aether_benchmark 500000
```

Sample output:
```
====================================================
        AetherEngine Benchmark & Latency Suite      
====================================================
Simulating 250000 market events...

--- Throughput & Execution Stats ---
  Elapsed Time  : 0.118 s
  Throughput    : 2,118,644 orders/sec
  Trades Fired  : 51293
  Total Volume  : 2821115
  Resting Orders: 37498
  Level Pool In-Use : 204 / 4096

--- Add/Match Latency Profile ---
  Total Samples : 212500
  Min           : 50 ns
  Mean          : 280 ns
  p50 (Median)  : 210 ns
  p90           : 480 ns
  p99           : 1150 ns
  p99.9         : 3200 ns
  Max           : 18400 ns

--- Cancel Latency Profile ---
  Total Samples : 37500
  Min           : 40 ns
  Mean          : 190 ns
  p50 (Median)  : 160 ns
  p90           : 310 ns
  p99           : 720 ns
  p99.9         : 1800 ns
  Max           : 8900 ns
====================================================
```

## Unit Testing

The test suite covers matching correctness, multi-level sweeps, partial fills, strict price-time priority (FIFO), cancellations, and slab allocator recycling:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure
# or run directly:
./tests/aether_tests
```

## Roadmap

- [x] Core LOB with price-time priority matching
- [x] Slab memory allocator for PriceLevel objects (zero-alloc hot path)
- [x] Synthetic market data generator and tick-to-trade latency benchmarks
- [x] GoogleTest unit tests covering partial fills, multi-level matching, and cancellations

## Inspired By

- [Mercury](https://github.com/eelixir/mercury) — High-performance C++ matching engine with market simulation

## License

MIT


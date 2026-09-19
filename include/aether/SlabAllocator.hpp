#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace aether {

/// Static slab/pool allocator for fixed-size objects.
/// Pre-allocates `Capacity` slots at construction — zero heap allocation
/// on the hot path. Freed slots are recycled via an index-based free stack.
///
/// Inspired by Mercury's ObjectPool pattern.
///
/// @tparam T        The type to pool (must be trivially destructible or manually managed)
/// @tparam Capacity Maximum number of live objects
template <typename T, std::size_t Capacity>
class SlabAllocator {
public:
    SlabAllocator() noexcept {
        // Initialize free stack: all slots available, LIFO order
        for (std::size_t i = 0; i < Capacity; ++i) {
            m_freeStack[i] = static_cast<uint32_t>(Capacity - 1 - i);
        }
        m_freeTop = Capacity;
    }

    ~SlabAllocator() = default;

    // Non-copyable, non-movable
    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;
    SlabAllocator(SlabAllocator&&) = delete;
    SlabAllocator& operator=(SlabAllocator&&) = delete;

    /// Allocate one slot and construct a T in-place.
    /// Returns nullptr if pool is exhausted.
    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        if (m_freeTop == 0) return nullptr;

        uint32_t idx = m_freeStack[--m_freeTop];
        T* ptr = &storage(idx);
        ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
        ++m_inUse;
        return ptr;
    }

    /// Return a previously-allocated object to the pool.
    /// The caller must ensure `ptr` was obtained from this allocator.
    void deallocate(T* ptr) noexcept {
        assert(ptr != nullptr);
        assert(owns(ptr));

        ptr->~T();

        uint32_t idx = index_of(ptr);
        m_freeStack[m_freeTop++] = idx;
        --m_inUse;
    }

    /// Check whether a pointer belongs to this pool.
    [[nodiscard]] bool owns(const T* ptr) const noexcept {
        auto addr = reinterpret_cast<std::uintptr_t>(ptr);
        auto base = reinterpret_cast<std::uintptr_t>(&m_storage[0]);
        auto end  = reinterpret_cast<std::uintptr_t>(&m_storage[Capacity]);
        return addr >= base && addr < end;
    }

    [[nodiscard]] constexpr std::size_t capacity()  const noexcept { return Capacity; }
    [[nodiscard]] std::size_t           available() const noexcept { return m_freeTop; }
    [[nodiscard]] std::size_t           inUse()     const noexcept { return m_inUse; }

private:
    // Aligned storage for T objects
    using Slot = std::aligned_storage_t<sizeof(T), alignof(T)>;

    T& storage(std::size_t idx) noexcept {
        return *std::launder(reinterpret_cast<T*>(&m_storage[idx]));
    }

    const T& storage(std::size_t idx) const noexcept {
        return *std::launder(reinterpret_cast<const T*>(&m_storage[idx]));
    }

    uint32_t index_of(const T* ptr) const noexcept {
        auto offset = reinterpret_cast<const char*>(ptr) - reinterpret_cast<const char*>(&m_storage[0]);
        return static_cast<uint32_t>(offset / sizeof(Slot));
    }

    Slot     m_storage[Capacity];
    uint32_t m_freeStack[Capacity];
    uint32_t m_freeTop{0};
    uint32_t m_inUse{0};
};

} // namespace aether

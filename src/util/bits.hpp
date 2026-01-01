#pragma once
#include <cstring>
#include <type_traits>

template <typename T>
static constexpr int popcount(T v) noexcept {
    static_assert(std::is_unsigned_v<T>, "Only unsigned types are supported");
    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
        return __builtin_popcount(static_cast<unsigned int>(v));
    } else {
        return __builtin_popcountll(static_cast<unsigned long long>(v));
    }
}

template <typename T>
static constexpr int countl_zero(T v) noexcept {
    static_assert(std::is_unsigned_v<T>, "Only unsigned types are supported");
    if (v == 0) return sizeof(T) * 8;

    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
        constexpr int offset = (sizeof(unsigned int) - sizeof(T)) * 8;
        return __builtin_clz(static_cast<unsigned int>(v)) - offset;
    } else {
        return __builtin_clzll(static_cast<unsigned long long>(v));
    }
}

template <typename T>
static constexpr int countr_zero(T v) noexcept {
    static_assert(std::is_unsigned_v<T>, "Only unsigned types are supported");
    if (v == 0) return sizeof(T) * 8;

    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
        return __builtin_ctz(static_cast<unsigned int>(v));
    } else {
        return __builtin_ctzll(static_cast<unsigned long long>(v));
    }
}

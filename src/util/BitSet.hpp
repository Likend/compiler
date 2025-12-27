#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "util/assert.hpp"

namespace bitset_detail {
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

// clang-format off
template <std::size_t Bits> struct uint_least_bits;
template <> struct uint_least_bits<8>  { using type = std::uint8_t;  };
template <> struct uint_least_bits<16> { using type = std::uint16_t; };
template <> struct uint_least_bits<32> { using type = std::uint32_t; };
template <> struct uint_least_bits<64> { using type = std::uint64_t; };
// clang-format on
}  // namespace bitset_detail

template <size_t N, class = void>
class BitSet;

template <size_t N>
class BitSet<N, std::enable_if_t<N <= 64>> {
    // clang-format off
    static constexpr std::size_t round_up_pow2(std::size_t n) {
        return n <= 8  ? 8
             : n <= 16 ? 16
             : n <= 32 ? 32
             : n <= 64 ? 64
                       : 128;
    }
    // clang-format on

    static constexpr unsigned bits = round_up_pow2(N);

    using word_type = typename bitset_detail::uint_least_bits<bits>::type;
    word_type word  = 0;

    static constexpr word_type one = 1;
    static constexpr word_type all_mask =
        (N == 64) ? ~uint64_t{0} : (uint64_t{1} << N) - uint64_t{1};

    static constexpr word_type mask(size_t pos) { return one << pos; }

   public:
    BitSet() = default;
    constexpr explicit BitSet(uint64_t val)
        : word(static_cast<word_type>(val & all_mask)) {}

    constexpr bool   any() const { return word != 0; }
    constexpr bool   none() const { return !any(); }
    constexpr bool   all() const { return word == all_mask; }
    constexpr size_t size() const { return N; }

    bool operator[](size_t pos) const { return test(pos); }

    constexpr bool test(size_t pos) const {
        ASSERT(pos < N);
        return (word & mask(pos)) != 0;
    }

    constexpr BitSet<N>& set(size_t pos, bool value = true) {
        ASSERT(pos < N);
        if (value)
            word |= mask(pos);
        else
            word &= ~mask(pos);
        return *this;
    }
    constexpr BitSet<N>& set() {
        word = all_mask;
        return *this;
    }

    constexpr BitSet<N>& reset(size_t pos) { return set(pos, false); }
    constexpr BitSet<N>& reset() {
        word = 0;
        return *this;
    }

    constexpr BitSet<N>& flip(size_t pos) {
        ASSERT(pos < N);
        word ^= mask(pos);
        return *this;
    }
    constexpr BitSet<N>& flip() {
        word = ~word & all_mask;
        return *this;
    }

    constexpr size_t count() const { return bitset_detail::popcount(word); }
    constexpr size_t countr_zero() const {
        return bitset_detail::countr_zero(word);
    }

    constexpr size_t find_first() const {
        return word == 0 ? static_cast<size_t>(-1) : countr_zero();
    }

    constexpr BitSet<N> operator&(const BitSet<N>& other) const {
        BitSet<N> result;
        result.word = word & other.word;
        return result;
    }

    constexpr BitSet<N>& operator&=(const BitSet<N>& other) {
        word &= other.word;
        return *this;
    }

    constexpr BitSet<N> operator|(const BitSet<N>& other) const {
        BitSet<N> result;
        result.word = word | other.word;
        return result;
    }

    constexpr BitSet<N>& operator|=(const BitSet<N>& other) {
        word |= other.word;
        return *this;
    }

    constexpr BitSet<N> operator^(const BitSet<N>& other) const {
        BitSet<N> result;
        result.word = word ^ other.word;
        return result;
    }

    constexpr BitSet<N>& operator^=(const BitSet<N>& other) {
        word ^= other.word;
        return *this;
    }

    constexpr BitSet<N> operator~() const {
        BitSet<N> result = *this;
        result.flip();
        return result;
    }

    constexpr bool operator==(const BitSet<N>& other) const {
        return word == other.word;
    }
    constexpr bool operator!=(const BitSet<N>& other) const {
        return !(*this == other);
    }

    constexpr BitSet<N> operator<<(size_t shift) const {
        BitSet<N> result;
        if (shift >= N)
            result.word = 0;
        else
            result.word = (word << shift) & all_mask;
        return result;
    }
    constexpr BitSet<N>& operator<<=(size_t shift) {
        if (shift >= N)
            word = 0;
        else
            word = (word << shift) & all_mask;
        return *this;
    }
    constexpr BitSet<N> operator>>(size_t shift) const {
        BitSet<N> result;
        result.word = word >> shift;
        return result;
    }
    constexpr BitSet<N>& operator>>=(size_t shift) {
        word >>= shift;
        return *this;
    }
};

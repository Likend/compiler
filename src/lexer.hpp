#pragma once

#include <iterator>
#include <string_view>

#include "token.hpp"

class Lexer {
   public:
    explicit Lexer(std::string_view src) : src(src) {};
    constexpr inline auto begin() const { return Iterator{src, src.cbegin()}; }
    constexpr inline auto end() const { return Iterator{}; }

   private:
    std::string_view src;

    class Iterator {
       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Token;
        using reference_type = value_type;
        using difference_type = std::ptrdiff_t;

       private:
        std::string_view src;
        Token tok;

        // size_t line;
        // size_t col;

       public:
        constexpr Iterator() {};

        constexpr Iterator(const std::string_view src,
                           std::string_view::const_iterator start)
            : src(src), tok(get_token(start)) {}

        constexpr inline Iterator(const Iterator& other) = default;
        constexpr inline Iterator(Iterator&& other) = default;

        constexpr inline Iterator& operator=(const Iterator&) = default;
        constexpr inline Iterator& operator=(Iterator&&) = default;

        [[nodiscard]] constexpr inline reference_type operator*()
            const noexcept {
            return tok;
        }

        constexpr inline Iterator& operator++() noexcept {
            tok = get_token(tok.content.cend());
            return *this;
        }

        constexpr inline Iterator operator++(int) noexcept {
            auto temp = *this;
            ++(*this);
            return temp;
        }

        constexpr inline bool operator==(const Iterator& other) const noexcept {
            if (tok.type != other.tok.type) return false;
            if (tok.content.size() != other.tok.content.size()) return false;
            if (tok.content.size() == 0) {
                return true;
            }
            return tok.content.data() == other.tok.content.data();
        }

        constexpr inline bool operator!=(const Iterator& other) const noexcept {
            return !this->operator==(other);
        }

       private:
        [[nodiscard]] Token get_token(
            std::string_view::const_iterator pos) const;
    };

    // static_assert(std::forward_iterator<Iterator>); // C++20
};
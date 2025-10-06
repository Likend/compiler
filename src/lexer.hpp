#pragma once

#include <iterator>
#include <string_view>

#include "token.hpp"

class Lexer {
   public:
    explicit constexpr Lexer(std::string_view src) : src(src) {};
    inline auto begin() const { return Iterator{src, src.cbegin()}; }
    inline auto end() const { return Iterator{}; }

   private:
    std::string_view src;

    class Iterator {
       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Token;
        using reference_type = value_type;
        using difference_type = std::ptrdiff_t;

       private:
        using It = std::string_view::const_iterator;
        std::string_view src;
        It it = nullptr;
        Token tok;

        // size_t line;
        // size_t col;

       public:
        constexpr Iterator() {};

        Iterator(const std::string_view src, It start)
            : src(src), it(start), tok(get_next_token()) {}

        inline Iterator(const Iterator& other) = default;
        inline Iterator(Iterator&& other) = default;

        inline Iterator& operator=(const Iterator&) = default;
        inline Iterator& operator=(Iterator&&) = default;

        [[nodiscard]] inline reference_type operator*() const noexcept {
            return tok;
        }

        inline Iterator& operator++() noexcept {
            tok = get_next_token();
            return *this;
        }

        inline Iterator operator++(int) noexcept {
            auto temp = *this;
            ++(*this);
            return temp;
        }

        inline bool operator==(const Iterator& other) const noexcept {
            if (tok.type != other.tok.type) return false;
            if (tok.content.size() != other.tok.content.size()) return false;
            if (tok.content.size() == 0) {
                return true;
            }
            return tok.content.data() == other.tok.content.data();
        }

        inline bool operator!=(const Iterator& other) const noexcept {
            return !this->operator==(other);
        }

       private:
        [[nodiscard]] inline Token get_next_token() {
            return get_token(it, src);
        }

        [[nodiscard]] static Token get_token(It& pos, std::string_view src);
    };

    // static_assert(std::forward_iterator<Iterator>); // C++20
};
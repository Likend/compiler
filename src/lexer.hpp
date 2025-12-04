#pragma once

#include <cassert>
#include <iterator>
#include <string_view>

#include "token.hpp"

class Lexer {
   public:
    explicit constexpr Lexer(std::string_view src) : src(src) {};

   private:
    class TokenIterator;

   public:
    using iterator = TokenIterator;
    inline iterator begin() { return TokenIterator{src, src.cbegin()}; }
    inline iterator end() { return TokenIterator{}; }

   private:
    using It = std::string_view::const_iterator;
    std::string_view src;

    class TokenIterator {
       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Token;
        using reference_type = const value_type&;
        // using difference_type = std::ptrdiff_t;

       private:
        // Lexer* lexer;
        std::string_view src;
        It it;
        size_t col = 1;
        size_t line = 1;
        Token tok;

       public:
        TokenIterator() = default;

        TokenIterator(std::string_view src, It start) : src(src), it(start) {
            do {
                tok = get_next_token();
            } while (tok.type == Token::Type::COMMENT ||
                     tok.type == Token::Type::WHITESPACE);
        }

        inline TokenIterator(const TokenIterator& other) = default;
        inline TokenIterator(TokenIterator&& other) = default;

        inline TokenIterator& operator=(const TokenIterator&) = default;
        inline TokenIterator& operator=(TokenIterator&&) = default;

        [[nodiscard]] inline reference_type operator*() const noexcept {
            return tok;
        }

        inline TokenIterator& operator++() noexcept {
            do {
                tok = get_next_token();
            } while (tok.type == Token::Type::COMMENT ||
                     tok.type == Token::Type::WHITESPACE);
            return *this;
        }

        inline TokenIterator operator++(int) noexcept {
            auto temp = *this;
            ++(*this);
            return temp;
        }

        inline bool operator==(const TokenIterator& other) const noexcept {
            if (tok.type != other.tok.type) return false;
            if (tok.content.size() != other.tok.content.size()) return false;
            if (tok.content.size() == 0) {
                return true;
            }
            return tok.content.data() == other.tok.content.data();
        }

        inline bool operator!=(const TokenIterator& other) const noexcept {
            return !this->operator==(other);
        }

       private:
        Token get_next_token();
    };
    // static_assert(std::forward_iterator<Iterator>); // C++20
};

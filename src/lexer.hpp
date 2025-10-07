#pragma once

#include <cassert>
#include <cctype>
#include <functional>
#include <iterator>
#include <string_view>
#include <variant>

#include "token.hpp"

class Lexer {
   public:
    explicit constexpr Lexer(std::string_view src) : src(src) {};
    inline auto begin() { return TokenIterator{*this, src, src.cbegin()}; }
    inline auto end() { return TokenIterator{*this}; }

   private:
    using It = std::string_view::const_iterator;
    std::string_view src;

    class TokenIterator {
       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Token;
        using reference_type = const value_type&;
        //     using difference_type = std::ptrdiff_t;

       private:
        std::reference_wrapper<Lexer> lexer;
        std::string_view src;
        It it;
        size_t col = 1;
        size_t line = 1;
        Token tok;

       public:
        TokenIterator(Lexer& lexer) : lexer(std::ref(lexer)) {}

        TokenIterator(Lexer& lexer, std::string_view src, It start)
            : lexer(std::ref(lexer)),
              src(src),
              it(start),
              tok(get_next_token()) {}

        inline TokenIterator(const TokenIterator& other) = default;
        inline TokenIterator(TokenIterator&& other) = default;

        inline TokenIterator& operator=(const TokenIterator&) = default;
        inline TokenIterator& operator=(TokenIterator&&) = default;

        [[nodiscard]] inline reference_type operator*() const noexcept {
            return tok;
        }

        inline TokenIterator& operator++() noexcept {
            tok = get_next_token();
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
        [[nodiscard]] inline Token get_next_token() {
            // return lexer.get().get_token(it, line, col, src);
            It end = src.cend();

            auto get_next_char = [this, &end]() -> char {
                ++it;
                if (it == end) return EOF;
                char c = *it;
                if (c == '\n') {
                    col = 0;
                    line++;
                } else
                    col++;
                return c;
            };

            size_t begin_distance = std::distance(src.cbegin(), it);
            size_t begin_line = line;
            size_t begin_col = col;
            Token::Type tok_type;
            if (it != end) {
                if (TokenMatcher matcher; matcher.enter_condition(*it)) {
                    do {
                        char c = get_next_char();
                        tok_type = matcher.feed(c);
                    } while (tok_type == Token::Type::NONE);
                } else {
                    get_next_char();
                    tok_type = Token::Type::ERROR;
                }
            } else {
                tok_type = Token::Type::NONE;
            }

            size_t end_distance = std::distance(src.cbegin(), it);
            std::string_view content =
                src.substr(begin_distance, end_distance - begin_distance);
            return {tok_type, content, begin_line, begin_col};
        }
    };
    // static_assert(std::forward_iterator<Iterator>); // C++20

    struct WhiteSpaceMatcher {
        bool enter_condition(char);
        Token::Type feed(char);
    };

    struct SlashMatcher {
        bool enter_condition(char);
        Token::Type feed(char);

       private:
        int32_t state = 0;
    };

    struct NumberMatcher {
        bool enter_condition(char);
        Token::Type feed(char);
    };

    struct IdentMatcher {
        bool enter_condition(char);
        Token::Type feed(char);

       private:
        std::string content;
    };

    struct StringMatcher {
        bool enter_condition(char);
        Token::Type feed(char);

       private:
        int32_t state = 0;
    };

    struct DelimiterMatcher {
        bool enter_condition(char);
        Token::Type feed(char);

       private:
        int32_t state = 0;
    };

    template <typename... Matcher>
    struct GeneralMatcher {
        bool enter_condition(char c) {
            return (handle_first<Matcher>(c) || ...);
        }

        Token::Type feed(char c) {
            return std::visit([c](auto& matcher) { return matcher.feed(c); },
                              state);
        }

       private:
        std::variant<Matcher...> state;

        template <typename M>
        bool handle_first(char c) {
            M matcher;
            bool ret = matcher.enter_condition(c);
            state = std::move(matcher);
            return ret;
        }
    };

    using TokenMatcher =
        GeneralMatcher<WhiteSpaceMatcher, SlashMatcher, NumberMatcher,
                       IdentMatcher, StringMatcher, DelimiterMatcher>;
};
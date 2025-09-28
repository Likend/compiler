#include "lexer.hpp"

#include <cassert>
#include <cctype>
// #include <concepts>
#include <iterator>
#include <string_view>

#include "token.hpp"

using It = std::string_view::const_iterator;

static void skip_whitespace(It& it) {
    while (std::isspace(*it)) ++it;
}

static void skip_comment(It& it, const It& end) {
#define STEP                   \
    do {                       \
        ++it;                  \
        if (it == end) return; \
    } while (0)

    assert(*it == '/');
    STEP;
    if (*it == '/') {
        do {
            STEP;
        } while (*it != '\n');
        STEP;  // skip \n
    } else if (*it == '*') {
        while (true) {
            STEP;
            if (*it == '*') {
                STEP;
                if (*it == '/') {
                    STEP;
                    break;
                }
            }
        }
    }
#undef STEP
}

struct ParserBase {
    constexpr ParserBase(It it) : it(it) {}

   protected:
    It it;
};

// template <typename T>
// concept Parser = requires(It it) {
//     { T{it} };
// } && requires(const T t) {
//     { t.first_condition() } -> std::same_as<bool>;
// } && requires(T t) {
//     { t.skip() } -> std::same_as<It>;
// } && requires(const T t, std::string_view content) {
//     { t.token_type(content) } -> std::same_as<Token::Type>;
// };

struct NumberParser : ParserBase {
    [[nodiscard]] bool first_condition() const { return std::isdigit(*it); }

    It skip() {
        if (*it == '0') {
            ++it;
            return it;
        }
        do ++it;
        while (std::isdigit(*it));
        return it;
    }

    Token::Type token_type([[maybe_unused]] std::string_view content) const {
        return Token::Type::INTCON;
    }
};

struct IdentParser : ParserBase {
    [[nodiscard]] bool first_condition() const {
        return std::isalpha(*it) || *it == '_';
    }

    It skip() {
        do ++it;
        while (std::isalnum(*it) || *it == '_');
        return it;
    }

    Token::Type token_type(std::string_view content) const {
        if (auto res = reserved_keywords_map.find(content);
            res != reserved_keywords_map.end())
            return res->second;
        else
            return Token::Type::IDENFR;
    }
};

struct StringParser : ParserBase {
    [[nodiscard]] bool first_condition() const { return *it == '"'; }

    It skip() {
        do ++it;
        while (*it != '"');
        return ++it;
    }

    Token::Type token_type(std::string_view content) const {
        return Token::Type::STRCON;
    }
};

struct DelimiterParser : ParserBase {
    [[nodiscard]] bool first_condition() const { return true; }  // default case

    It skip() {
        const char first_char = *it;
        ++it;
        switch (first_char) {
            case '!':  // '!' vs '!='
            case '<':  // '<' vs '<='
            case '>':  // '>' vs '>=
            case '=':  // '=' vs '=='
                if (*it == '=') ++it;
                break;
            case '&':  // '&' vs '&&'
                if (*it == '&') ++it;
                break;
            case '|':  // '|' vs '||'
                if (*it == '|') ++it;
                break;
        }
        return it;
    }

    Token::Type token_type(std::string_view content) const {
        if (auto res = delimiter_keywords_map.find(content);
            res != delimiter_keywords_map.end()) {
            return res->second;
        } else {
            return Token::Type::ERROR;
        }
    }
};

// static_assert(Parser<NumberParser>);
// static_assert(Parser<IdentParser>);
// static_assert(Parser<StringParser>);
// static_assert(Parser<DelimiterParser>);

// template <Parser... S>
template <typename... S>
Token parse_token(It it, std::string_view src) {
    static_assert(sizeof...(S) >= 1);

    size_t first = std::distance(src.cbegin(), it);
    std::string_view content;

    auto matcher = [first, src, &content](auto parser) -> Token::Type {
        if (parser.first_condition()) {
            It it = parser.skip();
            size_t last = std::distance(src.cbegin(), it);
            content = src.substr(first, last - first);
            return parser.token_type(content);
        }
        return Token::Type::NONE;
    };

    Token::Type token_type;
    (void)(((token_type = matcher(S{it})) != Token::Type::NONE) || ...);
    return {token_type, content};
}

// Returns the next token from the input by skipping whitespace and comments,
// then parsing the token using available parsers.
Token Lexer::Iterator::get_token(It it) const {
    while (true) {
        skip_whitespace(it);

        if (it == src.cend()) {
            return {};
        }
        if (*it != '/') {
            break;
        }
        skip_comment(it, src.cend());

        if (it == src.cend()) {
            return {};
        }
    }
    return parse_token<NumberParser, IdentParser, StringParser,
                       DelimiterParser>(it, src);
}
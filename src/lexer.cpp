#include "lexer.hpp"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <string_view>
// #include <concepts>

#include "token.hpp"

using It = std::string_view::const_iterator;

bool Lexer::WhiteSpaceMatcher::enter_condition(char c) {
    return std::isspace(c);
}

Token::Type Lexer::WhiteSpaceMatcher::feed(char c) {
    return (std::isspace(c)) ? Token::Type::NONE : Token::Type::WHITESPACE;
}

namespace SlashMatcherImpl {
enum class State : int32_t {
    START = 0,
    SINGLE_LINE_BRANCH,
    MULTI_LINE_BRANCH,
    WAIT_SLASH,
    TO_COMMENT,
    COMMENT,
    ERROR,
    DIV
};

static inline State next_state(char c, State state) {
    switch (state) {
        case State::START:
            switch (c) {
                case '/':
                    return State::SINGLE_LINE_BRANCH;
                case '*':
                    return State::MULTI_LINE_BRANCH;
                default:
                    return State::DIV;
            }
        case State::SINGLE_LINE_BRANCH:
            switch (c) {
                case '\n':
                    return State::TO_COMMENT;
                case EOF:
                    return State::COMMENT;
                default:
                    return State::SINGLE_LINE_BRANCH;
            }
        case State::MULTI_LINE_BRANCH:
            switch (c) {
                case '*':
                    return State::WAIT_SLASH;
                case EOF:
                    return State::ERROR;
                default:
                    return State::MULTI_LINE_BRANCH;
            }
        case State::WAIT_SLASH:
            switch (c) {
                case '/':
                    return State::TO_COMMENT;
                case EOF:
                    return State::ERROR;
                default:
                    return State::MULTI_LINE_BRANCH;
            }
        case State::TO_COMMENT:
            return State::COMMENT;
        default:
            assert(false);
            // __builtin_unreachable();
            return State::ERROR;
    }
}
}  // namespace SlashMatcherImpl

bool Lexer::SlashMatcher::enter_condition(char c) { return c == '/'; }

Token::Type Lexer::SlashMatcher::feed(char c) {
    using namespace SlashMatcherImpl;
    State& state = reinterpret_cast<State&>(this->state);
    state = next_state(c, state);
    switch (state) {
        case State::COMMENT:
            return Token::Type::COMMENT;
        case State::DIV:
            return Token::Type::DIV;
        case State::ERROR:
            return Token::Type::ERROR;
        default:
            return Token::Type::NONE;
    }
}

bool Lexer::NumberMatcher::enter_condition(char c) { return std::isdigit(c); }

Token::Type Lexer::NumberMatcher::feed(char c) {
    return std::isdigit(c) ? Token::Type::NONE : Token::Type::INTCON;
}

bool Lexer::IdentMatcher::enter_condition(char c) {
    if (std::isalpha(c) || c == '_') {
        content.push_back(c);
        return true;
    }
    return false;
}

Token::Type Lexer::IdentMatcher::feed(char c) {
    if (std::isalnum(c) || c == '_') {
        content.push_back(c);
        return Token::Type::NONE;
    } else {
        if (auto find = reserved_keywords_map.find(content);
            find != reserved_keywords_map.cend()) {
            return find->second;
        } else {
            return Token::Type::IDENFR;
        }
    }
}

namespace StringMatcherImpl {
enum class State : int32_t { START = 0, TO_END, END, ERROR };

static inline State handle_start(char c) {
    switch (c) {
        case '"':
            return State::TO_END;
        case EOF:
            return State::ERROR;
        default:
            return State::START;
    }
}

static State next_state(char c, State state) {
    switch (state) {
        case State::START:
            return handle_start(c);
        case State::TO_END:
            return State::END;
        case State::END:
        case State::ERROR:
            assert(false);
            // __builtin_unreachable();
            return State::ERROR;
    }
}
}  // namespace StringMatcherImpl

bool Lexer::StringMatcher::enter_condition(char c) { return c == '"'; }

Token::Type Lexer::StringMatcher::feed(char c) {
    using namespace StringMatcherImpl;
    State& state = reinterpret_cast<State&>(this->state);
    state = next_state(c, state);
    switch (state) {
        case State::END:
            return Token::Type::STRCON;
        case State::ERROR:
            return Token::Type::ERROR;
        default:
            return Token::Type::NONE;
    }
}

namespace DelimiterMatcherImpl {
enum Opt : int8_t {
    END = 0,
    TO_END,
    ERROR = -1,

    WAIT_EQ = '=',
    WAIT_AND = '&',
    WAIT_OR = '|',
};
struct alignas(4) State {
    int8_t first_char;
    int8_t second_char;
    Opt opt;
};
static_assert(sizeof(State) == 4);

static inline Opt first_opt(char c) {
    // clang-format off
    switch (c) {
        case '!': case '<':
        case '=': case '>':
            return WAIT_EQ;
        // single delimiter (except '/')
        case '+': case '-': case '*': case '%':
        case ',': case ';': case '(': case ')': 
        case '[': case ']': case '{': case '}':
            return TO_END;
        case '&':
            return WAIT_AND;
        case '|':
            return WAIT_OR;
        default:
            return ERROR;
    }
    // clang-format on
}

static inline Opt handle_wait(char c, char expect) {
    return (c == expect) ? TO_END : END;
}

static inline Opt second_opt(char c, Opt opt) {
    switch (opt) {
        case WAIT_EQ:
        case WAIT_AND:
        case WAIT_OR:
            return handle_wait(c, opt);
        case TO_END:
            return END;
        default:
            return ERROR;
    }
}

static constexpr int32_t from_str(const char* str) {
    return static_cast<uint8_t>(str[0]) | static_cast<uint8_t>(str[1]) << 8;
}
}  // namespace DelimiterMatcherImpl

bool Lexer::DelimiterMatcher::enter_condition(char c) {
    using namespace DelimiterMatcherImpl;
    State& state = reinterpret_cast<State&>(this->state);
    state.opt = first_opt(c);
    bool ret = state.opt != ERROR;
    if (ret) {
        state.first_char = c;
    }
    return ret;
}

Token::Type Lexer::DelimiterMatcher::feed(char c) {
    using namespace DelimiterMatcherImpl;
    State& state = reinterpret_cast<State&>(this->state);
    state.opt = second_opt(c, state.opt);
    switch (state.opt) {
        case END:
            switch (this->state) {
#define HANDLE_DELIMITER_KEYWORDS(X, Y) \
    case from_str(Y):                   \
        return Token::Type::X;

                EXPAND_DELIMITER_KEYWORDS

#undef HANDLE_DELIMITER_KEYWORDS
            }

        case ERROR:
            return Token::Type::ERROR;
        case TO_END:
            state.second_char = c;
            return Token::Type::NONE;
        default:
            assert(false);
            // __builtin_unreachable();
            return Token::Type::NONE;
    }
}

#include "lexer.hpp"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <string_view>
#include <variant>

#include "token.hpp"
#include "util/assert.hpp"

using It = std::string_view::const_iterator;

struct WhiteSpaceMatcher {
    bool enter_condition(char c) { return std::isspace(c); }

    Token::Type feed(char c) {
        return (std::isspace(c)) ? Token::Type::NONE : Token::Type::WHITESPACE;
    }
};

struct SlashMatcher {
    bool enter_condition(char c) { return c == '/'; }

    Token::Type feed(char c) {
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

   private:
    enum class State : int32_t {
        START = 0,
        SINGLE_LINE_BRANCH,
        MULTI_LINE_BRANCH,
        WAIT_SLASH,
        TO_COMMENT,
        COMMENT,
        ERROR,
        DIV
    } state = State::START;

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
                    case '*':
                        return State::WAIT_SLASH;
                    case EOF:
                        return State::ERROR;
                    default:
                        return State::MULTI_LINE_BRANCH;
                }
            case State::TO_COMMENT:
                return State::COMMENT;
            default:
                UNREACHABLE();
        }
    }
};

struct NumberMatcher {
    bool enter_condition(char c) { return std::isdigit(c); }

    Token::Type feed(char c) {
        return std::isdigit(c) ? Token::Type::NONE : Token::Type::INTCON;
    }
};

struct IdentMatcher {
    bool enter_condition(char c) {
        if (std::isalpha(c) || c == '_') {
            content.push_back(c);
            return true;
        }
        return false;
    }

    Token::Type feed(char c) {
        if (std::isalnum(c) || c == '_') {
            content.push_back(c);
            return Token::Type::NONE;
        } else {
            if (auto find = RESERVED_KEYWORDS_MAP.find(content);
                find != RESERVED_KEYWORDS_MAP.cend()) {
                return find->second;
            } else {
                return Token::Type::IDENFR;
            }
        }
    }

   private:
    std::string content;
};

struct StringMatcher {
    bool enter_condition(char c) { return c == '"'; }

    Token::Type feed(char c) {
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

   private:
    enum class State : int32_t {
        START = 0,
        TO_END,
        END,
        ERROR
    } state = State::START;

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
                UNREACHABLE();
                // return State::ERROR;
        }
        UNREACHABLE();
    }
};

struct DelimiterMatcher {
    static constexpr int32_t from_str(const char* str) {
        return static_cast<uint8_t>(str[0]) |
               (static_cast<uint8_t>(str[1]) << 8);
    }

    bool enter_condition(char c) {
        State& state = reinterpret_cast<State&>(this->state);
        state.opt    = first_opt(c);
        bool ret     = state.opt != ERROR;
        if (ret) {
            state.first_char = c;
        }
        return ret;
    }

    Token::Type feed(char c) {
        State& state = reinterpret_cast<State&>(this->state);
        state.opt    = second_opt(c, state.opt);
        switch (state.opt) {
            case END:
                switch (this->state) {
#define HANDLE_DELIMITER_KEYWORDS(X, Y) \
    case from_str(Y):                   \
        return Token::Type::X;

                    EXPAND_DELIMITER_KEYWORDS

#undef HANDLE_DELIMITER_KEYWORDS
                }
                return Token::Type::ERROR;
            case ERROR:
                return Token::Type::ERROR;
            case TO_END:
                state.second_char = c;
                return Token::Type::NONE;
            default:
                UNREACHABLE();
                // return Token::Type::NONE;
        }
    }

   private:
    int32_t state = 0;

    enum Opt : int8_t {
        END = 0,
        TO_END,
        ERROR = -1,

        WAIT_EQ  = '=',
        WAIT_AND = '&',
        WAIT_OR  = '|',
    };
    struct alignas(4) State {
        int8_t first_char;
        int8_t second_char;
        Opt    opt;
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
};

template <typename... Matcher>
struct GeneralMatcher {
    bool enter_condition(char c) { return (handle_first<Matcher>(c) || ...); }

    Token::Type feed(char c) {
        return std::visit([c](auto& matcher) { return matcher.feed(c); },
                          state);
    }

   private:
    std::variant<Matcher...> state;

    template <typename M>
    bool handle_first(char c) {
        M    matcher;
        bool ret = matcher.enter_condition(c);
        state    = std::move(matcher);
        return ret;
    }
};

using TokenMatcher =
    GeneralMatcher<WhiteSpaceMatcher, SlashMatcher, NumberMatcher, IdentMatcher,
                   StringMatcher, DelimiterMatcher>;

Token Lexer::TokenIterator::get_next_token() {
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

    size_t      begin_distance = std::distance(src.cbegin(), it);
    size_t      begin_line     = line;
    size_t      begin_col      = col;
    Token::Type tok_type;
    if (it != end) {
        if (TokenMatcher matcher; matcher.enter_condition(*it)) {
            do {
                char c   = get_next_char();
                tok_type = matcher.feed(c);
            } while (tok_type == Token::Type::NONE);
        } else {
            get_next_char();
            tok_type = Token::Type::ERROR;
        }
    } else {
        tok_type = Token::Type::NONE;
    }

    size_t           end_distance = std::distance(src.cbegin(), it);
    std::string_view content =
        src.substr(begin_distance, end_distance - begin_distance);
    return {tok_type, content, begin_line, begin_col};
}

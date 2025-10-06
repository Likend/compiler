#include "lexer.hpp"

#include <cassert>
#include <cctype>
#include <iterator>
#include <string_view>
#include <variant>
// #include <concepts>

#include "token.hpp"

using It = std::string_view::const_iterator;

enum class Result { CONTINUE, ACCEPT, REJECT };

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
    enum class State {
        START,
        SINGLE_LINE_BRACH,
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
                        return State::SINGLE_LINE_BRACH;
                    case '*':
                        return State::MULTI_LINE_BRANCH;
                    default:
                        return State::DIV;
                }
            case State::SINGLE_LINE_BRACH:
                switch (c) {
                    case '\n':
                        return State::TO_COMMENT;
                    case EOF:
                        return State::COMMENT;
                    default:
                        return State::SINGLE_LINE_BRACH;
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
                assert(false);  // unreachable
                return State::ERROR;
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
        content.push_back(c);
        return std::isalpha(c) || c == '_';
    }

    Token::Type feed(char c) {
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
    enum class State { START, TO_END, END, ERROR } state = State::START;

    static State next_state(char c, State state) {
        switch (state) {
            case State::START:
                return handle(c);
            case State::TO_END:
                return State::END;
            case State::END:
            case State::ERROR:
                assert(false);  // unreachable
                return State::ERROR;
        }
    }

    static inline State handle(char c) {
        switch (c) {
            case '"':
                return State::TO_END;
            case EOF:
                return State::ERROR;
            default:
                return State::START;
        }
    }
};

struct DelimiterMatcher {
    bool enter_condition(char c) {
        content.reserve(3);
        content.push_back(c);
        state = handle_start(c);
        return state != State::ERROR;
    }

    Token::Type feed(char c) {
        state = next_state(c, state);
        switch (state) {
            case State::END:
                if (auto find =
                        delimiter_keywords_map.find(std::string_view(content));
                    find != delimiter_keywords_map.cend()) {
                    return find->second;
                }  // else fall to error
            case State::ERROR:
                return Token::Type::ERROR;
            default:
                content.push_back(c);
                return Token::Type::NONE;
        }
    }

   private:
    std::string content;

    enum class State : char {
        START,
        TO_END,
        END,
        ERROR = -1,

        WAIT_EQ = '=',
        WAIT_AND = '&',
        WAIT_OR = '|',
    } state;

    static inline State next_state(char c, State state) {
        switch (state) {
            case State::WAIT_EQ:
            case State::WAIT_AND:
            case State::WAIT_OR:
                return handle_wait(c, static_cast<char>(state));
            case State::TO_END:
                return State::END;
            default:
                return State::ERROR;
        }
    }

    static inline State handle_start(char c) {
        // clang-format off
        switch (c) {
            case '!': case '<':
            case '=': case '>':
                return State::WAIT_EQ;
            // single delimiter (except '/')
            case '+': case '-': case '*': case '%':
            case ',': case ';': case '(': case ')': 
            case '[': case ']': case '{': case '}':
                return State::TO_END;
            case '&':
                return State::WAIT_AND;
            case '|':
                return State::WAIT_OR;
            default:
                return State::ERROR;
        }
        // clang-format on
    }

    static inline State handle_wait(char c, char expect) {
        return (c == expect) ? State::TO_END : State::END;
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
        M matcher;
        bool ret = matcher.enter_condition(c);
        state = std::move(matcher);
        return ret;
    }
};

using TokenMatcher =
    GeneralMatcher<WhiteSpaceMatcher, SlashMatcher, NumberMatcher, IdentMatcher,
                   StringMatcher, DelimiterMatcher>;

Token Lexer::Iterator::get_token(It& it, std::string_view src) {
    It end = src.cend();
    size_t first_pos = std::distance(src.cbegin(), it);
    size_t cur_pos;
    Token::Type tok_type;
    if (it != end) {
        if (TokenMatcher matcher; matcher.enter_condition(*it)) {
            do {
                ++it;
                char c = (it == end) ? EOF : *it;
                tok_type = matcher.feed(c);
            } while (tok_type == Token::Type::NONE);
        } else {
            ++it;
            tok_type = Token::Type::ERROR;
        }
    } else {
        tok_type = Token::Type::NONE;
    }

    cur_pos = std::distance(src.cbegin(), it);
    std::string_view content = src.substr(first_pos, cur_pos - first_pos);
    return {tok_type, content};
}
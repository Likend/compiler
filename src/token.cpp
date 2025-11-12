#include "token.hpp"

#include "util/assert.hpp"

std::string_view token_type_name(Token::Type type) {
    switch (type) {
#define HANDEL_CONSTANT_KEYWORD(X) \
    case Token::Type::X:           \
        return #X;
#define HANDLE_RESERVED_KEYWORD(X, V) \
    case Token::Type::X:              \
        return #X;
#define HANDLE_DELIMITER_KEYWORDS(X, V) \
    case Token::Type::X:                \
        return #X;

        EXPAND_CONSTANT_KEYWORDS
        EXPAND_RESERVED_KEYWORDS
        EXPAND_DELIMITER_KEYWORDS

#undef HANDEL_CONSTANT_KEYWORD
#undef HANDLE_RESERVED_KEYWORD
#undef HANDLE_DELIMITER_KEYWORDS

        case Token::Type::NONE:
        case Token::Type::ERROR:
            UNREACHABLE();
    }
    UNREACHABLE();
}

#define HANDLE_RESERVED_KEYWORD(X, V) {V, Token::Type::X},
const std::unordered_map<std::string_view, Token::Type> reserved_keywords_map =
    {EXPAND_RESERVED_KEYWORDS};
#undef HANDLE_RESERVED_KEYWORD

std::ostream& operator<<(std::ostream& os, Token::Type type) {
    if (type == Token::Type::ERROR)
        return os << "ERROR TOKEN!";
    else if (type != Token::Type::NONE)
        return os << token_type_name(type);
    else
        return os;
}

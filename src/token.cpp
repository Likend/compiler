#include "token.hpp"

const std::unordered_map<Token::Type, std::string_view> token_type_name_map = {
#define HANDEL_CONSTANT_KEYWORD(X) {Token::Type::X, #X},
#define HANDLE_RESERVED_KEYWORD(X, V) {Token::Type::X, #X},
#define HANDLE_DELIMITER_KEYWORDS(X, V) {Token::Type::X, #X},
    // clang-format off
    EXPAND_CONSTANT_KEYWORDS
    EXPAND_RESERVED_KEYWORDS
    EXPAND_DELIMITER_KEYWORDS
// clang-format on
#undef HANDEL_CONSTANT_KEYWORD
#undef HANDLE_RESERVED_KEYWORD
#undef HANDLE_DELIMITER_KEYWORDS
};

std::string_view token_type_name(Token::Type type) {
    assert(type != Token::Type::NONE);
    assert(type != Token::Type::ERROR);
    if (auto it = token_type_name_map.find(type);
        it != token_type_name_map.end()) {
        return it->second;
    } else
        return {};
}

#define HANDLE_RESERVED_KEYWORD(X, V) {V, Token::Type::X},
const std::unordered_map<std::string_view, Token::Type> reserved_keywords_map =
    {EXPAND_RESERVED_KEYWORDS};
#undef HANDLE_RESERVED_KEYWORD

#define HANDLE_DELIMITER_KEYWORDS(X, V) {V, Token::Type::X},
const std::unordered_map<std::string_view, Token::Type> delimiter_keywords_map =
    {EXPAND_DELIMITER_KEYWORDS};
#undef HANDLE_DELIMITER_KEYWORDS

std::ostream& operator<<(std::ostream& os, Token::Type type) {
    if (type == Token::Type::ERROR)
        return os << "ERROR TOKEN!";
    else if (type != Token::Type::NONE)
        return os << token_type_name(type);
    else
        return os;
}
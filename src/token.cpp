#include "token.hpp"

#define HANDLE_RESERVED_KEYWORD(X, V) {V, Token::Type::X},
const std::unordered_map<std::string_view, Token::Type> reserved_keywords_map =
    {EXPAND_RESERVED_KEYWORDS};
#undef HANDLE_RESERVED_KEYWORD

#define HANDLE_DELIMITER_KEYWORDS(X, V) {V, Token::Type::X},
const std::unordered_map<std::string_view, Token::Type> delimiter_keywords_map =
    {EXPAND_DELIMITER_KEYWORDS};
#undef HANDLE_DELIMITER_KEYWORDS

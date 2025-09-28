#pragma once

#include <cassert>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#define EXPAND_CONSTANT_KEYWORDS    \
    HANDEL_CONSTANT_KEYWORD(IDENFR) \
    HANDEL_CONSTANT_KEYWORD(INTCON) \
    HANDEL_CONSTANT_KEYWORD(STRCON)

#define EXPAND_RESERVED_KEYWORDS                    \
    HANDLE_RESERVED_KEYWORD(CONSTTK, "const")       \
    HANDLE_RESERVED_KEYWORD(INTTK, "int")           \
    HANDLE_RESERVED_KEYWORD(VOIDTK, "void")         \
    HANDLE_RESERVED_KEYWORD(STATICTK, "static")     \
    HANDLE_RESERVED_KEYWORD(BREAKTK, "break")       \
    HANDLE_RESERVED_KEYWORD(CONTINUETK, "continue") \
    HANDLE_RESERVED_KEYWORD(IFTK, "if")             \
    HANDLE_RESERVED_KEYWORD(ELSETK, "else")         \
    HANDLE_RESERVED_KEYWORD(FORTK, "for")           \
    HANDLE_RESERVED_KEYWORD(RETURNTK, "return")     \
    HANDLE_RESERVED_KEYWORD(MAINTK, "main")         \
    HANDLE_RESERVED_KEYWORD(PRINTFTK, "printf")

#define EXPAND_DELIMITER_KEYWORDS           \
    HANDLE_DELIMITER_KEYWORDS(NOT, "!")     \
    HANDLE_DELIMITER_KEYWORDS(AND, "&&")    \
    HANDLE_DELIMITER_KEYWORDS(OR, "||")     \
    HANDLE_DELIMITER_KEYWORDS(PLUS, "+")    \
    HANDLE_DELIMITER_KEYWORDS(MINU, "-")    \
    HANDLE_DELIMITER_KEYWORDS(MULT, "*")    \
    HANDLE_DELIMITER_KEYWORDS(DIV, "/")     \
    HANDLE_DELIMITER_KEYWORDS(MOD, "%")     \
    HANDLE_DELIMITER_KEYWORDS(SEMICN, ";")  \
    HANDLE_DELIMITER_KEYWORDS(COMMA, ",")   \
    HANDLE_DELIMITER_KEYWORDS(LSS, "<")     \
    HANDLE_DELIMITER_KEYWORDS(LEQ, "<=")    \
    HANDLE_DELIMITER_KEYWORDS(GRE, ">")     \
    HANDLE_DELIMITER_KEYWORDS(GEQ, ">=")    \
    HANDLE_DELIMITER_KEYWORDS(EQL, "==")    \
    HANDLE_DELIMITER_KEYWORDS(NEQ, "!=")    \
    HANDLE_DELIMITER_KEYWORDS(LPARENT, "(") \
    HANDLE_DELIMITER_KEYWORDS(RPARENT, ")") \
    HANDLE_DELIMITER_KEYWORDS(LBRACK, "[")  \
    HANDLE_DELIMITER_KEYWORDS(RBRACK, "]")  \
    HANDLE_DELIMITER_KEYWORDS(LBRACE, "{")  \
    HANDLE_DELIMITER_KEYWORDS(RBRACE, "}")  \
    HANDLE_DELIMITER_KEYWORDS(ASSIGN, "=")

class Token {
   public:
    // clang-format off
    enum class Type {
#define HANDEL_CONSTANT_KEYWORD(X) X,
#define HANDLE_RESERVED_KEYWORD(X, V) X,
#define HANDLE_DELIMITER_KEYWORDS(X, V) X,
        EXPAND_CONSTANT_KEYWORDS
        EXPAND_RESERVED_KEYWORDS
        EXPAND_DELIMITER_KEYWORDS
#undef HANDEL_CONSTANT_KEYWORD
#undef HANDLE_RESERVED_KEYWORD
#undef HANDLE_DELIMITER_KEYWORDS

        NONE, // not a token
        ERROR // error
    } type = Token::Type::NONE;
    // clang-format on

    std::string_view content;

    constexpr operator bool() const { return type == Token::Type::NONE; }
};

#define HANDEL_CONSTANT_KEYWORD(X) \
    [static_cast<std::underlying_type_t<Token::Type>>(Token::Type::X)] = #X,
#define HANDLE_RESERVED_KEYWORD(X, V) \
    [static_cast<std::underlying_type_t<Token::Type>>(Token::Type::X)] = #X,
#define HANDLE_DELIMITER_KEYWORDS(X, V) \
    [static_cast<std::underlying_type_t<Token::Type>>(Token::Type::X)] = #X,
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc99-designator"
constexpr static std::string_view token_type_name_map[] = {
    // clang-format off
    EXPAND_CONSTANT_KEYWORDS
    EXPAND_RESERVED_KEYWORDS
    EXPAND_DELIMITER_KEYWORDS
    // clang-format on
};
#pragma GCC diagnostic pop
#undef HANDEL_CONSTANT_KEYWORD
#undef HANDLE_RESERVED_KEYWORD
#undef HANDLE_DELIMITER_KEYWORDS

[[nodiscard]] constexpr std::string_view token_type_name(Token::Type type) {
    assert(type != Token::Type::NONE);
    assert(type != Token::Type::ERROR);
    return token_type_name_map[static_cast<std::underlying_type_t<Token::Type>>(
        type)];
}

inline std::ostream& operator<<(std::ostream& os, Token::Type type) {
    if (type == Token::Type::ERROR)
        return os << "ERROR TOKEN!";
    else if (type != Token::Type::NONE)
        return os << token_type_name(type);
    else
        return os;
}

extern const std::unordered_map<std::string_view, Token::Type>
    reserved_keywords_map;

extern const std::unordered_map<std::string_view, Token::Type>
    delimiter_keywords_map;

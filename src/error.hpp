#pragma once

#include <cstddef>
#include <vector>

#include "token.hpp"

struct ErrorInfo {
    enum class Type : char {
        ERROR_TOKEN                = 'a',
        REDEFINED_IDENT            = 'b',
        UNDEFINED_IDENT            = 'c',
        FUNC_ARG_COUNT_MISMATCH    = 'd',
        FUNC_ARG_TYPE_MISMATCH     = 'e',
        RETURN_MISMATCH            = 'f',
        MISSING_RETURN             = 'g',
        CONST_ASSIGNMENT           = 'h',
        MISSING_SEMICOLON          = 'i',
        MISSING_RPAREN             = 'j',
        MISSING_RBRACKET           = 'k',
        PRINTF_FORMAT_MISMATCH     = 'l',
        BREAK_CONTINUE_OUT_OF_LOOP = 'm'
    };
    Type        type;
    Token       token;
    std::string msg;
};

extern std::vector<ErrorInfo> error_infos;

static inline void reportError(ErrorInfo::Type error_type, const Token& token,
                               std::string msg) {
    error_infos.push_back(ErrorInfo{error_type, token, std::move(msg)});
}

static inline bool has_error() { return error_infos.size() != 0; }

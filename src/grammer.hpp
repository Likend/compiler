#pragma once

#include <memory>
#include <ostream>
#include <string_view>
#include <variant>
#include <vector>

#include "lexer.hpp"
#include "token.hpp"

struct ASTNode {
    enum class Type : int {
        COMP_UNIT,
        DECL,
        CONST_DECL,
        CONST_DEF,
        CONST_INIT_VAL,
        VAR_DECL,
        VAR_DEF,
        INIT_VAL,
        FUNC_DEF,
        MAIN_FUNC_DEF,
        FUNC_TYPE,
        FUNC_PARAMS,
        FUNC_PARAM,
        BLOCK,
        STMT,
        FOR_STMT,
        EXP,
        COND,
        L_VAL,
        PRIMARY_EXP,
        NUMBER,
        UNARY_EXP,
        UNARY_OP,
        FUNC_RPARAMS,
        MUL_EXP,
        ADD_EXP,
        REL_EXP,
        EQ_EXP,
        LAND_EXP,
        LOR_EXP,
        CONST_EXP,
    } type;

    using NodeType = std::variant<std::unique_ptr<ASTNode>, Token>;

    std::vector<NodeType> children;

    inline auto begin() { return children.begin(); }
    inline auto end() { return children.end(); }
};

struct ErrorInfo {
    char type;
    size_t line;
    size_t col;

    inline bool operator==(const ErrorInfo& other) const {
        return type == other.type && line == other.line && col == other.col;
    }

    inline bool operator<(const ErrorInfo& other) const {
        if (line < other.line)
            return true;
        else if (line == other.line) {
            if (col < other.col)
                return true;
            else if ((col == other.col) && (type < other.type))
                return true;
        }
        return false;
    }
};

namespace std {
template <>
struct hash<ErrorInfo> {
    size_t operator()(const ErrorInfo& info) const {
        return std::hash<char>{}(info.type) ^
               (std::hash<size_t>{}(info.line) < 1) ^
               (std::hash<size_t>{}(info.col) < 2);
    }
};
}  // namespace std

extern std::vector<ErrorInfo> error_infos;

#ifdef DEBUG_TOKEN_TYPE_NAME
[[nodiscard]] std::string_view astnode_type_name(ASTNode::Type type);

std::ostream& operator<<(std::ostream& os, ASTNode::Type type);
#endif
std::ostream& operator<<(std::ostream& os, const ASTNode& t);

std::unique_ptr<ASTNode> parse_grammer(Lexer::iterator& it);

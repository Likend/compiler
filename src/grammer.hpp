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

#ifdef DEBUG_TOKEN_TYPE_NAME
[[nodiscard]] std::string_view astnode_type_name(ASTNode::Type type);

std::ostream& operator<<(std::ostream& os, ASTNode::Type type);
#endif
std::ostream& operator<<(std::ostream& os, const ASTNode::NodeType& t);
using ParserValueType = std::vector<ASTNode::NodeType>;

std::unique_ptr<ASTNode> parse_grammer(Lexer::iterator& it);
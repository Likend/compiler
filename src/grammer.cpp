#include "grammer.hpp"

#include <memory>
#include <vector>

#include "lexer.hpp"
#include "token.hpp"

[[nodiscard]] std::string_view astnode_type_name(ASTNode::Type type) {
    switch (type) {
        case ASTNode::Type::COMP_UNIT:
            return "CompUnit";
        case ASTNode::Type::DECL:
            return "Decl";
        case ASTNode::Type::CONST_DECL:
            return "ConstDecl";
        case ASTNode::Type::CONST_DEF:
            return "ConstDef";
        case ASTNode::Type::CONST_INIT_VAL:
            return "ConstInitVal";
        case ASTNode::Type::VAR_DECL:
            return "VarDecl";
        case ASTNode::Type::VAR_DEF:
            return "VarDef";
        case ASTNode::Type::INIT_VAL:
            return "InitVal";
        case ASTNode::Type::FUNC_DEF:
            return "FuncDef";
        case ASTNode::Type::MAIN_FUNC_DEF:
            return "MainFuncDef";
        case ASTNode::Type::FUNC_TYPE:
            return "FuncType";
        case ASTNode::Type::FUNC_PARAMS:
            return "FuncFParams";
        case ASTNode::Type::FUNC_PARAM:
            return "FuncFParam";
        case ASTNode::Type::BLOCK:
            return "Block";
        case ASTNode::Type::STMT:
            return "Stmt";
        case ASTNode::Type::FOR_STMT:
            return "ForStmt";
        case ASTNode::Type::EXP:
            return "Exp";
        case ASTNode::Type::COND:
            return "Cond";
        case ASTNode::Type::L_VAL:
            return "LVal";
        case ASTNode::Type::PRIMARY_EXP:
            return "PrimaryExp";
        case ASTNode::Type::NUMBER:
            return "Number";
        case ASTNode::Type::UNARY_EXP:
            return "UnaryExp";
        case ASTNode::Type::UNARY_OP:
            return "UnaryOp";
        case ASTNode::Type::FUNC_RPARAMS:
            return "FuncRParams";
        case ASTNode::Type::MUL_EXP:
            return "MulExp";
        case ASTNode::Type::ADD_EXP:
            return "AddExp";
        case ASTNode::Type::REL_EXP:
            return "RelExp";
        case ASTNode::Type::EQ_EXP:
            return "EqExp";
        case ASTNode::Type::LAND_EXP:
            return "LAndExp";
        case ASTNode::Type::LOR_EXP:
            return "LOrExp";
        case ASTNode::Type::CONST_EXP:
            return "ConstExp";
    }
}

std::ostream& operator<<(std::ostream& os, ASTNode::Type type) {
    os << astnode_type_name(type);
    return os;
}

struct NodeTypeVisiter {
    std::ostream& os;

    NodeTypeVisiter(std::ostream& os) : os(os) {}

    void operator()(Token token) {
        os << "Token: " << token.type << ' ' << token.content << std::endl;
    }

    void operator()(const std::unique_ptr<ASTNode>& ast_node) {
        os << "ASTNode: " << (*ast_node).type << std::endl;
        for (const auto& i : (*ast_node).children) {
            os << i;
        }
    }
};

std::ostream& operator<<(std::ostream& os, const ASTNode::NodeType& t) {
    std::visit(NodeTypeVisiter{os}, t);
    return os;
}

using NodeType = ASTNode::NodeType;

template <ASTNode::Type type>
struct ParseNode {};

template <Token::Type type>
struct ParseToken {
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        const Token& token = *it;
        if (token.type == type) {
            container.emplace_back(token);
            ++it;
            return true;
        } else
            return false;
    }
};

template <typename... Parser>
struct Or {
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        return (Parser{}(it, container) || ...);
    }
};

template <typename... Parser>
struct Concat {
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        Lexer::iterator it_packup = it;
        size_t original_size = container.size();
        bool result = (Parser{}(it, container) && ...);
        if (!result) {
            it = it_packup;
            container.resize(original_size);
        }
        return result;
    }
};

template <typename Parser>
struct Optional {
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        Parser{}(it, container);
        return true;
    }
};

template <typename Parser>
struct Several {
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        bool result;
        do {
            result = Parser{}(it, container);
        } while (result);
        return true;
    }
};

template <ASTNode::Type type, typename Parser>
struct Define {
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        NodeType& node = container.emplace_back();
        auto& ast_node =
            node.emplace<std::unique_ptr<ASTNode>>(std::make_unique<ASTNode>());
        bool res = Parser{}(it, ast_node->children);
        if (!res) {
            container.pop_back();
            return false;
        }
        ast_node->type = type;
        return true;
    }
};

#define DEFINE(type, definition)         \
    template <>                          \
    struct ParseNode<ASTNode::Type::type> \
        : Define<ASTNode::Type::type, definition> {}

// #define DECLARE(type, defination) \ template<> struct ParseNode<A

#define OR(...) Or<__VA_ARGS__>
#define CONCAT(...) Concat<__VA_ARGS__>
#define OPTIONAL(parser) Optional<parser>
#define SEVERAL(parser) Several<parser>
#define TOKEN(type) ParseToken<Token::Type::type>
#define NODE(type) ParseNode<ASTNode::Type::type>

DEFINE(COMP_UNIT, CONCAT(SEVERAL(NODE(DECL)), SEVERAL(NODE(FUNC_DEF)),
                         NODE(MAIN_FUNC_DEF)));
DEFINE(DECL, OR(NODE(CONST_DECL), NODE(VAR_DECL)));
DEFINE(CONST_DECL, CONCAT(TOKEN(CONSTTK), TOKEN(INTTK), NODE(CONST_DEF),
                          SEVERAL(CONCAT(TOKEN(COMMA), NODE(CONST_DEF)))));
DEFINE(CONST_DEF,
       CONCAT(TOKEN(IDENFR),
              OPTIONAL(CONCAT(TOKEN(LBRACK), NODE(CONST_EXP), TOKEN(RBRACK))),
              TOKEN(ASSIGN), NODE(CONST_INIT_VAL)));
DEFINE(
    CONST_INIT_VAL,
    OR(NODE(CONST_EXP),
       CONCAT(TOKEN(LBRACE),
              OPTIONAL(CONCAT(NODE(CONST_EXP),
                              SEVERAL(CONCAT(TOKEN(COMMA), NODE(CONST_EXP))))),
              TOKEN(RBRACE))));
DEFINE(VAR_DECL,
       CONCAT(OPTIONAL(TOKEN(STATICTK)), TOKEN(INTTK), NODE(VAR_DEF),
              SEVERAL(CONCAT(TOKEN(COMMA), NODE(VAR_DEF))), TOKEN(SEMICN)));
DEFINE(VAR_DEF,
       CONCAT(TOKEN(IDENFR),
              OPTIONAL(CONCAT(TOKEN(LBRACK), NODE(CONST_EXP), TOKEN(RBRACK))),
              OPTIONAL(CONCAT(TOKEN(ASSIGN), NODE(INIT_VAL)))));
DEFINE(INIT_VAL,
       OR(NODE(EXP),
          CONCAT(TOKEN(LBRACE),
                 OPTIONAL(CONCAT(NODE(EXP),
                                 SEVERAL(CONCAT(TOKEN(COMMA), NODE(EXP))))),
                 TOKEN(RBRACE))));
DEFINE(FUNC_DEF,
       CONCAT(NODE(FUNC_TYPE), TOKEN(IDENFR), TOKEN(LPARENT),
              OPTIONAL(NODE(FUNC_PARAMS)), TOKEN(RPARENT), NODE(BLOCK)));
DEFINE(MAIN_FUNC_DEF, CONCAT(TOKEN(INTTK), TOKEN(MAINTK), TOKEN(LPARENT),
                             TOKEN(RPARENT), NODE(BLOCK)));
DEFINE(FUNC_TYPE, OR(TOKEN(VOIDTK), TOKEN(INTTK)));
DEFINE(FUNC_PARAMS, CONCAT(NODE(FUNC_PARAM),
                           SEVERAL(CONCAT(TOKEN(COMMA), NODE(FUNC_PARAM)))));
DEFINE(FUNC_PARAM, CONCAT(TOKEN(INTTK), TOKEN(IDENFR),
                          OPTIONAL(CONCAT(TOKEN(LBRACK), TOKEN(RBRACK)))));
DEFINE(BLOCK, CONCAT(TOKEN(LBRACE), SEVERAL(OR(NODE(DECL), NODE(STMT))),
                     TOKEN(RBRACE)));
DEFINE(STMT, OR(CONCAT(NODE(L_VAL), TOKEN(ASSIGN), NODE(EXP), TOKEN(SEMICN)),
                CONCAT(OPTIONAL(NODE(EXP)), TOKEN(SEMICN)), NODE(BLOCK),
                CONCAT(TOKEN(IFTK), TOKEN(LPARENT), NODE(COND), TOKEN(RPARENT),
                       NODE(STMT), OPTIONAL(CONCAT(TOKEN(ELSETK), NODE(STMT)))),
                CONCAT(TOKEN(FORTK), TOKEN(LPARENT), OPTIONAL(NODE(FOR_STMT)),
                       TOKEN(SEMICN), OPTIONAL(NODE(COND)), TOKEN(SEMICN),
                       OPTIONAL(NODE(FOR_STMT)), TOKEN(RPARENT), NODE(STMT)),
                CONCAT(TOKEN(BREAKTK), TOKEN(SEMICN)),
                CONCAT(TOKEN(CONTINUETK), TOKEN(SEMICN)),
                CONCAT(TOKEN(RETURNTK), OPTIONAL(NODE(EXP))),
                CONCAT(TOKEN(PRINTFTK), TOKEN(LPARENT), TOKEN(STRCON),
                       SEVERAL(CONCAT(TOKEN(COMMA), NODE(EXP))), TOKEN(RPARENT),
                       TOKEN(SEMICN))));
DEFINE(FOR_STMT, CONCAT(NODE(L_VAL), TOKEN(ASSIGN), NODE(EXP),
                        SEVERAL(CONCAT(TOKEN(COMMA), NODE(L_VAL), TOKEN(ASSIGN),
                                       NODE(EXP)))));
DEFINE(EXP, NODE(ADD_EXP));
DEFINE(COND, NODE(LOR_EXP));
DEFINE(L_VAL, CONCAT(TOKEN(IDENFR), OPTIONAL(CONCAT(TOKEN(LBRACK), NODE(EXP),
                                                    TOKEN(RBRACK)))));
DEFINE(PRIMARY_EXP, OR(CONCAT(TOKEN(LPARENT), NODE(EXP), TOKEN(RPARENT)),
                       NODE(L_VAL), NODE(NUMBER)));
DEFINE(NUMBER, TOKEN(INTCON));
DEFINE(UNARY_EXP, OR(NODE(PRIMARY_EXP),
                     CONCAT(TOKEN(IDENFR), TOKEN(LPARENT),
                            OPTIONAL(NODE(FUNC_RPARAMS)), TOKEN(RPARENT)),
                     CONCAT(NODE(UNARY_OP), NODE(UNARY_EXP))));
DEFINE(UNARY_OP, OR(TOKEN(PLUS), TOKEN(MINU), TOKEN(NOT)));
DEFINE(FUNC_RPARAMS,
       CONCAT(NODE(EXP), SEVERAL(CONCAT(TOKEN(COMMA), NODE(EXP)))));
DEFINE(MUL_EXP, CONCAT(NODE(UNARY_EXP),
                       SEVERAL(CONCAT(OR(TOKEN(MULT), TOKEN(DIV), TOKEN(MOD)),
                                      NODE(UNARY_EXP)))));
DEFINE(ADD_EXP,
       CONCAT(NODE(MUL_EXP),
              SEVERAL(CONCAT(OR(TOKEN(PLUS), TOKEN(MINU)), NODE(MUL_EXP)))));
DEFINE(REL_EXP,
       CONCAT(NODE(ADD_EXP),
              SEVERAL(CONCAT(OR(TOKEN(LSS), TOKEN(LEQ), TOKEN(GRE), TOKEN(GEQ)),
                             NODE(ADD_EXP)))));
DEFINE(EQ_EXP, CONCAT(NODE(REL_EXP), SEVERAL(CONCAT(OR(TOKEN(EQL), TOKEN(NEQ)),
                                                    NODE(REL_EXP)))));
DEFINE(LAND_EXP,
       CONCAT(NODE(EQ_EXP), SEVERAL(CONCAT(TOKEN(AND), NODE(EQ_EXP)))));
DEFINE(LOR_EXP,
       CONCAT(NODE(LOR_EXP), SEVERAL(CONCAT(TOKEN(OR), NODE(LOR_EXP)))));
DEFINE(CONST_EXP, NODE(ADD_EXP));

std::unique_ptr<ASTNode> parse_grammer(Lexer::iterator& it) {
    std::vector<NodeType> temp;
    bool result = ParseNode<ASTNode::Type::COMP_UNIT>{}(it, temp);
    if (!result) return nullptr;
    NodeType& node = temp.at(0);
    auto& ast = std::get<std::unique_ptr<ASTNode>>(node);
    return std::move(ast);
}
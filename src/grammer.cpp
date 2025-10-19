#include "grammer.hpp"

#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>
#include <variant>
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
        os << token.type << ' ' << token.content << std::endl;
    }

    void operator()(const std::unique_ptr<ASTNode>& ast_node) {
        os << *ast_node;
    }
};

std::ostream& operator<<(std::ostream& os, const ASTNode& node) {
#ifdef NDEBUG
    for (const auto& i : node.children) {
        std::visit(NodeTypeVisiter{os}, i);
    }
    os << '<' << node.type << '>' << std::endl;
    return os;
#else
    static uint32_t ident = 0;
    os << '<' << node.type << '>' << std::endl;
    ident++;
    for (const auto& i : node.children) {
        for (size_t i = 0; i < ident; i++) {
            os << '\t';
        }
        std::visit(NodeTypeVisiter{os}, i);
    }
    ident--;
    return os;
#endif
}

std::vector<ErrorInfo> error_infos;

using NodeType = ASTNode::NodeType;

template <ASTNode::Type type>
struct ParseNode {};

template <Token::Type type>
struct ParseToken {
    template <bool strict = true>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        const Token& token = *it;
        if (token.type == type) {
            container.emplace_back(token);
            ++it;
            return true;
        }
        if (token.type == Token::Type::ERROR) {
            if ((*token.content.data() == '&' && type == Token::Type::AND) ||
                (*token.content.data() == '|' && type == Token::Type::OR)) {
                error_infos.emplace_back(ErrorInfo{'a', token.line, token.col});
                container.emplace_back(token);
                ++it;
                return true;
            }
        }
        return false;
    }
};

static const Token& find_last_token(const std::vector<NodeType>& container);
struct FindLastTokenVisitor {
    const Token& operator()(const std::unique_ptr<ASTNode>& ast_node) {
        assert(!ast_node->children.empty());
        return find_last_token(ast_node->children);
    }
    const Token& operator()(const Token& token) { return token; }
};

static const Token& find_last_token(const std::vector<NodeType>& container) {
    const NodeType& last_node = container.back();
    return std::visit(FindLastTokenVisitor{}, last_node);
}

template <Token::Type type, char error_type>
struct ParseTokenRequired {
    template <bool strict = true>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        bool result =
            ParseToken<type>{}.template operator()<true>(it, container);
        const Token& last_token = find_last_token(container);
        if (!result) {
            error_infos.emplace_back(
                ErrorInfo{error_type, last_token.line,
                          last_token.col + last_token.content.size()});
        }
        return true;
    }
};

template <typename... Parser>
struct Or {
    template <bool strict = true>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        return (Parser{}.template operator()<true>(it, container) || ...);
    }
};

template <typename... Parser>
struct Concat {
    template <bool strict = false>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        return invoke(it, container, std::integral_constant<bool, strict>{});
    }

   private:
    inline bool invoke(Lexer::iterator& it, std::vector<NodeType>& container,
                       std::true_type) {
        Lexer::iterator it_packup = it;
        size_t original_size = container.size();
        bool result =
            (Parser{}.template operator()<true>(it, container) && ...);
        if (!result) {
            it = it_packup;
            container.resize(original_size);
        }
        return result;
    }

    inline bool invoke(Lexer::iterator& it, std::vector<NodeType>& container,
                       std::false_type) {
        return (Parser{}.template operator()<false>(it, container) && ...);
    }
};

template <typename Parser>
struct Optional {
    template <bool strict = true>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        Parser{}.template operator()<true>(it, container);
        return true;
    }
};

template <typename Parser>
struct Several {
    template <bool strict = true>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        bool result;
        do {
            result = Parser{}.template operator()<true>(it, container);
        } while (result);
        return true;
    }
};

template <ASTNode::Type type, typename Parser>
struct Define {
    template <bool strict = false>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        auto& ast_node =
            container.emplace_back().emplace<std::unique_ptr<ASTNode>>(
                std::make_unique<ASTNode>());
        bool res = Parser{}.template operator()<strict>(it, ast_node->children);
        if constexpr (strict) {
            if (!res) {
                container.pop_back();
                return false;
            }
        }
        ast_node->type = type;
        return res;
    }
};

#define DEFINE(type, definition)          \
    template <>                           \
    struct ParseNode<ASTNode::Type::type> \
        : Define<ASTNode::Type::type, definition> {}

#define ASSIGN(type, definition) \
    template <>                  \
    struct ParseNode<ASTNode::Type::type> : definition {}

#define OR(...) Or<__VA_ARGS__>
#define CONCAT(...) Concat<__VA_ARGS__>
#define OPTIONAL(parser) Optional<parser>
#define SEVERAL(parser) Several<parser>
#define TOKEN(type) ParseToken<Token::Type::type>
#define TOKEN_R(type, err) ParseTokenRequired<Token::Type::type, err>
#define NODE(type) ParseNode<ASTNode::Type::type>

DEFINE(COMP_UNIT, SEVERAL(OR(NODE(FUNC_DEF), NODE(MAIN_FUNC_DEF), NODE(DECL))));
ASSIGN(DECL, OR(NODE(CONST_DECL), NODE(VAR_DECL)));
DEFINE(CONST_DECL, CONCAT(TOKEN(CONSTTK), TOKEN(INTTK), NODE(CONST_DEF),
                          SEVERAL(CONCAT(TOKEN(COMMA), NODE(CONST_DEF))),
                          TOKEN_R(SEMICN, 'i')));
DEFINE(CONST_DEF, CONCAT(TOKEN(IDENFR),
                         OPTIONAL(CONCAT(TOKEN(LBRACK), NODE(CONST_EXP),
                                         TOKEN_R(RBRACK, 'k'))),
                         TOKEN(ASSIGN), NODE(CONST_INIT_VAL)));
DEFINE(
    CONST_INIT_VAL,
    OR(NODE(CONST_EXP),
       CONCAT(TOKEN(LBRACE),
              OPTIONAL(CONCAT(NODE(CONST_EXP),
                              SEVERAL(CONCAT(TOKEN(COMMA), NODE(CONST_EXP))))),
              TOKEN(RBRACE))));
DEFINE(VAR_DECL, CONCAT(OPTIONAL(TOKEN(STATICTK)), TOKEN(INTTK), NODE(VAR_DEF),
                        SEVERAL(CONCAT(TOKEN(COMMA), NODE(VAR_DEF))),
                        TOKEN_R(SEMICN, 'i')));
DEFINE(VAR_DEF, CONCAT(TOKEN(IDENFR),
                       OPTIONAL(CONCAT(TOKEN(LBRACK), NODE(CONST_EXP),
                                       TOKEN_R(RBRACK, 'k'))),
                       OPTIONAL(CONCAT(TOKEN(ASSIGN), NODE(INIT_VAL)))));
DEFINE(INIT_VAL,
       OR(NODE(EXP),
          CONCAT(TOKEN(LBRACE),
                 OPTIONAL(CONCAT(NODE(EXP),
                                 SEVERAL(CONCAT(TOKEN(COMMA), NODE(EXP))))),
                 TOKEN(RBRACE))));
DEFINE(FUNC_DEF,
       CONCAT(NODE(FUNC_TYPE), TOKEN(IDENFR), TOKEN(LPARENT),
              OPTIONAL(NODE(FUNC_PARAMS)), TOKEN_R(RPARENT, 'j'), NODE(BLOCK)));
DEFINE(MAIN_FUNC_DEF, CONCAT(TOKEN(INTTK), TOKEN(MAINTK), TOKEN(LPARENT),
                             TOKEN_R(RPARENT, 'j'), NODE(BLOCK)));
DEFINE(FUNC_TYPE, OR(TOKEN(VOIDTK), TOKEN(INTTK)));
DEFINE(FUNC_PARAMS, CONCAT(NODE(FUNC_PARAM),
                           SEVERAL(CONCAT(TOKEN(COMMA), NODE(FUNC_PARAM)))));
DEFINE(FUNC_PARAM,
       CONCAT(TOKEN(INTTK), TOKEN(IDENFR),
              OPTIONAL(CONCAT(TOKEN(LBRACK), TOKEN_R(RBRACK, 'k')))));
DEFINE(BLOCK, CONCAT(TOKEN(LBRACE), SEVERAL(OR(NODE(DECL), NODE(STMT))),
                     TOKEN(RBRACE)));
DEFINE(STMT,
       OR(CONCAT(NODE(L_VAL), TOKEN(ASSIGN), NODE(EXP), TOKEN_R(SEMICN, 'i')),
          CONCAT(NODE(EXP), TOKEN_R(SEMICN, 'i')), TOKEN(SEMICN), NODE(BLOCK),
          CONCAT(TOKEN(IFTK), TOKEN(LPARENT), NODE(COND), TOKEN_R(RPARENT, 'j'),
                 NODE(STMT), OPTIONAL(CONCAT(TOKEN(ELSETK), NODE(STMT)))),
          CONCAT(TOKEN(FORTK), TOKEN(LPARENT), OPTIONAL(NODE(FOR_STMT)),
                 TOKEN(SEMICN), OPTIONAL(NODE(COND)), TOKEN(SEMICN),
                 OPTIONAL(NODE(FOR_STMT)), TOKEN_R(RPARENT, 'j'), NODE(STMT)),
          CONCAT(TOKEN(BREAKTK), TOKEN_R(SEMICN, 'i')),
          CONCAT(TOKEN(CONTINUETK), TOKEN_R(SEMICN, 'i')),
          CONCAT(TOKEN(RETURNTK), OPTIONAL(NODE(EXP)), TOKEN_R(SEMICN, 'i')),
          CONCAT(TOKEN(PRINTFTK), TOKEN(LPARENT), TOKEN(STRCON),
                 SEVERAL(CONCAT(TOKEN(COMMA), NODE(EXP))),
                 TOKEN_R(RPARENT, 'j'), TOKEN_R(SEMICN, 'i'))));
DEFINE(FOR_STMT, CONCAT(NODE(L_VAL), TOKEN(ASSIGN), NODE(EXP),
                        SEVERAL(CONCAT(TOKEN(COMMA), NODE(L_VAL), TOKEN(ASSIGN),
                                       NODE(EXP)))));
DEFINE(EXP, NODE(ADD_EXP));
DEFINE(CONST_EXP, NODE(ADD_EXP));
DEFINE(COND, NODE(LOR_EXP));
DEFINE(L_VAL, CONCAT(TOKEN(IDENFR), OPTIONAL(CONCAT(TOKEN(LBRACK), NODE(EXP),
                                                    TOKEN_R(RBRACK, 'k')))));
DEFINE(PRIMARY_EXP, OR(CONCAT(TOKEN(LPARENT), NODE(EXP), TOKEN_R(RPARENT, 'j')),
                       NODE(L_VAL), NODE(NUMBER)));
DEFINE(NUMBER, TOKEN(INTCON));
DEFINE(UNARY_EXP,
       OR(CONCAT(TOKEN(IDENFR), TOKEN(LPARENT), OPTIONAL(NODE(FUNC_RPARAMS)),
                 TOKEN_R(RPARENT, 'j')),
          NODE(PRIMARY_EXP), CONCAT(NODE(UNARY_OP), NODE(UNARY_EXP))));
DEFINE(UNARY_OP, OR(TOKEN(PLUS), TOKEN(MINU), TOKEN(NOT)));
DEFINE(FUNC_RPARAMS,
       CONCAT(NODE(EXP), SEVERAL(CONCAT(TOKEN(COMMA), NODE(EXP)))));

template <ASTNode::Type type, typename UpperParser, typename... OpParser>
struct ParseBinExp {
    // A -> B | A op B
    template <bool strict = false>
    bool operator()(Lexer::iterator& it, std::vector<NodeType>& container) {
        auto& ast_node =
            container.emplace_back().emplace<std::unique_ptr<ASTNode>>(
                std::make_unique<ASTNode>());
        ast_node->type = type;

        // parse first B
        bool result =
            UpperParser{}.template operator()<strict>(it, ast_node->children);
        if (!result) return false;

        while (true) {
            // parse token
            std::vector<NodeType> new_container;
            new_container.reserve(3);
            auto& new_ast_node =
                new_container.emplace_back().emplace<std::unique_ptr<ASTNode>>(
                    std::make_unique<ASTNode>());  // spare space for first B.
            new_ast_node->type = type;

            Lexer::iterator it_packup = it;
            result = (OpParser{}.template operator()<true>(it, new_container) ||
                      ...);
            if (!result) return true;

            result =
                UpperParser{}.template operator()<strict>(it, new_container);
            if (!result) {
                it = it_packup;
                return true;
            }

            // swap first A
            new_ast_node->children = std::move(ast_node->children);
            ast_node->children = std::move(new_container);
        }
    }
};

#define BINEXP(type, upper_parser, ...) \
    ParseBinExp<ASTNode::Type::type, upper_parser, __VA_ARGS__>

ASSIGN(MUL_EXP,
       BINEXP(MUL_EXP, NODE(UNARY_EXP), TOKEN(MULT), TOKEN(DIV), TOKEN(MOD)));
ASSIGN(ADD_EXP, BINEXP(ADD_EXP, NODE(MUL_EXP), TOKEN(PLUS), TOKEN(MINU)));
ASSIGN(REL_EXP, BINEXP(REL_EXP, NODE(ADD_EXP), TOKEN(LSS), TOKEN(LEQ),
                       TOKEN(GRE), TOKEN(GEQ)));
ASSIGN(EQ_EXP, BINEXP(EQ_EXP, NODE(REL_EXP), TOKEN(EQL), TOKEN(NEQ)));
ASSIGN(LAND_EXP, BINEXP(LAND_EXP, NODE(EQ_EXP), TOKEN(AND)));
ASSIGN(LOR_EXP, BINEXP(LOR_EXP, NODE(LAND_EXP), TOKEN(OR)));

std::unique_ptr<ASTNode> parse_grammer(Lexer::iterator& it) {
    std::vector<NodeType> temp;
    bool result = ParseNode<ASTNode::Type::COMP_UNIT>{}(it, temp);
    if (!result) return nullptr;
    NodeType& node = temp.at(0);
    auto& ast = std::get<std::unique_ptr<ASTNode>>(node);
    return std::move(ast);
}

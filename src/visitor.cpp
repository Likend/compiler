// #include "visitor.hpp"

// #include <memory>
// #include <sstream>
// #include <stdexcept>
// #include <variant>

// #include "error.hpp"
// #include "grammer.hpp"
// #include "symbol_table.hpp"
// #include "token.hpp"
// #include "util/assert.hpp"

// using NodePtr = std::unique_ptr<ASTNode>;

#define assert_ast_type(t, node)                                        \
    do {                                                                \
        if ((node).type != ASTNode::Type::t) {                          \
            std::stringstream ss;                                       \
            ss << "Assert AST type failed! Expect " << ASTNode::Type::t \
               << " get " << (node).type << ". ";                       \
            ANNOTATE_LOCATION(ss);                                        \
            throw std::runtime_error(ss.str());                         \
        }                                                               \
    } while (0)

#define assert_token_type(t, token)                                     \
    do {                                                                \
        if ((token).type != Token::Type::t) {                           \
            std::stringstream ss;                                       \
            ss << "Assert Token type failed! Expect " << Token::Type::t \
               << " get " << (token).type << ". ";                      \
            ANNOTATE_LOCATION(ss);                                        \
            throw std::runtime_error(ss.str());                         \
        }                                                               \
    } while (0)

// void Visitor::operator()(const ASTNode& node) { invoke_comp_unit(node); }

// void Visitor::invoke_comp_unit(const ASTNode& node) {
//     assert_ast_type(COMP_UNIT, node);
//     for (const auto& child : node.children) {
//         const NodePtr& ptr = std::get<NodePtr>(child);
//         switch (ptr->type) {
//             case ASTNode::Type::FUNC_DEF:
//             case ASTNode::Type::MAIN_FUNC_DEF:
//                 invoke_func_def(*ptr);
//                 break;
//             case ASTNode::Type::CONST_DECL:
//             case ASTNode::Type::VAR_DECL:
//                 invoke_var_decl(*ptr);
//                 break;
//             default:
//                 std::stringstream ss;
//                 ss << "Unexpected type: " << ptr->type;
//                 throw std::runtime_error(ss.str());
//         }
//     }
// }

// void Visitor::invoke_func_def(const ASTNode& node) {
//     bool main_func;
//     if (node.type == ASTNode::Type::MAIN_FUNC_DEF) {
//         main_func = true;
//     } else if (node.type == ASTNode::Type::FUNC_DEF) {
//         main_func = false;
//     } else {
//         UNREACHABLE();
//     }

//     bool need_return;
//     if (!main_func) {
//         const NodePtr& func_type_node = std::get<NodePtr>(node.children[0]);
//         assert_ast_type(FUNC_TYPE, *func_type_node);
//         const Token& func_type =
//             std::get<Token>(func_type_node->children.at(0));
//         if (func_type.type == Token::Type::VOIDTK) {
//             need_return = false;
//         } else if (func_type.type == Token::Type::INTTK) {
//             need_return = true;
//         } else {
//             UNREACHABLE();
//         }
//     } else {
//         const Token& inttk = std::get<Token>(node.children[0]);
//         assert_token_type(INTTK, inttk);
//         need_return = true;
//     }

//     const Token& ident = std::get<Token>(node.children[1]);
//     if (main_func) {
//         assert_token_type(MAINTK, ident);
//     } else {
//         add_symbol(ident, {}, SymbolType::INT_FUNC);
//     }

//     push_scope();
//     if (const NodePtr* func_params = std::get_if<NodePtr>(&node.children[3])) {
//         invoke_func_params(**func_params);
//     }

//     const NodePtr& block = std::get<NodePtr>(node.children.back());
//     invoke_block(*block, need_return);
//     pop_scope();
// }

// void Visitor::invoke_func_params(const ASTNode& node) {
//     assert_ast_type(FUNC_PARAMS, node);
//     auto it = node.children.cbegin();

//     const NodePtr& func_param = std::get<NodePtr>(*(it++));
//     invoke_func_param(*func_param);

//     while (it != node.children.cend()) {
//         const Token& comma = std::get<Token>(*(it++));
//         assert_token_type(COMMA, comma);

//         const NodePtr& func_param = std::get<NodePtr>(*(it++));
//         invoke_func_param(*func_param);
//     }
// }

// void Visitor::invoke_func_param(const ASTNode& node) {
//     assert_ast_type(FUNC_PARAM, node);
//     auto it = node.children.cbegin();

//     const Token& inttk = std::get<Token>(*(it++));
//     assert_token_type(INTTK, inttk);

//     const Token& idenfr = std::get<Token>(*(it++));
//     assert_token_type(IDENFR, idenfr);

//     [[maybe_unused]] bool array = false;
//     if (it != node.children.cend()) {
//         array = true;
//         const Token& lbrack = std::get<Token>(*(it++));
//         assert_token_type(LBRACK, lbrack);

//         const Token& rbrack = std::get<Token>(*(it++));
//         assert_token_type(RBRACK, rbrack);
//     }

//     add_symbol(idenfr, {}, SymbolType::INT);
// }

// void Visitor::invoke_var_decl(const ASTNode& node) {
//     if (node.type != ASTNode::Type::CONST_DECL &&
//         node.type != ASTNode::Type::VAR_DECL) {
//         UNREACHABLE();
//     }

//     [[maybe_unused]] bool const_flag = false;
//     [[maybe_unused]] bool static_flag = false;

//     auto it = node.children.cbegin();
//     const Token* try_get = &std::get<Token>(*(it++));
//     if (try_get->type == Token::Type::CONSTTK) {
//         const_flag = true;
//         try_get = &std::get<Token>(*(it++));
//     } else if (try_get->type == Token::Type::STATICTK) {
//         static_flag = true;
//         try_get = &std::get<Token>(*(it++));
//     }

//     const Token& inttk = *try_get;
//     assert_token_type(INTTK, inttk);

//     const NodePtr& var_def = std::get<NodePtr>(*(it++));
//     invoke_var_def(*var_def);

//     while (1) {
//         const Token& try_comma = std::get<Token>(*(it++));
//         if (try_comma.type == Token::Type::SEMICN) break;
//         assert_token_type(COMMA, try_comma);

//         const NodePtr& var_def = std::get<NodePtr>(*(it++));
//         invoke_var_def(*var_def);
//     }
// }

// void Visitor::invoke_var_def(const ASTNode& node) {
//     if (node.type != ASTNode::Type::VAR_DEF &&
//         node.type != ASTNode::Type::CONST_DEF) {
//         UNREACHABLE();
//     }

//     auto it = node.children.cbegin();
//     const Token& idenfr = std::get<Token>(*(it++));
//     assert_token_type(IDENFR, idenfr);

//     // TODO
// }

// void Visitor::invoke_block(const ASTNode& node,
//                            [[maybe_unused]] bool require_return) {
//     assert_ast_type(BLOCK, node);
//     // TODO
// }

// void Visitor::push_scope() {
//     scope_stack.push(current_scope);
//     current_scope = new_define_scope = new_define_scope + 1;
// }

// void Visitor::pop_scope() {
//     current_scope = scope_stack.top();
//     scope_stack.pop();
// }

// void Visitor::add_symbol(Token idenfr_token, SymbolAttr attr, SymbolType type,
//                          char error_type) {
//     assert_token_type(IDENFR, idenfr_token);
//     bool result = symbol_table.try_add_symbol(idenfr_token.content, attr);
//     if (!result) {
//         error_infos.emplace_back(
//             ErrorInfo{error_type, idenfr_token.line, idenfr_token.col});
//     } else {
//         records.emplace_back(
//             SymbolRecord{current_scope, idenfr_token.content, type});
//     }
// }
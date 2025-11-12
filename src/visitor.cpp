#include "visitor.hpp"

#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <variant>

#include "error.hpp"
#include "grammer.hpp"
#include "symbol_table.hpp"
#include "token.hpp"
#include "util/assert.hpp"
#include "util/lambda_overload.hpp"

using NodePtr = std::unique_ptr<ASTNode>;

#define UNEXPECTED_TYPE(type)                \
    do {                                     \
        std::stringstream ss;                \
        ss << "Unexpected type: " << (type); \
        ANNOTATE_LOCATION(ss);               \
        throw std::runtime_error(ss.str());  \
    } while (0)

#define ASSERT_AST_TYPE(t, node)                                        \
    do {                                                                \
        if ((node).type != ASTNode::Type::t) {                          \
            std::stringstream ss;                                       \
            ss << "Assert AST type failed! Expect " << ASTNode::Type::t \
               << " get " << (node).type << ". ";                       \
            ANNOTATE_LOCATION(ss);                                      \
            throw std::runtime_error(ss.str());                         \
        }                                                               \
    } while (0)

#define ASSERT_TOKEN_TYPE(t, token)                                     \
    do {                                                                \
        if ((token).type != Token::Type::t) {                           \
            std::stringstream ss;                                       \
            ss << "Assert Token type failed! Expect " << Token::Type::t \
               << " get " << (token).type << ". ";                      \
            ANNOTATE_LOCATION(ss);                                      \
            throw std::runtime_error(ss.str());                         \
        }                                                               \
    } while (0)

Visitor::Visitor() {
    symbol_table.try_add_symbol("getint", SymbolAttr{}.set_function());
}

void Visitor::operator()(const ASTNode& node) { invoke_comp_unit(node); }

// 编译单元 CompUnit -> { ConstDecl | VarDecl | FuncDef | MainFuncDef }
void Visitor::invoke_comp_unit(const ASTNode& node) {
    ASSERT_AST_TYPE(COMP_UNIT, node);
    for (const auto& child : node.children) {
        const NodePtr& ptr = std::get<NodePtr>(child);
        switch (ptr->type) {
            case ASTNode::Type::FUNC_DEF:
            case ASTNode::Type::MAIN_FUNC_DEF:
                invoke_func_def(*ptr);
                break;
            case ASTNode::Type::CONST_DECL:
            case ASTNode::Type::VAR_DECL:
                invoke_var_decl(*ptr);
                break;
            default:
                UNEXPECTED_TYPE(ptr->type);
        }
    }
}

// 常量声明
// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
// 变量声明
// VarDecl -> [ 'static' ] BType VarDef { ',' VarDef } ';'
void Visitor::invoke_var_decl(const ASTNode& node) {
    if (node.type != ASTNode::Type::CONST_DECL &&
        node.type != ASTNode::Type::VAR_DECL) {
        UNEXPECTED_TYPE(node.type);
    }

    bool const_flag = (node.children.get(Token::Type::CONSTTK) != nullptr);
    bool static_flag = (node.children.get(Token::Type::STATICTK) != nullptr);

    for (const auto& def : node.children.equal_range(
             const_flag ? ASTNode::Type::CONST_DEF : ASTNode::Type::VAR_DEF)) {
        invoke_var_def(def, const_flag, static_flag);
    }
}

// 常量定义
// ConstDef -> Ident [ '[' ConstExp ']' ] '=' ConstInitVal
// 变量定义
// VarDef -> Ident [ '[' ConstExp ']' ] [ '=' InitVal ]
// Error type: b 名字重定义
//               函数名或者变量名在当前作用域下重复定义。
//               注意，变量一定是同一级作用域下才会判定出错，不同级作用域下，内层会覆盖外层定义。
//               报错行号 Ident 所在行数。
void Visitor::invoke_var_def(const ASTNode& node, bool const_flag,
                             bool static_flag) {
    if (node.type != ASTNode::Type::VAR_DEF &&
        node.type != ASTNode::Type::CONST_DEF) {
        UNEXPECTED_TYPE(node.type);
    }

    auto ident = node.children.get(Token::Type::IDENFR);
    ASSERT(ident);

    bool is_array = false;
    if (node.children.get(Token::Type::LBRACK)) {
        is_array = true;
        auto const_exp = node.children.get(ASTNode::Type::CONST_EXP);
        ASSERT(const_exp);
        invoke_exp(*const_exp, EvalOption{true});
    }

    add_symbol(*ident, SymbolAttr{}
                           .set_const(const_flag)
                           .set_static(static_flag)
                           .set_array(is_array));

    if (node.children.get(Token::Type::ASSIGN)) {
        auto init_val =
            node.children.get(const_flag ? ASTNode::Type::CONST_INIT_VAL
                                         : ASTNode::Type::INIT_VAL);
        ASSERT(init_val);
        invoke_var_init_val(*init_val, const_flag);
    }
}

// 常量初值
// ConstInitVal -> ConstExp | '{' [ ConstExp { ',' ConstExp } ] '}'
// 变量初值
// InitVal -> Exp | '{' [ Exp { ',' Exp } ] '}'
void Visitor::invoke_var_init_val(const ASTNode& node, bool const_flag) {
    if (node.type != ASTNode::Type::INIT_VAL &&
        node.type != ASTNode::Type::CONST_INIT_VAL) {
        UNEXPECTED_TYPE(node.type);
    }

    if (node.children.get(Token::Type::LBRACE)) {
        // 一维数组初值
        for (auto& exp : node.children.equal_range(
                 const_flag ? ASTNode::Type::CONST_EXP : ASTNode::Type::EXP)) {
            invoke_exp(exp, {const_flag});
        }
    } else {
        // 表达式初值
        auto exp = node.children.get(const_flag ? ASTNode::Type::CONST_EXP
                                                : ASTNode::Type::EXP);
        ASSERT(exp);
        invoke_exp(*exp, {const_flag});
    }
}

// 函数定义
// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
// 主函数定义
// MainFuncDef -> 'int' 'main' '(' ')' Block
// Error type: b 名字重定义
//               函数名或者变量名在当前作用域下重复定义。
//               注意，变量一定是同一级作用域下才会判定出错，不同级作用域下，内层会覆盖外层定义。
//               报错行号 Ident 所在行数。
//             g 有返回值的函数 缺少return语句
//               只需要考虑函数末尾是否存在return语句，无需考虑控制流。
//               报错行号为函数结尾的’}’所在行号。
void Visitor::invoke_func_def(const ASTNode& node) {
    bool main_func;
    if (node.type == ASTNode::Type::MAIN_FUNC_DEF) {
        main_func = true;
    } else if (node.type == ASTNode::Type::FUNC_DEF) {
        main_func = false;
    } else {
        UNEXPECTED_TYPE(node.type);
    }

    ScopeInfo scope_info = {};
    if (main_func) {
        scope_info.return_type = SymbolBaseType::INT;
    } else {
        auto func_type = node.children.get(ASTNode::Type::FUNC_TYPE);
        ASSERT(func_type);
        if (func_type->children.get(Token::Type::INTTK)) {
            scope_info.return_type = SymbolBaseType::INT;
        } else if (func_type->children.get(Token::Type::VOIDTK)) {
            scope_info.return_type = SymbolBaseType::VOID;
        } else {
            UNREACHABLE();
        }
    }

    if (!main_func) {
        auto ident = node.children.get(Token::Type::IDENFR);
        ASSERT(ident);
        auto fill_back_handler = add_symbol(
            *ident,
            SymbolAttr{}.set_function().set_base_type(scope_info.return_type));

        push_scope();
        if (auto func_params = node.children.get(ASTNode::Type::FUNC_PARAMS);
            func_params) {
            auto params_eval = invoke_func_params(*func_params);
            // 回填
            if (fill_back_handler) {
                fill_back_attr(fill_back_handler.value(),
                               SymbolAttr{}
                                   .set_function(std::move(params_eval))
                                   .set_base_type(scope_info.return_type));
            }
        }
    } else {
        push_scope();
    }

    auto block = node.children.get(ASTNode::Type::BLOCK);
    ASSERT(block);

    invoke_block(*block, scope_info);
    pop_scope();
}

// 函数形参表 FuncFParams -> FuncFParam { ',' FuncFParam }
std::vector<SymbolAttr> Visitor::invoke_func_params(const ASTNode& node) {
    ASSERT_AST_TYPE(FUNC_PARAMS, node);

    std::vector<SymbolAttr> function_params;
    for (const auto& func_param :
         node.children.equal_range(ASTNode::Type::FUNC_PARAM)) {
        auto attr = invoke_func_param(func_param);
        function_params.push_back(attr);
    }
    return function_params;
}

// 函数形参 FuncFParam -> BType Ident ['[' ']']
// Error type: b 名字重定义
//               函数名或者变量名在当前作用域下重复定义。
//               注意，变量一定是同一级作用域下才会判定出错，不同级作用域下，内层会覆盖外层定义。
//               报错行号 Ident 所在行数。
SymbolAttr Visitor::invoke_func_param(const ASTNode& node) {
    ASSERT_AST_TYPE(FUNC_PARAM, node);

    auto ident = node.children.get(Token::Type::IDENFR);
    ASSERT(ident);

    bool is_array = (node.children.get(Token::Type::LBRACK) != nullptr);
    auto attr = SymbolAttr{}.set_array(is_array);
    add_symbol(*ident, attr);
    return attr;
}

// 语句块 Block -> '{' { Decl | Stmt } '}'
void Visitor::invoke_block(const ASTNode& node, ScopeInfo scope_info) {
    ASSERT_AST_TYPE(BLOCK, node);

    const ASTNode* last_stmt = nullptr;
    for (auto& i : node.children) {
        if (auto block_item = std::get_if<NodePtr>(&i)) {
            if (block_item->get()->type == ASTNode::Type::CONST_DECL ||
                block_item->get()->type == ASTNode::Type::VAR_DECL) {
                invoke_var_decl(**block_item);
            } else if (block_item->get()->type == ASTNode::Type::STMT) {
                invoke_stmt(**block_item, scope_info);
                last_stmt = block_item->get();
            } else {
                UNEXPECTED_TYPE(block_item->get()->type);
            }
        }
    }

    if (scope_info.return_type == SymbolBaseType::INT &&
        symbol_table.current_scope_level() == 1) {
        // current_scope_level 限制只能为函数定义的block内
        bool has_last_return =
            last_stmt != nullptr &&
            last_stmt->children.get(Token::Type::RETURNTK) != nullptr;
        if (!has_last_return) {
            auto rbrace = node.children.get(Token::Type::RBRACE);
            ASSERT(rbrace);
            error_infos.emplace_back(ErrorInfo{'g', rbrace->line, rbrace->col});
        }
    }
}

static size_t count_specifiers(std::string_view strcon) {
    bool prev_is_percent = false;
    size_t result = 0;
    for (char c : strcon) {
        if (c == '%') {
            prev_is_percent = true;
            continue;
        }
        if (prev_is_percent && c == 'd') {
            result++;
        }
        prev_is_percent = false;
    }
    return result;
}

// 语句 Stmt -> LVal '=' Exp ';'
//             | [Exp] ';'
//             | Block
//             | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
//             | 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt
//             | 'break' ';'
//             | 'continue' ';'
//             | 'return' [Exp] ';'
//             | 'printf''('StringConst {','Exp}')'';'
// Error Type: f
//             l
//             m
//             h
void Visitor::invoke_stmt(const ASTNode& node, ScopeInfo scope_info) {
    ASSERT_AST_TYPE(STMT, node);

    // LVal '=' Exp ';'
    // Error type: h 不能改变常量的值
    //               LVal 为常量时，不能对其修改。报错行号为 LVal 所在行号。
    auto inner_invoke_assign_stmt = [&](const NodePtr& first_child_lval) {
        auto [lval_eval, lval_token] = invoke_lval(*first_child_lval);

        // LVal 为常量时，不能对其修改。报错行号为 LVal 所在行号。
        if (lval_eval.type.const_flag) {
            error_infos.emplace_back(
                ErrorInfo{'h', lval_token.line, lval_token.col});
        }

        auto exp = node.children.get(ASTNode::Type::EXP);
        ASSERT(exp);
        invoke_exp(*exp);
    };

    // 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
    auto inner_invoke_if_stmt = [&]() {
        auto cond = node.children.get(ASTNode::Type::COND);
        ASSERT(cond);
        invoke_cond(*cond);

        for (auto& stmt : node.children.equal_range(ASTNode::Type::STMT)) {
            invoke_stmt(stmt, scope_info);
        }
    };

    // 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt
    auto inner_invoke_for_stmt = [&]() {
        auto it = node.children.begin();
        auto for_tok = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(FORTK, for_tok);
        ++it;

        auto lparent_tok = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(LPARENT, lparent_tok);
        ++it;

        const ASTNode* cond = nullptr;
        const ASTNode* for_stmt1 = nullptr;
        const ASTNode* for_stmt2 = nullptr;

        if (auto _stmt = std::get_if<NodePtr>(&*it)) {
            ++it;
            for_stmt1 = _stmt->get();
        }

        auto semicn_tok1 = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(SEMICN, semicn_tok1);
        ++it;

        if (auto _cond = std::get_if<NodePtr>(&*it)) {
            ++it;
            cond = _cond->get();
        }

        auto semicn_tok2 = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(SEMICN, semicn_tok2);
        ++it;

        if (auto _stmt = std::get_if<NodePtr>(&*it)) {
            ++it;
            for_stmt2 = _stmt->get();
        }

        auto stmt = node.children.get(ASTNode::Type::STMT);
        ASSERT(stmt);

        if (for_stmt1) invoke_for_stmt(*for_stmt1);
        if (cond) invoke_cond(*cond);
        if (for_stmt2) invoke_for_stmt(*for_stmt2);

        scope_info.in_for_loop = true;
        invoke_stmt(*stmt, scope_info);
    };

    // Error type: m 在非循环块中使 用break和continue语句
    //               报错行号为 ‘break’ 与 ’continue’ 所在行号。
    auto inner_invoke_break_continue = [&](const Token& break_continue_tok) {
        if (!scope_info.in_for_loop) {
            error_infos.emplace_back(ErrorInfo{'m', break_continue_tok.line,
                                               break_continue_tok.col});
        }
    };

    // 'return' [Exp] ';'
    // Error type: f
    auto inner_invoke_return_stmt = [&](const Token& return_tok) {
        if (auto exp = node.children.get(ASTNode::Type::EXP)) {
            invoke_exp(*exp);
            if (scope_info.return_type == SymbolBaseType::VOID) {
                // 无返回值的函数 存在不匹配的 return语句
                // 报错行号为‘return’所在行号。
                error_infos.emplace_back(
                    ErrorInfo{'f', return_tok.line, return_tok.col});
            }
        }
    };

    // 'printf''('StringConst {','Exp}')'';'
    // Error type: l printf中格式字符与表达式个数不匹配
    //               报错行号为‘printf’所在行号。
    auto inner_invoke_printf_stmt = [&]() {
        auto printf_token = node.children.get(Token::Type::PRINTFTK);
        ASSERT(printf_token);
        auto string_const = node.children.get(Token::Type::STRCON);
        ASSERT(string_const);

        size_t exp_count = 0;
        for (auto& exp : node.children.equal_range(ASTNode::Type::EXP)) {
            invoke_exp(exp);
            exp_count++;
        }

        // Error 'l'
        size_t specifier_count = count_specifiers(string_const->content);
        if (specifier_count != exp_count) {
            error_infos.emplace_back(
                ErrorInfo{'l', printf_token->line, printf_token->col});
        }
    };

    auto invoke_node_child = [&](const NodePtr& first_child) {
        switch (first_child.get()->type) {
            case ASTNode::Type::L_VAL:
                inner_invoke_assign_stmt(first_child);
                break;
            case ASTNode::Type::EXP:
                invoke_exp(*first_child);
                break;
            case ASTNode::Type::BLOCK:
                push_scope();
                invoke_block(*first_child, scope_info);
                pop_scope();
                break;
            default:
                UNEXPECTED_TYPE(first_child.get()->type);
        }
    };

    auto invoke_token_child = [&](const Token& token) {
        switch (token.type) {
            case Token::Type::SEMICN:
                break;
            case Token::Type::IFTK:
                inner_invoke_if_stmt();
                break;
            case Token::Type::FORTK:
                inner_invoke_for_stmt();
                break;
            case Token::Type::BREAKTK:
            case Token::Type::CONTINUETK:
                inner_invoke_break_continue(token);
                break;
            case Token::Type::RETURNTK:
                inner_invoke_return_stmt(token);
                break;
            case Token::Type::PRINTFTK:
                inner_invoke_printf_stmt();
                break;
            default:
                UNEXPECTED_TYPE(token.type);
        }
    };

    std::visit(overloaded{invoke_node_child, invoke_token_child},
               *node.children.begin());
}

// ForStmt -> LVal '=' Exp { ',' LVal '=' Exp }
// Error type: h 不能改变常量的值
//               LVal 为常量时，不能对其修改。报错行号为 LVal 所在行号。
void Visitor::invoke_for_stmt(const ASTNode& node) {
    ASSERT_AST_TYPE(FOR_STMT, node);

    auto lval_rg = node.children.equal_range(ASTNode::Type::L_VAL);
    auto exp_rg = node.children.equal_range(ASTNode::Type::EXP);

    auto lval_it = lval_rg.begin();
    auto exp_it = exp_rg.begin();
    for (; lval_it != lval_rg.end(); ++lval_it, ++exp_it) {
        ASSERT(exp_it != exp_rg.end());

        auto& lval = *lval_it;
        auto& exp = *exp_it;

        auto [lval_eval, lval_token] = invoke_lval(lval);

        // LVal 为常量时，不能对其修改。报错行号为 LVal 所在行号。
        if (lval_eval.type.const_flag) {
            error_infos.emplace_back(
                ErrorInfo{'h', lval_token.line, lval_token.col});
        }

        invoke_exp(exp);
    }
}

// 表达式
// Exp -> AddExp
EvalResult Visitor::invoke_exp(const ASTNode& node, EvalOption option) {
    if (node.type != ASTNode::Type::EXP &&
        node.type != ASTNode::Type::CONST_EXP) {
        UNEXPECTED_TYPE(node.type);
    }

    auto add_exp = node.children.get(ASTNode::Type::ADD_EXP);
    ASSERT(add_exp);
    return invoke_add_exp(*add_exp, option);
}

// 条件表达式
// Cond -> LOrExp
EvalResult Visitor::invoke_cond(const ASTNode& node, EvalOption option) {
    ASSERT_AST_TYPE(COND, node);

    auto lor_exp = node.children.get(ASTNode::Type::LOR_EXP);
    ASSERT(lor_exp);
    return invoke_lor_exp(*lor_exp, option);
}

// 基本表达式
// PrimaryExp -> '(' Exp ')' | LVal | Number
EvalResult Visitor::invoke_primary_exp(const ASTNode& node, EvalOption option) {
    if (node.children.get(Token::Type::LPARENT)) {
        auto exp = node.children.get(ASTNode::Type::EXP);
        ASSERT(exp);
        return invoke_exp(*exp, option);
    } else if (auto lval = node.children.get(ASTNode::Type::L_VAL)) {
        auto [lval_eval, lval_token] = invoke_lval(*lval);
        return lval_eval;
    } else if (auto number = node.children.get(ASTNode::Type::NUMBER)) {
        auto [number_eval, intcon_token] = invoke_number(*number);
        return number_eval;
    } else {
        UNREACHABLE();
    }
}

// 左值表达式
// LVal -> Ident ['[' Exp ']']
// Error type: c 未定义的名字
//               使用了未定义的标识符
//               报错行号为 Ident 所在 行数。
std::tuple<EvalResult, Token> Visitor::invoke_lval(const ASTNode& node) {
    ASSERT_AST_TYPE(L_VAL, node);

    auto ident = node.children.get(Token::Type::IDENFR);
    ASSERT(ident);

    auto record = symbol_table.find(ident->content);
    if (record == nullptr) {
        error_infos.emplace_back(ErrorInfo{'c', ident->line, ident->col});
    }

    bool has_index = false;
    if (node.children.get(Token::Type::LBRACK)) {
        auto exp = node.children.get(ASTNode::Type::EXP);
        ASSERT(exp);
        invoke_exp(*exp);
        has_index = true;
    }

    if (record) {
        SymbolAttr attr = record->attr;
        if (attr.is_array && has_index) {
            attr.is_array = false;
        }
        return std::make_tuple(EvalResult{attr}, *ident);
    } else {
        return std::make_tuple(EvalResult{}, *ident);  // ?
    }
}

static int evaluate_number(std::string_view intcon) {
    int result = 0;
    for (char c : intcon) {
        result = result * 10 + (c - '0');
    }
    return result;
}

// 数值 Number -> IntConst
std::tuple<EvalResult, Token> Visitor::invoke_number(const ASTNode& node) {
    ASSERT_AST_TYPE(NUMBER, node);

    auto intcon = node.children.get(Token::Type::INTCON);
    ASSERT(intcon);
    int value = evaluate_number(intcon->content);
    return std::make_tuple(EvalResult{SymbolAttr{}.set_const(), value},
                           *intcon);
}

// 一元表达式
// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
// Error type: c 未定义的名字
//               使用了未定义的标识符
//               报错行号为 Ident 所在 行数。
//             d 函数参数个数不匹配
//               函数调用语句中，参数个数与函数定义中的参数个数不匹配。
//               报错行号为函数调用语句的函数名所在行数
//             e 函数参数类型不匹配
//               函数调用语句中，参数类型与函数定义中对应位置的参数类型不匹配。
//               报错行号为函数调用语句的函数名所在行数。
EvalResult Visitor::invoke_unary_exp(const ASTNode& node, EvalOption option) {
    if (auto primary_exp = node.children.get(ASTNode::Type::PRIMARY_EXP)) {
        return invoke_primary_exp(*primary_exp, option);
    } else if (auto ident = node.children.get(Token::Type::IDENFR)) {
        if (auto record = symbol_table.find(ident->content)) {
            ASSERT(record->attr.is_function);
            const std::vector<SymbolAttr>& fparams =
                record->attr.function_params;

            // 处理函数参数
            if (auto func_rparams =
                    node.children.get(ASTNode::Type::FUNC_RPARAMS)) {
                std::vector<EvalResult> rparams =
                    invoke_func_rparams(*func_rparams, option);

                if (fparams.size() != rparams.size()) {  // 函数参数个数不匹配
                    error_infos.emplace_back(
                        ErrorInfo{'d', ident->line, ident->col});
                } else {  // 函数参数类型不匹配
                    auto fparam_it = fparams.begin();
                    auto rparam_it = rparams.begin();
                    for (; fparam_it != fparams.end();
                         ++fparam_it, ++rparam_it) {
                        ASSERT(rparam_it != rparams.end());

                        // 对于调用函数时，参数类型不匹配一共有以下几种情况的不匹配：
                        // - 传递数组给变量。
                        // - 传递变量给数组。
                        if (fparam_it->is_array != rparam_it->type.is_array) {
                            error_infos.emplace_back(
                                ErrorInfo{'e', ident->line, ident->col});
                        }
                    }
                }
            }

            // 处理返回值
            // ASSERT(record->attr.base_type != SymbolBaseType::VOID);
            return {SymbolAttr{}
                        .set_base_type(record->attr.base_type)
                        .set_array(record->attr.is_array)};
        } else {
            error_infos.emplace_back(ErrorInfo{'c', ident->line, ident->col});
            return {};
        }
    } else if (auto unary_op = node.children.get(ASTNode::Type::UNARY_OP)) {
        invoke_unary_op(*unary_op);
        auto unary_exp = node.children.get(ASTNode::Type::UNARY_EXP);
        ASSERT(unary_exp);
        auto unary_exp_eval = invoke_unary_exp(*unary_exp);
        return unary_exp_eval;
    } else {
        UNREACHABLE();
    }
}

// 单目运算符
// UnaryOp -> '+' | '−' | '!'
const Token& Visitor::invoke_unary_op(const ASTNode& node) {
    ASSERT_AST_TYPE(UNARY_OP, node);

    const Token& token = std::get<Token>(*node.children.begin());
    ASSERT(token.type == Token::Type::PLUS || token.type == Token::Type::MINU ||
           token.type == Token::Type::NOT);
    return token;
}

// 函数实参表
// FuncRParams -> Exp { ',' Exp }
std::vector<EvalResult> Visitor::invoke_func_rparams(const ASTNode& node,
                                                     EvalOption option) {
    std::vector<EvalResult> evals;
    for (auto& exp : node.children.equal_range(ASTNode::Type::EXP)) {
        EvalResult eval = invoke_exp(exp, option);
        evals.push_back(eval);
    }
    return evals;
}

#define ASSERT_CALCABLE(eval_result)                             \
    do {                                                         \
        ASSERT((eval_result).is_array == false);                 \
        ASSERT((eval_result).is_function == false);              \
        ASSERT((eval_result).base_type != SymbolBaseType::VOID); \
    } while (0);

// 乘除模表达式
// MulExp -> UnaryExp | MulExp ('*' | '/' | '%') UnaryExp
// Modified:
//    MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
EvalResult Visitor::invoke_mul_exp(const ASTNode& node, EvalOption option) {
    auto it = node.children.begin();
    auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_unary_exp(*child, option);
    while (it != node.children.end()) {
        [[maybe_unused]] auto& op = std::get<Token>(*it);
        ++it;

        auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_unary_exp(*child, option);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);
        result1.type.const_flag =
            result1.type.const_flag && result2.type.const_flag;
    }

    return result1;
}

// 加减表达式
// AddExp -> MulExp | AddExp ('+' | '−') MulExp
// Modified:
//     AddExp -> MulExp { ('+' | '-') MulExp }
EvalResult Visitor::invoke_add_exp(const ASTNode& node, EvalOption option) {
    auto it = node.children.begin();
    auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_mul_exp(*child, option);
    while (it != node.children.end()) {
        [[maybe_unused]] auto& op = std::get<Token>(*it);
        ++it;

        auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_mul_exp(*child, option);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);
        result1.type.const_flag =
            result1.type.const_flag && result2.type.const_flag;
    }

    return result1;
}

// 关系表达式
// RelExp -> AddExp | RelExp ('<' | '>' | '<=' | '>=') AddExp
// Modified:
//     RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
EvalResult Visitor::invoke_rel_exp(const ASTNode& node, EvalOption option) {
    auto it = node.children.begin();
    auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_add_exp(*child, option);
    while (it != node.children.end()) {
        [[maybe_unused]] auto& op = std::get<Token>(*it);
        ++it;

        auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_add_exp(*child, option);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);
        result1.type.const_flag =
            result1.type.const_flag && result2.type.const_flag;
    }

    return result1;
}

// 相等性表达式
// EqExp -> RelExp | EqExp ('==' | '!=') RelExp
// Modified:
//     EqExp -> RelExp { ('==' | '!=') RelExp }
EvalResult Visitor::invoke_eq_exp(const ASTNode& node, EvalOption option) {
    auto it = node.children.begin();
    auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_rel_exp(*child, option);
    while (it != node.children.end()) {
        [[maybe_unused]] auto& op = std::get<Token>(*it);
        ++it;

        auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_rel_exp(*child, option);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);
        result1.type.const_flag =
            result1.type.const_flag && result2.type.const_flag;
    }

    return result1;
}

// 逻辑与表达式
// LAndExp -> EqExp | LAndExp '&&' EqExp
// Modified:
//     LAndExp -> EqExp { '&&' EqExp }
EvalResult Visitor::invoke_land_exp(const ASTNode& node, EvalOption option) {
    auto it = node.children.begin();
    auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_eq_exp(*child, option);
    while (it != node.children.end()) {
        [[maybe_unused]] auto& op = std::get<Token>(*it);
        ++it;

        auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_eq_exp(*child, option);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);
        result1.type.const_flag =
            result1.type.const_flag && result2.type.const_flag;
    }

    return result1;
}

// 辑或表达式
// LOrExp -> LAndExp | LOrExp '||' LAndExp
// Modified:
//     LOrExp -> LAndExp { '||' LAndExp }
EvalResult Visitor::invoke_lor_exp(const ASTNode& node, EvalOption option) {
    auto it = node.children.begin();
    auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_land_exp(*child, option);
    while (it != node.children.end()) {
        [[maybe_unused]] auto& op = std::get<Token>(*it);
        ++it;

        auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_land_exp(*child, option);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);
        result1.type.const_flag =
            result1.type.const_flag && result2.type.const_flag;
    }

    return result1;
}

void Visitor::push_scope() {
    scope_stack.push(current_scope);
    current_scope = new_define_scope = new_define_scope + 1;
    symbol_table.push_scope();
}

void Visitor::pop_scope() {
    current_scope = scope_stack.top();
    scope_stack.pop();
    symbol_table.pop_scope();
}

// 如果当前作用域没有重名符号定义，则将符号加入符号表
// 否则不执行操作，记录错误
std::optional<Visitor::SymbolAttrFillBackHandler> Visitor::add_symbol(
    Token idenfr_token, SymbolAttr attr, char error_type) {
    ASSERT_TOKEN_TYPE(IDENFR, idenfr_token);

    if (symbol_table.exist_in_scope(idenfr_token.content)) {
        error_infos.emplace_back(
            ErrorInfo{error_type, idenfr_token.line, idenfr_token.col});
        return std::nullopt;
    } else {
        auto& attr_ref =
            symbol_table.try_add_symbol(idenfr_token.content, attr);

        size_t index = records.size();
        records.emplace_back(
            SymbolRecord{current_scope, idenfr_token.content, attr});
        return SymbolAttrFillBackHandler{attr_ref, index};
    }
}

void Visitor::fill_back_attr(const SymbolAttrFillBackHandler& handler,
                             const SymbolAttr& attr) {
    handler.attr_in_symbol_table.get() = attr;
    records.at(handler.attr_record_index).attr = attr;
}

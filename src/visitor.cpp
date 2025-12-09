#include "visitor.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include "error.hpp"
#include "grammer.hpp"
#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Instructions.hpp"
#include "ir/Type.hpp"
#include "ir/Value.hpp"
#include "symbol_table.hpp"
#include "token.hpp"
#include "util/assert.hpp"
#include "util/lambda_overload.hpp"

using NodePtr = std::unique_ptr<ASTNode>;
using namespace std::string_literals;

#define UNEXPECTED_TYPE(type)                      \
    do {                                           \
        std::stringstream ss;                      \
        ss << "Unexpected type: " << (type);       \
        assert_throw(ss.str(), ANNOTATE_LOCATION); \
    } while (0)

#define ASSERT_AST_TYPE(t, node)                                        \
    do {                                                                \
        if ((node).type != ASTNode::Type::t) {                          \
            std::stringstream ss;                                       \
            ss << "Assert AST type failed! Expect " << ASTNode::Type::t \
               << " get " << (node).type << ". ";                       \
            assert_throw(ss.str(), ANNOTATE_LOCATION);                  \
        }                                                               \
    } while (0)

#define ASSERT_TOKEN_TYPE(t, token)                                     \
    do {                                                                \
        if ((token).type != Token::Type::t) {                           \
            std::stringstream ss;                                       \
            ss << "Assert Token type failed! Expect " << Token::Type::t \
               << " get " << (token).type << ". ";                      \
            assert_throw(ss.str(), ANNOTATE_LOCATION);                  \
        }                                                               \
    } while (0)

Visitor::Visitor(const ASTNode& node) {
    // declare i32 @getint()          ; 读取一个整数
    auto* getint_type = ir::FunctionType::get(builder->getInt32Ty(), {}, false);
    auto* getint_value = ir::Function::Create(
        getint_type, ir::Function::ExternalLinkage, "getint", *module);
    SymbolAttr attr;
    attr.type.is_function = true;
    attr.addr_value       = getint_value;
    symbol_table.try_add_symbol("getint", attr);

    // declare void @putint(i32)      ; 输出一个整数
    auto* putint_type = ir::FunctionType::get(builder->getVoidTy(),
                                              {builder->getInt32Ty()}, false);
    putint_func       = ir::Function::Create(
        putint_type, ir::Function::ExternalLinkage, "putint", *module);

    // declare void @putch(i32)       ; 输出一个字符
    auto* putch_type = ir::FunctionType::get(builder->getVoidTy(),
                                             {builder->getInt32Ty()}, false);
    putch_func = ir::Function::Create(putch_type, ir::Function::ExternalLinkage,
                                      "putch", *module);

    // declare void @putstr(i8*)       ; 输出字符串
    auto* putstr_type = ir::FunctionType::get(
        builder->getVoidTy(), {builder->getPtrTy(builder->getInt8Ty())}, false);
    putstr_func = ir::Function::Create(
        putstr_type, ir::Function::ExternalLinkage, "putstr", *module);

    // declare init global function
    auto* init_global_func_ty =
        ir::FunctionType::get(builder->getVoidTy(), {}, false);
    init_global_func = ir::Function::Create(init_global_func_ty,
                                            ir::GlobalValue::InternalLinkage,
                                            ".init", *module);
    init_global_bb = ir::BasicBlock::Create(*context, "init", init_global_func);

    invoke_comp_unit(node);

    builder->SetInsertPoint(init_global_bb);
    builder->CreateRetVoid();
}

// void Visitor::operator()(const ASTNode& node) { invoke_comp_unit(node); }

// 编译单元 CompUnit -> { ConstDecl | VarDecl | FuncDef | MainFuncDef }
void Visitor::invoke_comp_unit(const ASTNode& node) {
    ASSERT_AST_TYPE(COMP_UNIT, node);
    for (const auto& child : node.children) {
        const auto& ptr = std::get<NodePtr>(child);
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

    bool const_flag  = (node.children.get(Token::Type::CONSTTK) != nullptr);
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
    bool is_global_scope = builder->GetInsertBlock() == nullptr || static_flag;

    const Token* ident = node.children.get(Token::Type::IDENFR);
    ASSERT(ident);
    bool               is_array = false;
    std::optional<int> array_count;
    if (node.children.get(Token::Type::LBRACK)) {  // Ident [ ConstExp ]
        is_array              = true;
        const auto* const_exp = node.children.get(ASTNode::Type::CONST_EXP);
        ASSERT(const_exp);
        auto exp_eval = invoke_exp(*const_exp);
        auto value    = exp_eval.exp->test_constexpr();
        ASSERT(value.has_value());
        array_count = *value;
    }

    // alloc address
    ir::Type* alloc_type;
    if (is_array) {
        alloc_type = ir::ArrayType::get(builder->getInt32Ty(), *array_count);
    } else {
        alloc_type = builder->getInt32Ty();
    }

    ir::Value* alloc_ptr;
    if (is_global_scope) {
        alloc_ptr = ir::GlobalVariable::Create(
            *module, alloc_type, const_flag, ir::GlobalValue::ExternalLinkage,
            nullptr, std::string(ident->content));
    } else {
        alloc_ptr = builder->CreateAlloca(alloc_type, nullptr,
                                          "alloc."s.append(ident->content));
    }

    // initializer
    std::vector<EvalResult> init_evals;
    if (node.children.get(Token::Type::ASSIGN)) {
        const ASTNode* init_val =
            node.children.get(const_flag ? ASTNode::Type::CONST_INIT_VAL
                                         : ASTNode::Type::INIT_VAL);
        ASSERT(init_val);
        init_evals = invoke_var_init_val(*init_val, const_flag);
    }

    std::vector<int32_t> const_values;
    if (is_global_scope) {
        auto* global_ptr = dynamic_cast<ir::GlobalVariable*>(alloc_ptr);
        auto* const_zero = ir::ConstantInt::get(builder->getInt32Ty(), 0);

        ir::BasicBlock* current_bb = builder->GetInsertBlock();
        builder->SetInsertPoint(init_global_bb);

        if (is_array) {
            std::vector<ir::Constant*> array_const_elem(*array_count,
                                                        const_zero);

            for (size_t i = 0; i < std::min(init_evals.size(),
                                            static_cast<size_t>(*array_count));
                 i++) {
                if (auto const_int = init_evals[i].exp->test_constexpr()) {
                    array_const_elem[i] =
                        ir::ConstantInt::get(builder->getInt32Ty(), *const_int);
                    if (const_flag) const_values.push_back(*const_int);
                } else {
                    ASSERT(!const_flag);
                    ir::Value* ptr_to_elem = builder->CreateGEP(
                        alloc_ptr, {builder->getInt32(0), builder->getInt32(i)},
                        "ptr.arr." + std::to_string(i));
                    builder->CreateStore(init_evals[i].exp->rvalue(*builder),
                                         ptr_to_elem);
                }
            }
            global_ptr->setInitializer(ir::ConstantArray::get(
                static_cast<ir::ArrayType*>(alloc_type), array_const_elem));
        } else {
            if (!init_evals.empty()) {
                ASSERT(init_evals.size() == 1);
                if (auto const_int = init_evals[0].exp->test_constexpr()) {
                    global_ptr->setInitializer(ir::ConstantInt::get(
                        builder->getInt32Ty(), *const_int));
                    if (const_flag) const_values.push_back(*const_int);
                } else {
                    ASSERT(!const_flag);
                    global_ptr->setInitializer(const_zero);
                    builder->CreateStore(init_evals[0].exp->rvalue(*builder),
                                         alloc_ptr);
                }
            } else {
                global_ptr->setInitializer(const_zero);
            }
        }

        builder->SetInsertPoint(current_bb);
    } else {
        if (is_array) {
            for (size_t i = 0; i < static_cast<size_t>(*array_count); i++) {
                std::vector<ir::Value*> index = {builder->getInt32(0),
                                                 builder->getInt32(i)};

                ir::Value* ptr_to_elem = builder->CreateGEP(
                    alloc_ptr, index, "ptr.arr." + std::to_string(i));
                if (i < init_evals.size()) {
                    if (auto const_int = init_evals[i].exp->test_constexpr()) {
                        builder->CreateStore(builder->getInt32(*const_int),
                                             ptr_to_elem);
                        if (const_flag) const_values.push_back(*const_int);
                    } else {
                        ASSERT(!const_flag);
                        builder->CreateStore(
                            init_evals[i].exp->rvalue(*builder), ptr_to_elem);
                    }
                } else {
                    builder->CreateStore(builder->getInt32(0), ptr_to_elem);
                }
            }
        } else {
            if (!init_evals.empty()) {
                ASSERT(init_evals.size() == 1);
                if (auto const_int = init_evals[0].exp->test_constexpr()) {
                    builder->CreateStore(builder->getInt32(*const_int),
                                         alloc_ptr);
                    if (const_flag) const_values.push_back(*const_int);
                } else {
                    ASSERT(!const_flag);
                    builder->CreateStore(init_evals[0].exp->rvalue(*builder),
                                         alloc_ptr);
                }
            }
        }
    }

    // Add to symbol table
    SymbolType type;
    type.base_type   = SymbolBaseType::INT;
    type.const_flag  = const_flag;
    type.static_flag = static_flag;
    type.is_array    = is_array;
    type.array_count = array_count;
    SymbolAttr attr;
    attr.type         = type;
    attr.addr_value   = alloc_ptr;
    attr.const_values = std::move(const_values);
    add_symbol(*ident, attr);
}

// 常量初值
// ConstInitVal -> ConstExp | '{' [ ConstExp { ',' ConstExp } ] '}'
// 变量初值
// InitVal -> Exp | '{' [ Exp { ',' Exp } ] '}'
std::vector<EvalResult> Visitor::invoke_var_init_val(const ASTNode& node,
                                                     bool const_flag) {
    if (node.type != ASTNode::Type::INIT_VAL &&
        node.type != ASTNode::Type::CONST_INIT_VAL) {
        UNEXPECTED_TYPE(node.type);
    }

    std::vector<EvalResult> results;
    for (const ASTNode& exp : node.children.equal_range(
             const_flag ? ASTNode::Type::CONST_EXP : ASTNode::Type::EXP)) {
        results.push_back(invoke_exp(exp));
    }
    return results;
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
    bool is_main_func;
    if (node.type == ASTNode::Type::MAIN_FUNC_DEF) {
        is_main_func = true;
    } else if (node.type == ASTNode::Type::FUNC_DEF) {
        is_main_func = false;
    } else {
        UNEXPECTED_TYPE(node.type);
    }

    // Return type used in
    // 1. ScopeInfo
    // 2. llvm function def
    ir::Type* llvm_return_type;
    ScopeInfo scope_info;
    if (is_main_func) {
        scope_info.return_type = SymbolBaseType::INT;
        llvm_return_type       = builder->getInt32Ty();
    } else {
        const ASTNode* func_type = node.children.get(ASTNode::Type::FUNC_TYPE);
        ASSERT(func_type);
        if (func_type->children.get(Token::Type::INTTK)) {
            scope_info.return_type = SymbolBaseType::INT;
            llvm_return_type       = builder->getInt32Ty();
        } else if (func_type->children.get(Token::Type::VOIDTK)) {
            scope_info.return_type = SymbolBaseType::VOID;
            llvm_return_type       = builder->getVoidTy();
        } else {
            UNREACHABLE();
        }
    }

    // 1. function name & function_ident
    // 2. function params
    std::vector<std::tuple<SymbolType, Token>> params;
    std::string_view                           function_name;
    const Token*                               function_ident;
    if (is_main_func) {
        function_name = "main";
    } else {
        function_ident = node.children.get(Token::Type::IDENFR);
        ASSERT(function_ident);

        function_name = function_ident->content;
        if (const ASTNode* func_params =
                node.children.get(ASTNode::Type::FUNC_PARAMS);
            func_params) {
            params = invoke_func_params(*func_params);
        }
    }

    std::vector<SymbolType> params_types;
    std::transform(params.begin(), params.end(),
                   std::back_inserter(params_types),
                   [](auto tuple) { return std::get<0>(tuple); });

    std::vector<ir::Type*> llvm_params_types;
    std::transform(params_types.begin(), params_types.end(),
                   std::back_inserter(llvm_params_types),
                   [this](SymbolType symbol_type) -> ir::Type* {
                       if (symbol_type.is_array)
                           return builder->getPtrTy(builder->getInt32Ty());
                       else
                           return builder->getInt32Ty();
                   });

    auto* llvm_function_type =
        ir::FunctionType::get(llvm_return_type, llvm_params_types, false);
    auto* llvm_function_value =
        ir::Function::Create(llvm_function_type, ir::Function::ExternalLinkage,
                             std::string(function_name), *module);

    // add function to symbol table
    if (!is_main_func) {
        SymbolAttr attr;
        attr.type.base_type       = scope_info.return_type;
        attr.type.function_params = std::move(params_types);
        attr.type.is_function     = true;
        attr.addr_value           = llvm_function_value;
        add_symbol(*function_ident, attr);
    }

    push_scope();
    auto* entry_bb =
        ir::BasicBlock::Create(*context, "entry", llvm_function_value);
    builder->SetInsertPoint(entry_bb);

    // add params to symbol table
    size_t i = 0;
    for (auto& arg : llvm_function_value->args()) {
        const auto& [type, ident] = params[i];
        arg.setName(std::string(ident.content));
        ir::Value* arg_ptr;
        if (!arg.getType()->isPointerTy()) {
            arg_ptr = builder->CreateAlloca(arg.getType(), nullptr,
                                            "alloc."s.append(ident.content));
            builder->CreateStore(&arg, arg_ptr);
        } else {
            arg_ptr = &arg;
        }

        SymbolAttr attr;
        attr.type       = type;
        attr.addr_value = arg_ptr;
        add_symbol(ident, attr);
        i++;
    }

    if (is_main_func) {
        builder->CreateCall(init_global_func->getFunctionType(),
                            init_global_func, {}, "");
    }

    const ASTNode* block = node.children.get(ASTNode::Type::BLOCK);
    ASSERT(block);
    invoke_block(*block, scope_info);
    pop_scope();
    builder->SetInsertPoint(nullptr);
}

// 函数形参表 FuncFParams -> FuncFParam { ',' FuncFParam }
std::vector<std::tuple<SymbolType, Token>> Visitor::invoke_func_params(
    const ASTNode& node) {
    ASSERT_AST_TYPE(FUNC_PARAMS, node);

    std::vector<std::tuple<SymbolType, Token>> results;
    for (const auto& func_param :
         node.children.equal_range(ASTNode::Type::FUNC_PARAM)) {
        auto attr = invoke_func_param(func_param);
        results.push_back(attr);
    }
    return results;
}

// 函数形参 FuncFParam -> BType Ident ['[' ']']
// Error type: b 名字重定义
//               函数名或者变量名在当前作用域下重复定义。
//               注意，变量一定是同一级作用域下才会判定出错，不同级作用域下，内层会覆盖外层定义。
//               报错行号 Ident 所在行数。
// Note: 收集所有信息到 invoke_func_def 再处理报错 (invoke_func_def 调用
//       add_symbol 即可)
std::tuple<SymbolType, Token> Visitor::invoke_func_param(const ASTNode& node) {
    ASSERT_AST_TYPE(FUNC_PARAM, node);

    const Token* ident = node.children.get(Token::Type::IDENFR);
    ASSERT(ident);

    bool       is_array = (node.children.get(Token::Type::LBRACK) != nullptr);
    SymbolType type;
    type.is_array = is_array;
    return std::make_tuple(type, *ident);
}

// 语句块 Block -> '{' { Decl | Stmt } '}'
void Visitor::invoke_block(const ASTNode& node, ScopeInfo scope_info) {
    ASSERT_AST_TYPE(BLOCK, node);

    const ASTNode* last_stmt = nullptr;
    for (const auto& i : node.children) {
        if (const auto* block_item = std::get_if<NodePtr>(&i)) {
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

    // Error type: b
    // LLVM 要求必须有 ret
    if (symbol_table.current_scope_level() == 1) {
        // current_scope_level 限制只能为函数定义的block内
        bool has_last_return =
            last_stmt != nullptr &&
            last_stmt->children.get(Token::Type::RETURNTK) != nullptr;
        if (!has_last_return) {
            if (scope_info.return_type == SymbolBaseType::INT) {
                const Token* rbrace = node.children.get(Token::Type::RBRACE);
                ASSERT(rbrace);
                reportError(ErrorInfo::Type::MISSING_RETURN, *rbrace,
                            "Missing return in int function");
                builder->CreateRet(builder->getInt32(0));
            } else {
                builder->CreateRetVoid();
            }
        }
    }
}

template <typename Func_deli, typename Func_substr>
static void split_string_skip_empty(std::string_view str,
                                    std::string_view delimiter,
                                    Func_deli        delimiter_handler,
                                    Func_substr      substr_handler) {
    size_t start = 0;
    for (size_t end = str.find(delimiter); end != std::string_view::npos;
         end        = str.find(delimiter, start)) {
        if (end > start) {  // 只有当子串非空时才添加
            substr_handler(str.substr(start, end - start));
        }
        delimiter_handler();
        start = end + delimiter.length();
    }
    if (start < str.length()) substr_handler(str.substr(start));
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
            reportError(ErrorInfo::Type::CONST_ASSIGNMENT, lval_token,
                        "Can not modify constant");
        }

        const ASTNode* exp = node.children.get(ASTNode::Type::EXP);
        ASSERT(exp);
        EvalResult exp_eval = invoke_exp(*exp);
        builder->CreateStore(exp_eval.exp->rvalue(*builder),
                             lval_eval.exp->lvalue(*builder));
    };

    // 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
    auto inner_invoke_if_stmt = [&]() {
        const ASTNode* cond_node = node.children.get(ASTNode::Type::COND);
        ASSERT(cond_node);
        auto cond     = invoke_cond(*cond_node);
        bool has_else = node.children.get(Token::Type::ELSETK) != nullptr;

        ir::Function* this_function = builder->GetInsertBlock()->getParent();
        ASSERT(this_function);
        ir::BasicBlock* then_bb =
            ir::BasicBlock::Create(*context, "if.then", this_function);
        ir::BasicBlock* else_bb = nullptr;
        ir::BasicBlock* merge_bb =
            ir::BasicBlock::Create(*context, "if.merge", this_function);
        if (has_else) {
            else_bb =
                ir::BasicBlock::Create(*context, "if.else", this_function);
            cond->gen_code(then_bb, else_bb, *builder);
        } else {
            cond->gen_code(then_bb, merge_bb, *builder);
        }

        auto stmt_rg = node.children.equal_range(ASTNode::Type::STMT);
        auto stmt_it = stmt_rg.begin();
        ASSERT(stmt_it != stmt_rg.end());  // At least 1 element
        builder->SetInsertPoint(then_bb);
        invoke_stmt(*stmt_it, scope_info);
        builder->CreateBr(merge_bb);

        if (has_else) {
            ++stmt_it;
            ASSERT(stmt_it != stmt_rg.end());  // At least 2 element
            builder->SetInsertPoint(else_bb);
            invoke_stmt(*stmt_it, scope_info);
            builder->CreateBr(merge_bb);
        }

        builder->SetInsertPoint(merge_bb);
    };

    // 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt
    auto inner_invoke_for_stmt = [&]() {
        auto it      = node.children.begin();
        auto for_tok = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(FORTK, for_tok);
        ++it;

        auto lparent_tok = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(LPARENT, lparent_tok);
        ++it;

        const ASTNode* cond_node = nullptr;
        const ASTNode* for_stmt1 = nullptr;
        const ASTNode* for_stmt2 = nullptr;

        if (const auto* stmt = std::get_if<NodePtr>(&*it)) {
            ++it;
            for_stmt1 = stmt->get();
        }

        auto semicn_tok1 = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(SEMICN, semicn_tok1);
        ++it;

        if (const auto* cond = std::get_if<NodePtr>(&*it)) {
            ++it;
            cond_node = cond->get();
        }

        auto semicn_tok2 = std::get<Token>(*it);
        ASSERT_TOKEN_TYPE(SEMICN, semicn_tok2);
        ++it;

        if (const auto* stmt = std::get_if<NodePtr>(&*it)) {
            for_stmt2 = stmt->get();
        }

        const ASTNode* stmt = node.children.get(ASTNode::Type::STMT);
        ASSERT(stmt);

        ir::Function* this_function = builder->GetInsertBlock()->getParent();
        ASSERT(this_function);
        ir::BasicBlock* loop_bb =
            ir::BasicBlock::Create(*context, "loop", this_function);
        ir::BasicBlock* cond_bb =
            ir::BasicBlock::Create(*context, "loop.cond", this_function);
        ir::BasicBlock* update_bb =
            ir::BasicBlock::Create(*context, "loop.update", this_function);
        ir::BasicBlock* after_bb =
            ir::BasicBlock::Create(*context, "loop.after", this_function);

        if (for_stmt1) invoke_for_stmt(*for_stmt1);
        builder->CreateBr(cond_bb);

        builder->SetInsertPoint(cond_bb);
        if (cond_node) {
            auto cond = invoke_cond(*cond_node);
            cond->gen_code(loop_bb, after_bb, *builder);
        } else {
            builder->CreateBr(loop_bb);
        }

        builder->SetInsertPoint(update_bb);
        if (for_stmt2) invoke_for_stmt(*for_stmt2);
        builder->CreateBr(cond_bb);

        builder->SetInsertPoint(loop_bb);
        scope_info.for_info = ScopeInfo::ForInfo{update_bb, after_bb};
        invoke_stmt(*stmt, scope_info);
        builder->CreateBr(update_bb);

        builder->SetInsertPoint(after_bb);
    };

    // Error type: m 在非循环块中使 用break和continue语句
    //               报错行号为 ‘break’ 与 ’continue’ 所在行号。
    auto inner_invoke_break = [&](const Token& break_tok) {
        if (!scope_info.in_for()) {
            reportError(ErrorInfo::Type::BREAK_CONTINUE_OUT_OF_LOOP, break_tok,
                        "Unexpected break out of loop");
        } else {
            builder->CreateBr(scope_info.for_info->after_bb);
        }
    };

    // Error type: m 在非循环块中使 用break和continue语句
    //               报错行号为 ‘break’ 与 ’continue’ 所在行号。
    auto inner_invoke_continue = [&](const Token& continue_tok) {
        if (!scope_info.for_info) {
            reportError(ErrorInfo::Type::BREAK_CONTINUE_OUT_OF_LOOP,
                        continue_tok, "Unexpected continue out of loop");
        } else {
            builder->CreateBr(scope_info.for_info->update_bb);
        }
    };

    // 'return' [Exp] ';'
    // Error type: f
    auto inner_invoke_return_stmt = [&](const Token& return_tok) {
        if (const ASTNode* exp = node.children.get(ASTNode::Type::EXP)) {
            EvalResult eval = invoke_exp(*exp);
            if (scope_info.return_type == SymbolBaseType::VOID) {
                // 无返回值的函数 存在不匹配的 return语句
                // 报错行号为‘return’所在行号。
                reportError(ErrorInfo::Type::RETURN_MISMATCH, return_tok,
                            "Unexpected return in void function");
            }
            builder->CreateRet(eval.exp->rvalue(*builder));
        } else {
            builder->CreateRetVoid();
        }
    };

    // 'printf''('StringConst {','Exp}')'';'
    // Error type: l printf中格式字符与表达式个数不匹配
    //               报错行号为‘printf’所在行号。
    auto inner_invoke_printf_stmt = [&]() {
        const Token* printf_token = node.children.get(Token::Type::PRINTFTK);
        ASSERT(printf_token);
        const Token* string_const = node.children.get(Token::Type::STRCON);
        ASSERT(string_const);

        std::vector<EvalResult> args;
        for (const ASTNode& exp :
             node.children.equal_range(ASTNode::Type::EXP)) {
            args.push_back(invoke_exp(exp));
        }

        std::string_view str_ref = string_const->content;
        str_ref                  = str_ref.substr(
            1, str_ref.length() - 2);  // remove first and last '""

        // replace '\n'
        std::string str;
        split_string_skip_empty(
            str_ref, "\\n", [&str]() { str.push_back('\n'); },
            [&str](std::string_view substr) { str.append(substr); });

        size_t specifier_count = 0;
        split_string_skip_empty(
            str, "%d",
            [this, &specifier_count, &args]() {
                if (args.size() > specifier_count) {
                    EvalResult& eval = args[specifier_count];
                    builder->CreateCall(putint_func->getFunctionType(),
                                        putint_func,
                                        {eval.exp->rvalue(*builder)}, "");
                }
                specifier_count++;
            },
            [this](std::string_view substr) {
                auto* str_value =
                    builder->CreateGlobalString(std::string(substr), ".str");
                auto* str_value_gep = builder->CreateGEP(
                    str_value, {builder->getInt32(0), builder->getInt32(0)},
                    ".str.gep");
                builder->CreateCall(putstr_func->getFunctionType(), putstr_func,
                                    {str_value_gep}, "");
            });
        // Error 'l'
        if (specifier_count != args.size()) {
            reportError(ErrorInfo::Type::PRINTF_FORMAT_MISMATCH, *printf_token,
                        "Printf format mismatch");
        }
    };

    auto invoke_node_child = [&](const NodePtr& first_child) {
        switch (first_child.get()->type) {
            case ASTNode::Type::L_VAL:
                inner_invoke_assign_stmt(first_child);
                break;
            case ASTNode::Type::EXP: {
                EvalResult exp_eval = invoke_exp(*first_child);
                exp_eval.exp->rvalue(*builder);
                break;
            }
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
                inner_invoke_break(token);
                break;
            case Token::Type::CONTINUETK:
                inner_invoke_continue(token);
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
    auto exp_rg  = node.children.equal_range(ASTNode::Type::EXP);

    auto lval_it = lval_rg.begin();
    auto exp_it  = exp_rg.begin();
    for (; lval_it != lval_rg.end(); ++lval_it, ++exp_it) {
        ASSERT(exp_it != exp_rg.end());

        const ASTNode& lval = *lval_it;
        const ASTNode& exp  = *exp_it;

        auto [lval_eval, lval_token] = invoke_lval(lval);

        // LVal 为常量时，不能对其修改。报错行号为 LVal 所在行号。
        if (lval_eval.type.const_flag) {
            reportError(ErrorInfo::Type::CONST_ASSIGNMENT, lval_token,
                        "Can not modify constant");
        }

        EvalResult exp_eval = invoke_exp(exp);
        builder->CreateStore(exp_eval.exp->rvalue(*builder),
                             lval_eval.exp->lvalue(*builder));
    }
}

// 表达式
// Exp -> AddExp
EvalResult Visitor::invoke_exp(const ASTNode& node) const {
    if (node.type != ASTNode::Type::EXP &&
        node.type != ASTNode::Type::CONST_EXP) {
        UNEXPECTED_TYPE(node.type);
    }

    const ASTNode* add_exp = node.children.get(ASTNode::Type::ADD_EXP);
    ASSERT(add_exp);
    return invoke_add_exp(*add_exp);
}

// 条件表达式
// Cond -> LOrExp
std::unique_ptr<Cond> Visitor::invoke_cond(const ASTNode& node) const {
    ASSERT_AST_TYPE(COND, node);
    const ASTNode* lor_exp = node.children.get(ASTNode::Type::LOR_EXP);
    ASSERT(lor_exp);
    return invoke_lor_exp(*lor_exp);
}

// 基本表达式
// PrimaryExp -> '(' Exp ')' | LVal | Number
EvalResult Visitor::invoke_primary_exp(const ASTNode& node) const {
    if (node.children.get(Token::Type::LPARENT)) {
        const ASTNode* exp = node.children.get(ASTNode::Type::EXP);
        ASSERT(exp);
        return invoke_exp(*exp);
    } else if (const ASTNode* lval = node.children.get(ASTNode::Type::L_VAL)) {
        auto [lval_eval, lval_token] = invoke_lval(*lval);
        return std::move(lval_eval);
    } else if (const ASTNode* number =
                   node.children.get(ASTNode::Type::NUMBER)) {
        return invoke_number(*number);
    } else {
        UNREACHABLE();
    }
}

// 左值表达式
// LVal -> Ident ['[' Exp ']']
// Error type: c 未定义的名字
//               使用了未定义的标识符
//               报错行号为 Ident 所在 行数。
std::tuple<EvalResult, Token> Visitor::invoke_lval(const ASTNode& node) const {
    ASSERT_AST_TYPE(L_VAL, node);

    const Token* ident = node.children.get(Token::Type::IDENFR);
    ASSERT(ident);

    const SymbolTable::Record* record = symbol_table.find(ident->content);
    if (record == nullptr) {
        reportError(ErrorInfo::Type::UNDEFINED_IDENT, *ident,
                    "Use of undefined ident");
    }

    bool                 has_index;
    std::unique_ptr<Exp> ret_exp;
    if (node.children.get(Token::Type::LBRACK)) {
        const ASTNode* exp = node.children.get(ASTNode::Type::EXP);
        ASSERT(exp);
        EvalResult exp_eval = invoke_exp(*exp);
        if (record)
            ret_exp = std::make_unique<ArrayAccessExp>(*record,
                                                       std::move(exp_eval.exp));
        has_index = true;
    } else {
        if (record) {
            if (record->attr.type.is_array) {
                ret_exp = std::make_unique<PtrVarExp>(*record);
            } else {
                ret_exp = std::make_unique<IntVarExp>(*record);
            }
        }
        has_index = false;
    }

    if (!record) {
        ret_exp = std::make_unique<PoisonIntVarExp>();
    }

    SymbolType type;
    if (record) {
        type = record->attr.type;
        if (type.is_array && has_index) {
            type.is_array = false;
        }
    }
    EvalResult result;
    result.type = type;
    result.exp  = std::move(ret_exp);
    return std::make_tuple(std::move(result), *ident);
}

static int32_t evaluate_number(std::string_view intcon) {
    int32_t result = 0;
    for (char c : intcon) {
        result = result * 10 + (c - '0');
    }
    return result;
}

// 数值 Number -> IntConst
EvalResult Visitor::invoke_number(const ASTNode& node) const {
    ASSERT_AST_TYPE(NUMBER, node);

    const Token* intcon = node.children.get(Token::Type::INTCON);
    ASSERT(intcon);
    int32_t value = evaluate_number(intcon->content);

    EvalResult eval_result;
    eval_result.type.const_flag = true;
    eval_result.exp             = std::make_unique<IntLiteralExp>(value);
    return eval_result;
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
EvalResult Visitor::invoke_unary_exp(const ASTNode& node) const {
    if (const ASTNode* primary_exp =
            node.children.get(ASTNode::Type::PRIMARY_EXP)) {
        return invoke_primary_exp(*primary_exp);
    } else if (const Token* ident = node.children.get(Token::Type::IDENFR)) {
        if (const auto* record = symbol_table.find(ident->content)) {
            ASSERT(record->attr.type.is_function);
            const std::vector<SymbolType>& fparams =
                record->attr.type.function_params;

            // 处理函数参数
            std::vector<EvalResult> rparams;
            if (const ASTNode* func_rparams =
                    node.children.get(ASTNode::Type::FUNC_RPARAMS)) {
                rparams = invoke_func_rparams(*func_rparams);
            }
            if (fparams.size() != rparams.size()) {  // 函数参数个数不匹配
                reportError(ErrorInfo::Type::FUNC_ARG_COUNT_MISMATCH, *ident,
                            "Func arg count mismatch");
            } else {  // 函数参数类型不匹配
                auto fparam_it = fparams.begin();
                auto rparam_it = rparams.begin();
                for (; fparam_it != fparams.end(); ++fparam_it, ++rparam_it) {
                    ASSERT(rparam_it != rparams.end());

                    // 对于调用函数时，参数类型不匹配一共有以下几种情况的不匹配：
                    // - 传递数组给变量。
                    // - 传递变量给数组。
                    if (fparam_it->is_array != rparam_it->type.is_array) {
                        reportError(ErrorInfo::Type::FUNC_ARG_TYPE_MISMATCH,
                                    *ident, "");
                    }
                }
            }

            // return void 函数不能有 name
            std::vector<std::unique_ptr<Exp>> args_exp;
            std::transform(
                rparams.begin(), rparams.end(), std::back_inserter(args_exp),
                [](EvalResult& result) { return std::move(result.exp); });
            auto call_exp =
                std::make_unique<FuncCallExp>(*record, std::move(args_exp));

            EvalResult result;
            result.exp = std::move(call_exp);
            // 处理返回值
            // ASSERT(record->attr.base_type != SymbolBaseType::VOID);
            result.type.base_type = record->attr.type.base_type;
            result.type.is_array  = record->attr.type.is_array;
            return result;
        } else {
            reportError(ErrorInfo::Type::UNDEFINED_IDENT, *ident,
                        "Undefined ident");
            EvalResult result;
            result.exp = std::make_unique<PoisonIntVarExp>();
            return result;
        }
    } else if (const ASTNode* unary_op =
                   node.children.get(ASTNode::Type::UNARY_OP)) {
        const Token&   op_token  = invoke_unary_op(*unary_op);
        const ASTNode* unary_exp = node.children.get(ASTNode::Type::UNARY_EXP);
        ASSERT(unary_exp);
        auto unary_exp_eval = invoke_unary_exp(*unary_exp);
        unary_exp_eval.exp  = std::make_unique<UnaryExp>(
            op_token.type, std::move(unary_exp_eval.exp));
        return unary_exp_eval;
    } else {
        UNREACHABLE();
    }
}

// 单目运算符
// UnaryOp -> '+' | '−' | '!'
const Token& Visitor::invoke_unary_op(const ASTNode& node) const {
    ASSERT_AST_TYPE(UNARY_OP, node);

    const auto& token = std::get<Token>(*node.children.begin());
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    ASSERT(token.type == Token::Type::PLUS || token.type == Token::Type::MINU ||
           token.type == Token::Type::NOT);
    return token;
}

// 函数实参表
// FuncRParams -> Exp { ',' Exp }
std::vector<EvalResult> Visitor::invoke_func_rparams(
    const ASTNode& node) const {
    std::vector<EvalResult> evals;
    for (const ASTNode& exp : node.children.equal_range(ASTNode::Type::EXP)) {
        evals.push_back(invoke_exp(exp));
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
EvalResult Visitor::invoke_mul_exp(const ASTNode& node) const {
    auto        it    = node.children.begin();
    const auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_unary_exp(*child);
    while (it != node.children.end()) {
        const auto& op = std::get<Token>(*it);
        ++it;

        const auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_unary_exp(*child);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);

        result1.exp = std::make_unique<BinaryOpIntExp>(
            op.type, std::move(result1.exp), std::move(result2.exp));
    }
    return result1;
}

// 加减表达式
// AddExp -> MulExp | AddExp ('+' | '−') MulExp
// Modified:
//     AddExp -> MulExp { ('+' | '-') MulExp }
EvalResult Visitor::invoke_add_exp(const ASTNode& node) const {
    auto        it    = node.children.begin();
    const auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_mul_exp(*child);
    while (it != node.children.end()) {
        const auto& op = std::get<Token>(*it);
        ++it;

        const auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_mul_exp(*child);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);

        result1.exp = std::make_unique<BinaryOpIntExp>(
            op.type, std::move(result1.exp), std::move(result2.exp));
    }

    return result1;
}

// 关系表达式
// RelExp -> AddExp | RelExp ('<' | '>' | '<=' | '>=') AddExp
// Modified:
//     RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
EvalResult Visitor::invoke_rel_exp(const ASTNode& node) const {
    auto        it    = node.children.begin();
    const auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_add_exp(*child);
    while (it != node.children.end()) {
        [[maybe_unused]] const auto& op = std::get<Token>(*it);
        ++it;

        const auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_add_exp(*child);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);

        result1.exp = std::make_unique<BinaryOpBoolExp>(
            op.type, std::move(result1.exp), std::move(result2.exp));
    }

    return result1;
}

// 相等性表达式
// EqExp -> RelExp | EqExp ('==' | '!=') RelExp
// Modified:
//     EqExp -> RelExp { ('==' | '!=') RelExp }
EvalResult Visitor::invoke_eq_exp(const ASTNode& node) const {
    auto        it    = node.children.begin();
    const auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_rel_exp(*child);
    while (it != node.children.end()) {
        const auto& op = std::get<Token>(*it);
        ++it;

        const auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_rel_exp(*child);
        ASSERT_CALCABLE(result1.type);
        ASSERT_CALCABLE(result2.type);

        result1.exp = std::make_unique<BinaryOpBoolExp>(
            op.type, std::move(result1.exp), std::move(result2.exp));
    }

    return result1;
}

// 逻辑与表达式
// LAndExp -> EqExp | LAndExp '&&' EqExp
// Modified:
//     LAndExp -> EqExp { '&&' EqExp }
std::unique_ptr<Cond> Visitor::invoke_land_exp(const ASTNode& node) const {
    auto        it    = node.children.begin();
    const auto& child = std::get<NodePtr>(*it);
    ++it;

    auto result1 = invoke_eq_exp(*child);
    if (result1.exp->type() == Exp::T_INT)
        result1.exp = std::make_unique<IntToBoolExp>(std::move(result1.exp));

    std::unique_ptr<Cond> cond =
        std::make_unique<SingleCond>(std::move(result1.exp));
    while (it != node.children.end()) {
        ++it;  // skip op
        const auto& child = std::get<NodePtr>(*it);
        ++it;
        auto result2 = invoke_eq_exp(*child);
        if (result2.exp->type() == Exp::T_INT)
            result2.exp =
                std::make_unique<IntToBoolExp>(std::move(result2.exp));

        auto cond2 = std::make_unique<SingleCond>(std::move(result2.exp));
        cond = std::make_unique<LAndCond>(std::move(cond), std::move(cond2));
    }

    return cond;
}

// 辑或表达式
// LOrExp -> LAndExp | LOrExp '||' LAndExp
// Modified:
//     LOrExp -> LAndExp { '||' LAndExp }
std::unique_ptr<Cond> Visitor::invoke_lor_exp(const ASTNode& node) const {
    auto        it    = node.children.begin();
    const auto& child = std::get<NodePtr>(*it);
    ++it;

    std::unique_ptr<Cond> cond = invoke_land_exp(*child);
    while (it != node.children.end()) {
        ++it;  // skip op
        const auto& child = std::get<NodePtr>(*it);
        ++it;
        auto cond2 = invoke_land_exp(*child);
        cond = std::make_unique<LOrCond>(std::move(cond), std::move(cond2));
    }

    return cond;
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
void Visitor::add_symbol(Token idenfr_token, SymbolAttr attr) {
    ASSERT_TOKEN_TYPE(IDENFR, idenfr_token);

    if (symbol_table.exist_in_scope(idenfr_token.content)) {
        reportError(ErrorInfo::Type::REDEFINED_IDENT, idenfr_token,
                    "Refined ident");
        // return {};
    } else {
        // auto& attr_ref =
        symbol_table.try_add_symbol(idenfr_token.content, attr);

        // size_t index = records.size();
        records.emplace_back(
            SymbolRecord{current_scope, idenfr_token.content, attr});
        // return std::make_unique<SymbolAttrFillBackHandler>(
        //     SymbolAttrFillBackHandler{attr_ref, index});
    }
}

// void Visitor::fill_back_attr(const SymbolAttrFillBackHandler& handler,
//                              const SymbolAttr& attr) {
//     handler.attr_in_symbol_table = attr;
//     records.at(handler.attr_record_index).attr = attr;
// }

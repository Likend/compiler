#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <tuple>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/IRBuilder.hpp"
#include "ir/LLVMContext.hpp"
#include "ir/Module.hpp"
#include "ir/Value.hpp"
// #include <llvm/Support/raw_ostream.h>

#include "evaluate.hpp"
#include "grammer.hpp"
#include "symbol_table.hpp"
#include "token.hpp"

struct SymbolRecord {
    size_t           scope_index;
    std::string_view ident_name;
    SymbolAttr       attr;
};

struct ScopeInfo {
    SymbolBaseType return_type = SymbolBaseType::INT;
    struct ForInfo {
        ir::BasicBlock* update_bb;  // continue jump to here
        ir::BasicBlock* after_bb;   // break jump to here
    };
    std::optional<ForInfo> for_info = std::nullopt;

    bool in_for() const { return for_info.has_value(); }
};

struct EvalResult {
    SymbolType           type{};
    std::unique_ptr<Exp> exp;
};

class Visitor {
   public:
    Visitor(const ASTNode& node);

    Visitor(const Visitor&)            = delete;
    Visitor& operator=(const Visitor&) = delete;

    // void operator()(const ASTNode& node);
    std::vector<SymbolRecord> records;

    void write_ir(std::ostream& os) { os << module.get(); }

   private:
    void invoke_comp_unit(const ASTNode& node);
    void invoke_func_def(const ASTNode& node);  // FUNC_DEF and MAIN_FUNC_DEF
    std::vector<std::tuple<SymbolType, Token>> invoke_func_params(
        const ASTNode& node);
    std::tuple<SymbolType, Token> invoke_func_param(const ASTNode& node);
    void invoke_var_decl(const ASTNode& node);  // VAR_DECL and CONST_DECL
    void invoke_var_def(const ASTNode& node, bool const_flag,
                        bool static_flag);  // VAR_DEF and CONST_DEF
    std::vector<EvalResult> invoke_var_init_val(
        const ASTNode& node,
        bool           const_flag);  // INIT_VAL and CONST_INIT_VAL

    void invoke_block(const ASTNode& node, ScopeInfo scope_info);
    void invoke_stmt(const ASTNode& node, ScopeInfo scope_info);
    void invoke_for_stmt(const ASTNode& node);

    // Expression and condition
    EvalResult invoke_exp(const ASTNode& node) const;  // EXP and CONST_EXP
    std::unique_ptr<Cond> invoke_cond(const ASTNode& node) const;

    std::tuple<EvalResult, Token> invoke_lval(const ASTNode& node) const;
    EvalResult                    invoke_number(const ASTNode& node) const;
    const Token&                  invoke_unary_op(const ASTNode& node) const;

    EvalResult              invoke_primary_exp(const ASTNode& node) const;
    EvalResult              invoke_unary_exp(const ASTNode& node) const;
    std::vector<EvalResult> invoke_func_rparams(const ASTNode& node) const;

    EvalResult            invoke_mul_exp(const ASTNode& node) const;
    EvalResult            invoke_add_exp(const ASTNode& node) const;
    EvalResult            invoke_rel_exp(const ASTNode& node) const;
    EvalResult            invoke_eq_exp(const ASTNode& node) const;
    std::unique_ptr<Cond> invoke_land_exp(const ASTNode& node) const;
    std::unique_ptr<Cond> invoke_lor_exp(const ASTNode& node) const;

    // Tools
    void push_scope();
    void pop_scope();

    // struct SymbolAttrFillBackHandler {
    //     SymbolAttr& attr_in_symbol_table;
    //     size_t attr_record_index;
    // };

    void add_symbol(Token idenfr_token, SymbolAttr attr);

    // void fill_back_attr(const SymbolAttrFillBackHandler& handler,
    //                     const SymbolAttr& attr);

   private:
    std::unique_ptr<ir::LLVMContext> context =
        std::make_unique<ir::LLVMContext>();
    std::unique_ptr<ir::Module> module =
        std::make_unique<ir::Module>("module", *context);
    std::unique_ptr<ir::IRBuilder> builder =
        std::make_unique<ir::IRBuilder>(*context);

    size_t             current_scope    = 1;
    size_t             new_define_scope = 1;
    std::stack<size_t> scope_stack;
    SymbolTable        symbol_table;

    // pre-declared functions;
    ir::Function* putint_func;
    ir::Function* putch_func;
    ir::Function* putstr_func;

    // init global scope var function
    ir::Function*   init_global_func;
    ir::BasicBlock* init_global_bb;
};

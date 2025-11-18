#pragma once

#include <optional>
#include <stack>
#include <tuple>

#include "grammer.hpp"
#include "symbol_table.hpp"
#include "token.hpp"

struct SymbolRecord {
    size_t scope_index;
    std::string_view ident_name;
    SymbolAttr attr;
};

struct ScopeInfo {
    SymbolBaseType return_type = SymbolBaseType::INT;
    bool in_for_loop = false;
};

struct EvalOption {
    bool const_flag = false;
};

struct EvalResult {
    SymbolType type = {};
    std::optional<int> constexpr_value = std::nullopt;
};

class Visitor {
   public:
    Visitor();

    void operator()(const ASTNode& node);
    std::vector<SymbolRecord> records;

   private:
    void invoke_comp_unit(const ASTNode& node);
    void invoke_func_def(const ASTNode& node);  // FUNC_DEF and MAIN_FUNC_DEF
    std::vector<SymbolType> invoke_func_params(const ASTNode& node);
    SymbolAttr invoke_func_param(const ASTNode& node);
    void invoke_var_decl(const ASTNode& node);  // VAR_DECL and CONST_DECL
    void invoke_var_def(const ASTNode& node, bool const_flag,
                        bool static_flag);  // VAR_DEF and CONST_DEF
    void invoke_var_init_val(const ASTNode& node,
                             bool const_flag);  // INIT_VAL and CONST_INIT_VAL

    void invoke_block(const ASTNode& node, ScopeInfo scope_info);
    void invoke_stmt(const ASTNode& node, ScopeInfo scope_info);
    void invoke_for_stmt(const ASTNode& node);

    EvalResult invoke_exp(const ASTNode& node,
                          EvalOption option = {});  // EXP and CONST_EXP
    EvalResult invoke_cond(const ASTNode& node, EvalOption option = {});

    std::tuple<EvalResult, Token> invoke_lval(const ASTNode& node);
    std::tuple<EvalResult, Token> invoke_number(const ASTNode& node);
    const Token& invoke_unary_op(const ASTNode& node);

    EvalResult invoke_primary_exp(const ASTNode& node, EvalOption option = {});
    EvalResult invoke_unary_exp(const ASTNode& node, EvalOption option = {});
    std::vector<EvalResult> invoke_func_rparams(const ASTNode& node,
                                                EvalOption option = {});

    EvalResult invoke_mul_exp(const ASTNode& node, EvalOption option = {});
    EvalResult invoke_add_exp(const ASTNode& node, EvalOption option = {});
    EvalResult invoke_rel_exp(const ASTNode& node, EvalOption option = {});
    EvalResult invoke_eq_exp(const ASTNode& node, EvalOption option = {});
    EvalResult invoke_land_exp(const ASTNode& node, EvalOption option = {});
    EvalResult invoke_lor_exp(const ASTNode& node, EvalOption option = {});

    void push_scope();
    void pop_scope();

    struct SymbolAttrFillBackHandler {
        SymbolAttr& attr_in_symbol_table;
        size_t attr_record_index;
    };

    std::optional<SymbolAttrFillBackHandler> add_symbol(Token idenfr_token,
                                                        SymbolAttr attr,
                                                        char error_type = 'b');

    void fill_back_attr(const SymbolAttrFillBackHandler& handler,
                        const SymbolAttr& attr);

   private:
    size_t current_scope = 1;
    size_t new_define_scope = 1;
    std::stack<size_t> scope_stack;
    SymbolTable symbol_table;
};

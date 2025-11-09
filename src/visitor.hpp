#pragma once

#include <stack>

#include "grammer.hpp"
#include "symbol_table.hpp"
#include "token.hpp"

struct SymbolRecord {
    size_t scope_level;
    std::string_view ident_name;
    SymbolType type;
};

class Visitor {
    size_t current_scope = 1;
    size_t new_define_scope = 1;
    std::stack<size_t> scope_stack;
    SymbolTable symbol_table;
    std::vector<SymbolRecord> records;

   public:
    void operator()(const ASTNode& node);

   private:
    void invoke_comp_unit(const ASTNode& node);
    void invoke_func_def(const ASTNode& node);  // FUNC_DEF and MAIN_FUNC_DEF
    void invoke_func_params(const ASTNode& node);
    void invoke_func_param(const ASTNode& node);
    void invoke_var_decl(const ASTNode& node);  // VAR_DECL and CONST_DECL
    void invoke_var_def(const ASTNode& node);   // VAR_DEF and CONST_DEF
    void invoke_block(const ASTNode& node, bool require_return = false);

    void push_scope();
    void pop_scope();

    void add_symbol(Token idenfr_token, SymbolAttr attr, SymbolType type,
                    char error_type = 'b');
};

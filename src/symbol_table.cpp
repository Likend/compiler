#include "symbol_table.hpp"

#include <cassert>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>

SymbolTable::SymbolTable() { scope_boundaries.push(set.get_scope_marker()); };

const SymbolTable::Record* SymbolTable::find(
    std::string_view symbol_name) const {
    if (auto it = set.find(symbol_name); it != set.end()) {
        return &(*it);
    }
    return nullptr;
}

void SymbolTable::push_scope() {
    auto marker = set.get_scope_marker();
    scope_boundaries.push(marker);
}

void SymbolTable::pop_scope() {
    // 检查是否可以弹出作用域（不能弹出全局作用域）
    if (current_scope_level() == 0) {
        throw std::runtime_error("Cannot pop the global scope");
    }
    auto marker = scope_boundaries.top();
    set.pop_scope(marker);
    scope_boundaries.pop();
}

bool SymbolTable::exist_in_scope(std::string_view symbol_name) const {
    const Record* record = find(symbol_name);
    return record && record->scope_level == current_scope_level();
}

bool SymbolTable::try_add_symbol(std::string_view symbol_name,
                                 SymbolAttr symbol_attr) {
    // 检查符号是否已在当前作用域中存在
    if (exist_in_scope(symbol_name)) return false;  // 符号已存在，添加失败

    // 创建新节点
    Record record{std::string(symbol_name), symbol_attr, current_scope_level()};
    set.insert(std::move(record));

    return true;
}

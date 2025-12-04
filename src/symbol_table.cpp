#include "symbol_table.hpp"

#include <cassert>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>

#include "util/assert.hpp"

size_t SymbolType::size() const {
    size_t item_size = (base_type == SymbolBaseType::INT) ? 4 : 0;
    size_t count = is_array ? *array_count : 1;
    return item_size * count;
}

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

SymbolAttr& SymbolTable::try_add_symbol(std::string_view symbol_name,
                                        SymbolAttr symbol_attr) {
    // // 检查符号是否已在当前作用域中存在
    // if (exist_in_scope(symbol_name)) return nullptr;  // 符号已存在，添加失败

    // 创建新节点
    Record record{std::string(symbol_name), symbol_attr, current_scope_level()};
    auto& ref = set.insert(std::move(record));
    // do not modify ref.name
    return ref.attr;
}

std::ostream& operator<<(std::ostream& os, const SymbolType& symbol_type) {
    if (symbol_type.const_flag) os << "Const";
    if (symbol_type.static_flag) os << "Static";
    switch (symbol_type.base_type) {
        case SymbolBaseType::INT:
            os << "Int";
            break;
        case SymbolBaseType::VOID:
            os << "Void";
            break;
        default:
            UNREACHABLE();
    }
    if (symbol_type.is_array) os << "Array";
    if (symbol_type.is_function) os << "Func";
    if (symbol_type.array_count) {
        os << '[' << *symbol_type.array_count << ']';
    }
    return os;
}

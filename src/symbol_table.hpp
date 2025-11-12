#pragma once

#include <cstddef>
#include <functional>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

#include "util/scope_hash_set.hpp"

// enum class SymbolType { INT, CONST_INT, STATIC_INT, INT_FUNC, INT_ARRAY };

enum class SymbolBaseType { INT, VOID };

struct SymbolAttr {
    SymbolBaseType base_type = SymbolBaseType::INT;
    bool const_flag = false;
    bool static_flag = false;
    bool is_array = false;
    bool is_function = false;

    std::vector<SymbolAttr> function_params;

    SymbolAttr& set_base_type(SymbolBaseType base_type) {
        this->base_type = base_type;
        return *this;
    }

    SymbolAttr& set_const(bool const_flag = true) {
        this->const_flag = const_flag;
        return *this;
    }

    SymbolAttr& set_static(bool static_flag = true) {
        this->static_flag = static_flag;
        return *this;
    }

    SymbolAttr& set_array(bool is_array = true) {
        this->is_array = is_array;
        return *this;
    }

    SymbolAttr& set_function(std::vector<SymbolAttr> function_params = {}) {
        this->is_function = true;
        this->function_params = std::move(function_params);
        return *this;
    }
};

static SymbolAttr default_symbol_attr {};

class SymbolTable {
   public:
    struct Record {
        std::string name;    // 符号名称
        SymbolAttr attr;   // 符号属性
        size_t scope_level;  // 作用域层级
    };

    SymbolTable();

    /**
     * @brief 获取当前作用域层级
     * @return size_t 当前作用域层级（全局作用域为0）
     */
    size_t current_scope_level() const { return scope_boundaries.size() - 1; }

    void push_scope();
    void pop_scope();

    /**
     * @brief 查找符号（在所有作用域中）
     * @param symbol_name 符号名称
     * @return std::optional<std::tuple<const SymbolAttr&, size_t>>
     *         如果找到, 则返回符号属性SymbolAttr和所在的层级
     */
    const Record* find(std::string_view symbol_name) const;

    /**
     * @brief 检查符号是否在当前作用域中已存在
     * @param symbol_name 符号名称
     * @return bool 是否存在
     */
    bool exist_in_scope(std::string_view symbol_name) const;

    /**
     * @brief 尝试添加符号到当前作用域
     * @param symbol_name 符号名称
     *                    (注意: 要求调用方保证字符串生命周期超过符号表对象)
     * @param symbol_attr 符号属性
     * @return SymbolAttr* 是否添加成功（如果已存在则失败）
     */
    SymbolAttr& try_add_symbol(std::string_view symbol_name,
                               SymbolAttr symbol_attr);

   private:
    struct Hash {
        size_t operator()(const std::string_view& name) const {
            return std::hash<std::string_view>{}(name);
        }
        size_t operator()(const Record& node) const {
            return this->operator()(node.name);
        };
    };

    struct Pred {
        bool operator()(const std::string_view& a,
                        const std::string_view& b) const {
            return a == b;
        }
        bool operator()(const std::string_view& a, const Record& b) const {
            return this->operator()(a, b.name);
        }
        bool operator()(const Record& a, const std::string_view& b) const {
            return this->operator()(a.name, b);
        }
        bool operator()(const Record& a, const Record& b) const {
            return this->operator()(a.name, b.name);
        }
    };

    ScopeHashSet<Record, Hash, Pred> set;

    using ScopeMarker = ScopeHashSet<Record, Hash, Pred>::ScopeMarker;
    std::stack<ScopeMarker> scope_boundaries;

    // struct Impl;
    // std::unique_ptr<Impl> pImpl;
};

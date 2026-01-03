#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "util/IntrusiveList.hpp"
namespace ir {
class Value;
class Function;
class Module;
class BasicBlock;
class Value;

class ValueSymbolTable {
    std::unordered_map<std::string, Value*> vmap;

   public:
    Value* lookup(std::string_view name) const;
    bool   empty() const { return vmap.empty(); }
    size_t size() const { return vmap.size(); }

   private:
    template <typename Derive, typename Parent>
    friend class ValueNode;

    std::string makeUniqueName(std::string name) const;
    void        removeValueName(std::string_view name);
    void        insertValue(Value* value);
};

namespace value_node_detail {
ValueSymbolTable& getSymTab(Function*);
ValueSymbolTable& getSymTab(Module*);
ValueSymbolTable& getSymTab(BasicBlock*);
bool              isVoid(Value*);
}  // namespace value_node_detail

template <typename Derive, typename Parent>
class ValueNode : public IntrusiveNodeWithParent<Parent> {
    template <typename T>
    friend struct ::IntrusiveNodeTraits;

   public:
    ValueNode() = default;

   private:
    ValueNode(Parent* parent) : IntrusiveNodeWithParent<Parent>(parent) {}

    using intrusive_node_type = ValueNode;

    Derive& self() { return *static_cast<Derive*>(this); }

    static ValueNode* next(ValueNode* curr) {
        return static_cast<ValueNode*>(
            IntrusiveNodeWithParent<Parent>::next(curr));
    }

    static ValueNode* prev(ValueNode* curr) {
        return static_cast<ValueNode*>(
            IntrusiveNodeWithParent<Parent>::prev(curr));
    }

    static void link_between(ValueNode* curr, ValueNode* prev,
                             ValueNode* next) {
        IntrusiveNodeWithParent<Parent>::link_between(curr, prev, next);
        if (value_node_detail::isVoid(&curr->self())) return;
        ValueSymbolTable* symTab;
        if constexpr (std::is_same_v<Parent, Function>) {
            Function* f = prev->parent();
            symTab      = &value_node_detail::getSymTab(f);
        } else if constexpr (std::is_same_v<Parent, Module>) {
            Module* m = prev->parent();
            symTab    = &value_node_detail::getSymTab(m);
        } else if constexpr (std::is_same_v<Parent, BasicBlock>) {
            BasicBlock* bb = prev->parent();
            symTab         = &value_node_detail::getSymTab(bb);
        } else {
            static_assert(std::is_same_v<Parent, Function> ||
                              std::is_same_v<Parent, Module> ||
                              std::is_same_v<Parent, BasicBlock>,
                          "Parent must be Function, Module, or BasicBlock");
        }
        std::string name =
            symTab->makeUniqueName(std::string(curr->self().getName()));
        curr->self().setName(std::move(name));
        symTab->insertValue(&curr->self());
    }

    static ValueNode* create_sentinel(Parent& parent) {
        return new ValueNode{&parent};
    }
};
}  // namespace ir

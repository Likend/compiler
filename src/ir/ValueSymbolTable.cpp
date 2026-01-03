#include "ir/ValueSymbolTable.hpp"

#include <string>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Module.hpp"
#include "ir/Value.hpp"
#include "util/assert.hpp"

using namespace ir;

Value* ValueSymbolTable::lookup(std::string_view name) const {
    if (auto it = vmap.find(std::string(name)); it != vmap.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

std::string ValueSymbolTable::makeUniqueName(std::string name) const {
    size_t originalSize = name.size();

    size_t count = 1;
    while (lookup(name) != nullptr) {
        name.resize(originalSize);
        name.push_back('.');
        name.append(std::to_string(count));
        count += 1;
    }
    return name;
}

void ValueSymbolTable::removeValueName(std::string_view name) {
    if (auto it = vmap.find(std::string(name)); it != vmap.end()) {
        vmap.erase(it);
    }
}

void ValueSymbolTable::insertValue(Value* value) {
    ASSERT_WITH(lookup(value->getName()) == nullptr, "Value with",
                value->getName(), "already in table");
    vmap[std::string(value->getName())] = value;
}

ValueSymbolTable& value_node_detail::getSymTab(BasicBlock* bb) {
    return bb->parent()->getValueSymbolTable();
}

ValueSymbolTable& value_node_detail::getSymTab(Module* m) {
    return m->getValueSymbolTable();
}

ValueSymbolTable& value_node_detail::getSymTab(Function* f) {
    return f->getValueSymbolTable();
}

bool value_node_detail::isVoid(Value* value) {
    return value->getType()->isVoidTy();
}

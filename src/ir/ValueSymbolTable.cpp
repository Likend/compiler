#include "ir/ValueSymbolTable.hpp"

#include <string>

#include "ir/Value.hpp"
#include "util/assert.hpp"

using namespace ir;

Value* ValueSymbolTable::lookup(std::string_view name) const {
    if (auto it = vmap.find(name); it != vmap.end()) {
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
    if (auto it = vmap.find(name); it != vmap.end()) {
        vmap.erase(it);
    }
}

void ValueSymbolTable::insertValue(Value* value) {
    ASSERT_WITH(lookup(value->getName()) == nullptr,
                "Value with name already in table");
    vmap[value->getName()] = value;
}

#pragma once

#include <string_view>
#include <unordered_map>
namespace ir {
class Value;

class ValueSymbolTable {
    std::unordered_map<std::string_view, Value*> vmap;

   public:
    Value* lookup(std::string_view name) const;
    bool   empty() const { return vmap.empty(); }
    size_t size() const { return vmap.size(); }

   private:
    friend class Value;
    std::string makeUniqueName(std::string name) const;
    void        removeValueName(std::string_view name);
    void        insertValue(Value* value);
};
}  // namespace ir

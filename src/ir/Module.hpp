#pragma once

#include <string_view>

#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/IntrusiveList.hpp"

namespace ir {
class LLVMContext;
class Module : public IntrusiveList<Function> {
   private:
    LLVMContext&     context;
    std::string      moduleID;
    ValueSymbolTable symbolTable;
    // std::string sourceFileName;
    friend class Constant;
    friend class Function;
    friend class GlobalVariable;

   public:
    ~Module() {
        // 手动调用 IntrusiveList 的析构，保证此时 symbolTable 有效
        this->clear();
    }

    explicit Module(std::string moduleID, LLVMContext& c);
    std::string_view getName() const { return moduleID; }

    Function*       getFunction(std::string_view name) const;
    GlobalVariable* getGlobalVariable(std::string_view name) const;

    const ValueSymbolTable& getValueSymbolTable() const { return symbolTable; }
    ValueSymbolTable&       getValueSymbolTable() { return symbolTable; }

    IntrusiveList<GlobalVariable> globals;
};
}  // namespace ir

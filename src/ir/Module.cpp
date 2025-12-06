#include "ir/Module.hpp"

#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/LLVMContext.hpp"

using namespace ir;
Module::Module(std::string moduleID, LLVMContext& c)
    : context(c), moduleID(std::move(moduleID)) {
    context.addModule(this);
}

Function* Module::getFunction(std::string_view name) const {
    return dynamic_cast<Function*>(symbolTable.lookup(name));
}

GlobalVariable* Module::getGlobalVariable(std::string_view name) const {
    return dynamic_cast<GlobalVariable*>(symbolTable.lookup(name));
}

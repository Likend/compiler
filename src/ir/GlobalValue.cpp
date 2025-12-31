#include "ir/GlobalValue.hpp"

#include "ir/Module.hpp"

using namespace ir;

GlobalVariable* ir::GlobalVariable::Create(
    Module& module, Type* ty, bool isConstant, LinkageTypes linkage,
    Constant* initializer, std::string name, unsigned addrSpace) {
    auto* gv = new GlobalVariable(ty, isConstant, linkage, initializer,
                                  std::move(name), addrSpace);
    module.globals.push_back(gv);
    return gv;
}

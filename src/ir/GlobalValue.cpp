#include "ir/GlobalValue.hpp"

#include "ir/Module.hpp"
#include "util/assert.hpp"

using namespace ir;

GlobalVariable::GlobalVariable(Module& module, Type* ty, bool isConstant,
                               LinkageTypes linkage, Constant* initializer,
                               std::string name, unsigned addrSpace)
    : GlobalObject(ty, 1, linkage, std::move(name), addrSpace, module),
      isConstantGlobal(isConstant) {
    ASSERT(module.getGlobalVariable(name) == nullptr);
    module.globalList.emplace_back(std::unique_ptr<GlobalVariable>(this));
    if (initializer) setOperand(0, initializer);
}

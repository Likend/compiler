#include "ir/Function.hpp"

#include <memory>

#include "ir/BasicBlock.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"

using namespace ir;

Function::Function(FunctionType* ty, LinkageTypes linkage, unsigned addrSpace,
                   std::string name, Module& module)
    : GlobalObject(ty, 0, linkage, std::move(name), addrSpace, module) {
    ASSERT(module.getFunction(name) == nullptr);
    module.functionList.emplace_back(std::unique_ptr<Function>(this));
    buildArguments();
}
Function::~Function() = default;

void Function::buildArguments() {
    FunctionType* funcTy = getFunctionType();
    arguments.reserve(funcTy->getNumParams());
    for (size_t i = 0; i < funcTy->getNumParams(); i++) {
        Type* argTy = funcTy->getParamType(i);
        arguments.push_back(std::make_unique<Argument>(
            argTy, "", this, static_cast<unsigned>(i)));
    }
}

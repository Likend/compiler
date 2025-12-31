#include "ir/Function.hpp"

#include "ir/BasicBlock.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"

using namespace ir;

Function::Function(FunctionType* ty, LinkageTypes linkage, std::string name,
                   unsigned addrSpace)
    : GlobalObject(ty, 0, linkage, std::move(name), addrSpace),
      IntrusiveList<BasicBlock>(*this),
      args(*this) {
    buildArguments();
}
Function::~Function() = default;

void Function::buildArguments() {
    FunctionType* funcTy = getFunctionType();
    for (size_t i = 0; i < funcTy->getNumParams(); i++) {
        Type* argTy = funcTy->getParamType(i);
        args.push_back(new Argument{argTy, static_cast<unsigned>(i)});
    }
}

ir::Function* ir::Function::Create(FunctionType* ty, LinkageTypes linkage,
                                   unsigned addrSpace, std::string name,
                                   Module& module) {
    auto* f = new Function(ty, linkage, std::move(name), addrSpace);
    module.push_back(f);
    return f;
}

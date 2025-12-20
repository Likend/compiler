#include "ir/Pass.hpp"

#include "ir/Module.hpp"

using namespace ir;

void FunctionPass::runOnModule(Module& module) {
    for (auto& function : module) {
        runOnFunction(function);
    }
}

void PassManager::run(Module& module) {
    for (auto& pass : passes) {
        pass->doInitialization(module);
        pass->runOnModule(module);
        pass->doFinalization(module);
    }
}

void PassManager::establishManager() {
    for (auto& pass : passes) {
        pass->manager = this;
    }
}

#include "ir/Pass.hpp"

#include "ir/Module.hpp"

using namespace ir;

bool FunctionPass::runOnModule(Module& module) {
    bool changed = false;
    for (auto& function : module) {
        changed = changed | runOnFunction(function);
    }
    return changed;
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

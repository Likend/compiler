#include "ir/Pass.hpp"

#include <exception>
#include <iostream>

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
        try {
            pass->doInitialization(module);
            pass->runOnModule(module);
            pass->doFinalization(module);
        } catch (std::exception& e) {
            std::cerr << "Pass [" << typeid(*pass).name() << "] failed!"
                      << std::endl
                      << "Message: " << e.what() << std::endl;
            throw;
        }
    }
}

void PassManager::establishManager() {
    for (auto& pass : passes) {
        pass->manager = this;
    }
}

#include "ir/WellForm.hpp"

#include "ir/Function.hpp"

using namespace ir;

static bool RemoveDuplicateTerminator(ir::BasicBlock* bb) {
    auto it = bb->begin();
    for (; it != bb->end(); ++it) {
        if (it->isTerminator()) {
            break;
        }
    }
    ++it;
    if (it == bb->end()) return false;
    for (; it != bb->end();) {
        ASSERT(it->use_empty());
        it->dropAllReferences();
        it = bb->erase(it);
    }
    return true;
}

bool WellFormPass::runOnFunction(ir::Function& f) {
    bool changed = false;
    for (ir::BasicBlock& bb : f) {
        changed |= RemoveDuplicateTerminator(&bb);
    }
    return changed;
}

#include "opt/RemoveUnuse.hpp"

#include <queue>
#include <unordered_set>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"

using namespace ir;
using namespace opt;

bool RemoveUnusePass::runOnFunction(Function& f) {
    bool changed = RemoveUnreachableBranch(f);

    std::unordered_set<Instruction*> worklist;

    for (BasicBlock& bb : f) {
        for (Instruction& inst : bb) {
            worklist.insert(&inst);
        }
    }

    while (!worklist.empty()) {
        Instruction* inst = *worklist.begin();
        worklist.erase(inst);

        if (inst->use_empty()) {
            if (dynamic_cast<CallInst*>(inst) ||
                dynamic_cast<StoreInst*>(inst) || inst->isTerminator())
                continue;
            for (auto& op : inst->operands()) {
                if (auto* instOp = dynamic_cast<Instruction*>(op.get())) {
                    if (instOp->parent()->parent() == &f) {
                        worklist.insert(instOp);
                    }
                }
            }
            inst->dropAllReferences();
            inst->parent()->erase(inst);
            changed = true;
        }
    }

    return changed;
}

bool RemoveUnusePass::RemoveUnreachableBranch(Function& f) {
    if (f.empty()) return false;

    // find reachable branches
    std::unordered_set<BasicBlock*> reachable;
    std::queue<BasicBlock*>         worklist;
    worklist.push(&f.front());

    do {
        BasicBlock* bb = worklist.front();
        worklist.pop();
        reachable.insert(bb);

        for (BasicBlock& succ : bb->successors()) {
            if (reachable.count(&succ) == 0) {
                worklist.push(&succ);
            }
        }
    } while (!worklist.empty());

    bool changed = false;

    for (auto bbIt = f.begin(); bbIt != f.end();) {
        BasicBlock& bb = *bbIt++;
        if (reachable.count(&bb) == 0) {
            for (auto instIt = bb.begin(); instIt != bb.end();) {
                Instruction& inst = *instIt;
                inst.replaceAllUsesWith(PoisonValue::get(inst.getType()));
                inst.dropAllReferences();
                instIt = bb.erase(instIt);
            }
            bb.replaceAllUsesWith(PoisonValue::get(bb.getType()));
            f.erase(bb);
            changed = true;
        }
    }

    return changed;
}

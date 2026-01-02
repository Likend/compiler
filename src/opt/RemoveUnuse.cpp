#include "opt/RemoveUnuse.hpp"

#include <unordered_set>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/Value.hpp"
#include "util/assert.hpp"

using namespace ir;
using namespace opt;

bool RemoveUnusePass::runOnFunction(Function& f) {
    bool changed = false;

    std::unordered_set<Value*> all;

    for (BasicBlock& bb : f) {
        all.insert(&bb);
        for (Instruction& inst : bb) {
            all.insert(&inst);
        }
    }

    std::unordered_set<Value*> worklist = all;

    for (Value* val : worklist) {
        ASSERT(dynamic_cast<BasicBlock*>(val) ||
               dynamic_cast<Instruction*>(val));
    }

    while (!worklist.empty()) {
        Value* val = *worklist.begin();
        worklist.erase(val);
        if (all.find(val) == all.end()) continue;

        if (val->use_empty()) {
            if (auto* bb = dynamic_cast<BasicBlock*>(val)) {
                if (bb == &f.front()) continue;
                changed = true;
                for (auto instIt = bb->begin(); instIt != bb->end();) {
                    Instruction& inst = *instIt;
                    for (auto& op : inst.operands()) {
                        if (all.find(op.get()) != all.end()) {
                            worklist.insert(op.get());
                        }
                    }
                    inst.replaceAllUsesWith(PoisonValue::get(inst.getType()));
                    inst.dropAllReferences();
                    instIt = bb->erase(instIt);
                    all.erase(&inst);
                }
                f.erase(bb);
                all.erase(bb);
            } else if (auto* inst = dynamic_cast<Instruction*>(val)) {
                if (dynamic_cast<CallInst*>(inst) ||
                    dynamic_cast<StoreInst*>(inst) || inst->isTerminator())
                    continue;
                BasicBlock* bb = inst->parent();
                for (auto& op : inst->operands()) {
                    if (all.find(op.get()) != all.end()) {
                        worklist.insert(op.get());
                    }
                }
                inst->dropAllReferences();
                bb->erase(inst);
                all.erase(inst);
            } else {
                UNREACHABLE();
            }
        }
    }

    return changed;
}

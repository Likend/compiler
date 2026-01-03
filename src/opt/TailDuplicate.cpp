#include "opt/TailDuplicate.hpp"

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"

using namespace opt;
using namespace ir;

void copyBlockInto(BasicBlock& dest, BasicBlock& src) {
    Instruction& it = dest.back();

    std::unordered_map<Value*, Value*> vmap;  // old value to new value
    auto getValue = [&vmap](const Value* oldValue) -> Value* {
        if (auto it = vmap.find(const_cast<Value*>(oldValue)); it != vmap.end())
            return it->second;
        else
            return const_cast<Value*>(oldValue);
    };

    for (Instruction& inst : src) {
        Instruction* newInst = inst.clone(getValue);
        dest.insert(it, newInst);
        vmap[&inst] = newInst;
    }

    it.dropAllReferences();
    dest.erase(it);
}

bool duplicateOnce(BasicBlock& bb, std::unordered_set<BasicBlock*>& set) {
    if (bb.succ_size() == 1) {
        // last is br
        auto* br = dynamic_cast<BranchInst*>(&bb.back());
        if (br == nullptr) {
            std::cout << "Check last Instruction in BasicBlock " << bb.getName()
                      << " in Function " << bb.parent()->getName();
            return false;
        }

        BasicBlock& succ = *bb.succ_begin();
        if (set.count(&succ) != 0) return false;  // 死循环

        // if (succ.size() < 5) {
        copyBlockInto(bb, succ);
        set.insert(&succ);
        return true;
        // }
    }
    return false;
}

bool TailDuplicatePass::runOnFunction(Function& f) {
    bool changed = false;

    for (BasicBlock& bb : f) {
        bool                            localChaned;
        std::unordered_set<BasicBlock*> mergedBlocks;
        mergedBlocks.insert(&bb);
        do {
            localChaned = duplicateOnce(bb, mergedBlocks);
        } while (localChaned);
    }

    return changed;
}

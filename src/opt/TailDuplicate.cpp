#include "opt/TailDuplicate.hpp"

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/Value.hpp"

using namespace opt;
using namespace ir;

void copyBlockInto(BasicBlock& dest, BasicBlock& src,
                   std::unordered_map<Value*, Value*>& vmap) {
    Instruction& it = dest.back();

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

bool hasCrossBlockUser(Instruction& inst, BasicBlock& succ) {
    for (ir::User& user : inst.users()) {
        auto* userInst = dynamic_cast<Instruction*>(&user);
        if (!userInst) continue;

        // 如果使用者不在 succ 内部，且不是 succ
        // 唯一的跳转指令（如果是合并的话）
        if (userInst->parent() != &succ) {
            return true;  // 存在外部引用，由于 SSA 维护复杂，先放弃复制
        }
    }
    return false;
}

bool duplicateOnce(BasicBlock& bb, std::unordered_set<BasicBlock*>& set,
                   std::unordered_map<Value*, Value*>& vmap) {
    if (bb.succ_size() == 1) {
        // last is br
        auto* br = dynamic_cast<BranchInst*>(&bb.back());
        if (br == nullptr) {
            std::cout << "Check last Instruction in BasicBlock " << bb.getName()
                      << " in Function " << bb.parent()->getName();
            return false;
        }

        BasicBlock& succ = *bb.succ_begin();

        // 如果有死循环
        if (set.count(&succ) != 0) return false;

        // 产生的值在块外部有使用
        for (Instruction& inst : succ) {
            if (hasCrossBlockUser(inst, succ)) return false;
        }

        // if (succ.size() < 5) {
        copyBlockInto(bb, succ, vmap);
        set.insert(&succ);
        return true;
        // }
    }
    return false;
}

bool TailDuplicatePass::runOnFunction(Function& f) {
    bool changed = false;

    for (BasicBlock& bb : f) {
        bool                               localChaned;
        std::unordered_set<BasicBlock*>    mergedBlocks;
        std::unordered_map<Value*, Value*> vmap;
        mergedBlocks.insert(&bb);
        do {
            localChaned = duplicateOnce(bb, mergedBlocks, vmap);
        } while (localChaned);
    }

    return changed;
}

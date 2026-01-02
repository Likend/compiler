#include "opt/MemToReg.hpp"

#include <unordered_map>
#include <utility>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "util/assert.hpp"

using namespace opt;
using namespace ir;

bool MemToRegPass::runOnFunction(Function& f) {
    bool changed = false;
    for (BasicBlock& bb : f) {
        changed |= replaceInnerBasicBlock(bb);
    }

    return changed;
}

bool MemToRegPass::replaceInnerBasicBlock(BasicBlock& bb) {
    enum State { LOAD, STORE };
    std::unordered_map<Value*, std::pair<Instruction*, State>> map;

    bool changed = false;

    for (auto instIt = bb.begin(); instIt != bb.end();) {
        Instruction& inst = *instIt;
        if (auto* i = dynamic_cast<LoadInst*>(&inst)) {
            // 先尝试插入，如果 key 已存在，insert 会失败并返回已有迭代器
            auto [iter, inserted] =
                map.insert({i->getPointerOperand(), std::make_pair(i, LOAD)});
            if (!inserted) {  //  key 已存在，iter->second 就是旧值
                auto [prevInst, prevState] = iter->second;
                switch (prevState) {
                    case LOAD:
                        i->replaceAllUsesWith(prevInst);
                        break;
                    case STORE:
                        auto* storeInst = dynamic_cast<StoreInst*>(prevInst);
                        ASSERT(storeInst);
                        i->replaceAllUsesWith(storeInst->getValueOperand());
                        break;
                }
                i->dropAllReferences();
                instIt  = bb.erase(i);
                changed = true;
                continue;
            }
        } else if (auto* i = dynamic_cast<StoreInst*>(&inst)) {
            auto [iter, inserted] =
                map.insert({i->getPointerOperand(), std::make_pair(i, STORE)});
            if (!inserted) {
                auto [prevInst, prevState] = iter->second;
                switch (prevState) {
                    case LOAD:
                        break;
                    case STORE:
                        bb.erase(prevInst);
                        changed = true;
                        break;
                }
                iter->second = std::make_pair(i, STORE);
            }
        } else if (auto* i = dynamic_cast<CallInst*>(&inst)) {
            // clear all not alloca
            for (auto mapIt = map.begin(); mapIt != map.end();) {
                if (!dynamic_cast<AllocaInst*>(mapIt->first)) {
                    mapIt = map.erase(mapIt);
                } else {
                    ++mapIt;
                }
            }
            // clear alloca as argument
            for (ir::Use& arg : i->args()) {
                if (auto* alloca = dynamic_cast<AllocaInst*>(arg.get())) {
                    map.erase(alloca);
                }
            }
        }
        ++instIt;
    }
    return changed;
}

#include "opt/MemToReg.hpp"

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "opt/MemToReg.hpp"
#include "util/assert.hpp"

using namespace opt;
using namespace ir;

void check(Function& f) {
    std::unordered_set<Value*> values;

    for (BasicBlock& bb : f) {
        for (Instruction& inst : bb) {
            values.insert(&inst);
        }
    }

    for (BasicBlock& bb : f) {
        for (auto& user : bb.users()) {
            ASSERT(values.count(&user));
        }
        for (Instruction& inst : bb) {
            for (auto& user : inst.users()) {
                ASSERT(values.count(&user));
            }
        }
    }
}

bool MemToRegPass::runOnFunction(Function& f) {
    check(f);
    if (f.empty()) return false;

    bool changed = false;
    for (BasicBlock& bb : f) {
        changed |= replaceInnerBlock(bb);
    }

    check(f);

    changed |= replaceOnceStore(f);

    return changed;
}

bool MemToRegPass::replaceInnerBlock(BasicBlock& bb) {
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
                        prevInst->dropAllReferences();
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

bool MemToRegPass::replaceOnceStore(Function& f) {
    bool changed = false;

    std::vector<AllocaInst*> allocas = getAllocas(f);

    for (AllocaInst* alloca : allocas) {
        std::vector<StoreInst*> stores;
        std::vector<LoadInst*>  loads;

        bool hasOtherUsage = false;
        for (User& user : alloca->users()) {
            if (auto* inst = dynamic_cast<StoreInst*>(&user))
                stores.push_back(inst);
            else if (auto* inst = dynamic_cast<LoadInst*>(&user))
                loads.push_back(inst);
            else {
                hasOtherUsage = true;
                break;
            }
        }

        if (hasOtherUsage) continue;

        Value* loadInstReplacement = nullptr;

        if (stores.size() == 1) {
            StoreInst* storeInst = stores[0];
            ASSERT(storeInst->getPointerOperand() == alloca);
            loadInstReplacement = storeInst->getValueOperand();
            storeInst->dropAllReferences();
            storeInst->parent()->erase(storeInst);
        } else if (stores.size() == 0) {
            loadInstReplacement =
            PoisonValue::get(alloca->getAllocatedType());
        }
        if (loadInstReplacement) {
            changed |= !loads.empty();
            for (LoadInst* loadInst : loads) {
                loadInst->replaceAllUsesWith(loadInstReplacement);
                loadInst->dropAllReferences();
                loadInst->parent()->erase(loadInst);
            }
            ASSERT(alloca->use_empty());
            alloca->dropAllReferences();
            alloca->parent()->erase(alloca);
        }
    }
    return changed;
}

std::vector<AllocaInst*> MemToRegPass::getAllocas(Function& f) {
    BasicBlock&              entry = f.front();
    std::vector<AllocaInst*> allocas;
    for (Instruction& inst : entry) {
        if (auto* ai = dynamic_cast<AllocaInst*>(&inst)) allocas.push_back(ai);
    }
    return allocas;
}

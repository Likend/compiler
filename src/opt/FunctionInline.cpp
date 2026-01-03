#include "opt/FunctionInline.hpp"

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/Module.hpp"
#include "ir/User.hpp"
#include "ir/Value.hpp"
#include "util/assert.hpp"
#include "util/IntrusiveList.hpp"

using namespace opt;
using namespace ir;

struct CallSite {
    Function* caller;
    Function* callee;
    CallInst* ci;

    CallSite(Function* caller, Function* callee, CallInst* ci)
        : caller(caller), callee(callee), ci(ci) {}
};

namespace std {
template <>
struct hash<CallSite> {
    size_t operator()(const CallSite& cs) const noexcept {
        return std::hash<CallInst*>{}(cs.ci);
    }
};

template <>
struct equal_to<CallSite> {
    bool operator()(const CallSite& a, const CallSite& b) const noexcept {
        return a.ci == b.ci;
    }
};
}  // namespace std

struct CallAnalysis {
    std::unordered_set<CallSite> callsites;

    // Add Edge
    void addCall(Function* caller, Function* callee, CallInst* ci) {
        callsites.emplace(caller, callee, ci);
    }
    // Remove edge
    void removeCall(Function* caller, Function* callee, CallInst* ci) {
        callsites.erase(CallSite{caller, callee, ci});
    }
};

CallAnalysis ComputeCallAnalysis(Module& m) {
    CallAnalysis analysis;
    for (Function& f : m) {
        for (BasicBlock& bb : f) {
            for (Instruction& instr : bb) {
                if (auto* ci = dynamic_cast<CallInst*>(&instr)) {
                    auto* callee =
                        dynamic_cast<Function*>(ci->getCalledOperand());
                    if (callee == nullptr) continue;
                    Function* caller = &f;
                    analysis.addCall(caller, callee, ci);
                }
            }
        }
    }
    return analysis;
}

int getInstructionWeight(Instruction* inst) {
    if (dynamic_cast<LoadInst*>(inst) || dynamic_cast<StoreInst*>(inst))
        return 3;
    else if (dynamic_cast<BranchInst*>(inst))
        return 5;
    else if (dynamic_cast<CallInst*>(inst))
        return 10;
    else
        return 1;
}

int InlineBenefit(CallSite* cs, const CallAnalysis& ca) {
    int bonus = 0;

    // 扫描指令计分
    for (BasicBlock& bb : *(cs->callee)) {
        for (Instruction& i : bb) {
            bonus -= getInstructionWeight(&i);
        }
    }

    // 评估常量收益
    for (ir::Use& arg : cs->ci->args()) {
        Value* argValue = arg.get();
        if (dynamic_cast<Constant*>(argValue)) {
            for (ir::User& user : argValue->users()) {
                if (auto* inst = dynamic_cast<Instruction*>(&user))
                    bonus += getInstructionWeight(inst);
            }
        }
    }

    // 评估结构收益
    size_t callCount = 0;
    for (const CallSite& css : ca.callsites) {
        if (css.callee == cs->callee) {
            callCount++;
        }
    }
    if (callCount == 1) bonus += 100;

    return bonus;
}

void InlineOnce(Function* caller, Function* callee, CallInst* ci) {
    BasicBlock* bb = ci->parent();
    BasicBlock* splitBB =
        &*caller->emplace(++IntrusiveIterator{bb}, bb->getContext(),
                          std::string(bb->getName()) + ".split");
    splitBB->splice(splitBB->begin(), *bb, ci);

    std::unordered_map<Value*, Value*> vmap;  // old value to new value
    auto getValue = [&vmap](const Value* oldValue) -> Value* {
        if (auto it = vmap.find(const_cast<Value*>(oldValue)); it != vmap.end())
            return it->second;
        else
            return const_cast<Value*>(oldValue);
    };

    // replace arguments
    auto cinstArgIt  = ci->arg_begin();
    auto calleeArgIt = callee->args.begin();
    while (true) {
        if (cinstArgIt == ci->arg_end()) break;

        vmap[&*calleeArgIt] = *cinstArgIt;

        ++cinstArgIt;
        ++calleeArgIt;
    }

    // transfer alloca
    for (Instruction& inst : callee->front()) {
        if (auto* alloca = dynamic_cast<AllocaInst*>(&inst)) {
            auto* newInst = alloca->clone(getValue);
            caller->front().push_front(newInst);
            vmap[alloca] = newInst;
        }
    }

    // copy basic block (empty)
    for (BasicBlock& calleeBB : *callee) {
        auto newBB      = caller->emplace(splitBB, calleeBB.getContext(),
                                          std::string(calleeBB.getName()));
        vmap[&calleeBB] = &*newBB;
    }

    std::vector<std::pair<Value*, BasicBlock*>> returns;

    // copy instructions
    for (BasicBlock& calleeBB : *callee) {
        auto* callerBB = dynamic_cast<BasicBlock*>(vmap.at(&calleeBB));
        ASSERT(callerBB);
        for (Instruction& calleeInst : calleeBB) {
            if (auto* i = dynamic_cast<ReturnInst*>(&calleeInst)) {
                if (Value* returnValue = i->getReturnValue()) {
                    returns.push_back(
                        std::make_pair(getValue(returnValue), callerBB));
                }  // else return type = void
                callerBB->push_back(new BranchInst{splitBB});
            } else if (auto* i = dynamic_cast<AllocaInst*>(&calleeInst)) {
                ASSERT(vmap.count(i) != 0);
            } else {
                Instruction* newInst = calleeInst.clone(getValue);
                callerBB->push_back(newInst);
                vmap[&calleeInst] = newInst;
            }
        }
    }

    bb->push_back(
        new BranchInst{dynamic_cast<BasicBlock*>(getValue(&callee->front()))});

    if (returns.size() == 0) {
        ASSERT(ci->use_empty());
    } else if (returns.size() == 1) {
        ci->replaceAllUsesWith(returns[0].first);
    } else {
        // append phi
        auto* newPhi =
            new PHINode{ci->getType(), 4, std::string(ci->getName())};
        splitBB->insert(ci, newPhi);

        ci->replaceAllUsesWith(newPhi);
    }

    ci->dropAllReferences();
    splitBB->erase(ci);
}

// TODO remove this
// count ReturnInst in function
size_t countReturnInst(Function& f) {
    size_t count = 0;
    for (BasicBlock& bb : f) {
        Instruction& back = bb.back();
        if (dynamic_cast<ReturnInst*>(&back)) count++;
    }
    return count;
}

bool CandidateInlinableSuggestion(Function* caller, Function* callee) {
    // 只有声明没有定义的外部函数
    if (callee->empty()) return false;

    // 循环与递归检测
    if (callee == caller) return false;

    size_t countReturn = countReturnInst(*callee);
    // 暂时：没有phi节点
    if (countReturn > 1) return false;

    // 函数理应有返回值，但是没有返回语句
    // 函数可能是死循环，返回语句被死代码删除掉了
    if (countReturn == 0 && !callee->getReturnType()->isVoidTy()) return false;

    return true;
}

bool FunctionInlinePass::runOnModule(Module& m) {
    bool changed = false;

    for (Function& f : m) {
        for (auto it = f.user_begin(); it != f.user_end();) {
            User& user = *it++;
            if (auto* ci = dynamic_cast<CallInst*>(&user)) {
                Function* caller = ci->parent()->parent();
                if (CandidateInlinableSuggestion(caller, &f)) {
                    InlineOnce(ci->parent()->parent(), &f, ci);
                    changed = true;
                }
            }
        }
    }
    return changed;
}

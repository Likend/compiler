#include "opt/ConstantPropagation.hpp"

#include <queue>

#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Instructions.hpp"
#include "ir/Type.hpp"

using namespace ir;
using namespace opt;

struct Lattice {
    enum Type { Top, Bottom, Constant };
    Type    type     = Top;
    int32_t constant = 0;
};

struct LatticeAnalysis : std::unordered_map<Instruction*, Lattice> {
    Lattice get(Value* v) {
        if (auto* ci = dynamic_cast<ConstantInt*>(v))
            return {Lattice::Constant, ci->getValue()};
        else if (auto* ci = dynamic_cast<Instruction*>(v))
            return (*this)[ci];
        return {Lattice::Bottom};  // 默认非常量（如函数参数、内存加载）
    }
};

Lattice evaluate(Instruction* inst, LatticeAnalysis& lattices) {
    if (auto* i = dynamic_cast<BinaryOperator*>(inst)) {
        Lattice lhs = lattices.get(i->getLHS());
        Lattice rhs = lattices.get(i->getRHS());

        // 特殊化
        if (i->getBinaryOps() == BinaryOperator::Sub &&
            i->getLHS() == i->getRHS())
            return {Lattice::Constant, 0};
        else if ((i->getBinaryOps() == BinaryOperator::Mul) &&
                 ((lhs.type == Lattice::Constant && lhs.constant == 0) ||
                  (rhs.type == Lattice::Constant && rhs.constant == 0)))
            return {Lattice::Constant, 0};
        else if ((i->getBinaryOps() == BinaryOperator::SDiv ||
                  i->getBinaryOps() == BinaryOperator::SRem) &&
                 (lhs.type == Lattice::Constant && lhs.constant == 0))
            return {Lattice::Constant, 0};

        // 只要有一个是 Bottom，结果就是 Bottom
        if (lhs.type == Lattice::Bottom || rhs.type == Lattice::Bottom)
            return {Lattice::Bottom};

        // 只要有一个是 Top（还没算出来），结果先保持 Top
        if (lhs.type == Lattice::Top || rhs.type == Lattice::Top)
            return {Lattice::Top};

        // 两个都是 Constant，进行常量折叠
        int32_t l = lhs.constant;
        int32_t r = rhs.constant;
        switch (i->getBinaryOps()) {
            case BinaryOperator::Add:
                return {Lattice::Constant, l + r};
            case BinaryOperator::Sub:
                return {Lattice::Constant, l - r};
            case BinaryOperator::Xor:
                return {Lattice::Constant, l ^ r};
            case BinaryOperator::Mul:
                return {Lattice::Constant, l * r};
            case BinaryOperator::SDiv:
                return (r != 0) ? Lattice{Lattice::Constant, l / r}
                                : Lattice{Lattice::Bottom};
            case BinaryOperator::SRem:
                return (r != 0) ? Lattice{Lattice::Constant, l % r}
                                : Lattice{Lattice::Bottom};
                DEFAULT_UNREACHABLE();
        }
    }

    else if (auto* i = dynamic_cast<ICmpInst*>(inst)) {
        Lattice lhs = lattices.get(i->getLHS());
        Lattice rhs = lattices.get(i->getRHS());

        // 只要有一个是 Bottom，结果就是 Bottom
        if (lhs.type == Lattice::Bottom || rhs.type == Lattice::Bottom)
            return {Lattice::Bottom};

        // 只要有一个是 Top（还没算出来），结果先保持 Top
        if (lhs.type == Lattice::Top || rhs.type == Lattice::Top)
            return {Lattice::Top};

        // 两个都是 Constant，进行常量折叠
        int32_t l = lhs.constant;
        int32_t r = rhs.constant;
        bool    res;
        // clang-format off
        switch (i->getPredicate()) {
            case CmpInst::ICMP_EQ:  res = (l == r); break;
            case CmpInst::ICMP_NE:  res = (l != r); break;
            case CmpInst::ICMP_SGT: res = (l >  r); break;
            case CmpInst::ICMP_SGE: res = (l >= r); break;
            case CmpInst::ICMP_SLT: res = (l <  r); break;
            case CmpInst::ICMP_SLE: res = (l <= r); break;
            DEFAULT_UNREACHABLE();
        }
        // clang-format on
        return {Lattice::Constant, res ? 1 : 0};
    }

    else if (auto* i = dynamic_cast<CastInst*>(inst)) {
        Lattice src = lattices.get(i->getSrc());

        if (src.type != Lattice::Constant) return src;

        switch (i->getOpcode()) {
            case CastInst::SExt:  // 目前没有 SExt
            case CastInst::ZExt:
                return src;
                DEFAULT_UNREACHABLE();
        }
    }

    else if (auto* i = dynamic_cast<LoadInst*>(inst)) {
        Value* ptr = i->getPointerOperand();
        if (auto* gv = dynamic_cast<GlobalVariable*>(ptr)) {
            // 检查指针是否指向一个已知的全局常量
            if (gv->isConstant() && gv->hasInitializer()) {
                if (auto* init =
                        dynamic_cast<ConstantInt*>(gv->getInitializer())) {
                    return {Lattice::Constant, init->getValue()};
                }
            }
        } else if (auto* gep = dynamic_cast<GetElementPtrInst*>(ptr)) {
            Value* base = gep->getPointerOperand();
            auto*  gv   = dynamic_cast<GlobalVariable*>(base);
            if (gv && gv->isConstant() && gv->hasInitializer()) {
                Constant* init = gv->getInitializer();

                Constant* current = init;
                bool      failed  = false;
                bool      first   = true;
                for (ir::Use& idxOp : gep->indices()) {
                    Lattice lat = lattices.get(idxOp.get());

                    if (lat.type != Lattice::Constant) {
                        failed = true;
                        break;
                    }

                    int32_t idx = lat.constant;

                    // 第 0 个索引通常是针对指针基地址的偏移
                    if (first) {
                        if (idx != 0) {
                            failed = true;
                            break;
                        }
                        continue;
                    }

                    // 后续索引进入数组内部提取元素
                    if (auto* ca = dynamic_cast<ConstantAggregate*>(current)) {
                        if (idx >= 0 &&
                            (uint64_t)idx < ca->getNumAggregateElements()) {
                            current = ca->getAggregateElement(idx);
                        } else {
                            failed = true;
                            break;
                        }
                    } else {
                        failed = true;
                        break;
                    }
                }

                if (!failed) {
                    if (auto* ci = dynamic_cast<ConstantInt*>(current)) {
                        return {Lattice::Constant, ci->getValue()};
                    }
                }
            }
        }
    }

    return {Lattice::Bottom};
}

bool rewriteInstruction(Instruction* inst, LatticeAnalysis& lattices) {
    BasicBlock* bb = inst->parent();

    const Lattice& lattice = lattices[inst];
    if (lattice.type == Lattice::Constant) {
        // 创建一个新的常量对象
        auto* constVal = ConstantInt::get(
            static_cast<IntegerType*>(inst->getType()), lattice.constant);
        inst->replaceAllUsesWith(constVal);
        inst->dropAllReferences();
        bb->erase(inst);
        return true;
    }

    if (auto* i = dynamic_cast<BranchInst*>(inst)) {
        if (i->isConditional()) {
            const Lattice& lattice = lattices.get(i->getCondition());
            if (lattice.type == Lattice::Constant) {
                BasicBlock* target =
                    lattice.constant ? i->getTrueBB() : i->getFalseBB();
                auto* newBr = new BranchInst{target};
                bb->insert(i, newBr);
                i->replaceAllUsesWith(newBr);
                i->dropAllReferences();
                bb->erase(i);
                return true;
            }
        }
    }

    return false;
}

bool ConstantPropagationPass::runOnFunction(Function& f) {
    if (f.empty()) return false;

    LatticeAnalysis          lattices;
    std::queue<Instruction*> workList;
    for (BasicBlock& bb : f) {
        for (Instruction& inst : bb) {
            workList.push(&inst);
        }
    }

    while (!workList.empty()) {
        Instruction* inst = workList.front();
        workList.pop();

        const Lattice& oldLattice = lattices[inst];
        const Lattice& newLattice = evaluate(inst, lattices);

        // 如果 Lattice 状态发生了升级 (None -> Constant -> Bottom)
        if (oldLattice.type != newLattice.type) {
            lattices[inst] = newLattice;

            // Add all user to work list
            for (User& user : inst->users()) {
                if (auto* instUser = dynamic_cast<Instruction*>(&user)) {
                    workList.push(instUser);
                }
            }
        }
    }

    // 替换指令
    bool changed = false;
    for (BasicBlock& bb : f) {
        for (auto it = bb.begin(); it != bb.end();) {
            Instruction& inst = *it++;
            changed |= rewriteInstruction(&inst, lattices);
        }
    }

    return changed;
}

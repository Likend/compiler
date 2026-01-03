#include "opt/SubexprElimination.hpp"

#include <functional>
#include <unordered_set>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"

using namespace opt;
using namespace ir;

// 带有副作用的指令
bool hasSideEffect(Instruction* inst) {
    return dynamic_cast<CallInst*>(inst) || dynamic_cast<StoreInst*>(inst) ||
           dynamic_cast<LoadInst*>(inst);
}

struct InstructionHasher {
    size_t operator()(const Instruction* inst) const {
        std::hash<const void*> hasher;

        size_t h       = 0;
        auto   combine = [&](size_t val) {
            h ^= val + 0x9e3779b9 + (h << 6) + (h >> 2);
        };

        if (const auto* i = dynamic_cast<const BinaryOperator*>(inst)) {
            auto opcode = i->getBinaryOps();
            combine(opcode);
            size_t l = hasher(i->getLHS());
            size_t r = hasher(i->getRHS());
            if (i->isCommutative()) {
                combine(l ^ r);
            } else {
                combine(l);
                combine(r);
            }
        }

        else if (const auto* i = dynamic_cast<const ICmpInst*>(inst)) {
            auto   opcode = i->getPredicate();
            size_t l      = hasher(i->getLHS());
            size_t r      = hasher(i->getRHS());
            switch (opcode) {
                case CmpInst::ICMP_EQ:
                case CmpInst::ICMP_NE:
                    combine(l ^ r);
                    break;
                case CmpInst::ICMP_SGT:
                case CmpInst::ICMP_SGE:
                case CmpInst::ICMP_SLT:
                case CmpInst::ICMP_SLE:
                    combine(l);
                    combine(r);
                    break;
            }
        }

        else if (const auto* i = dynamic_cast<const CastInst*>(inst)) {
            combine(hasher(i->getSrc()));
            combine(hasher(i->getDestTy()));  // 确保转换后的目标类型一致
        }

        else if (const auto* i = dynamic_cast<const GetElementPtrInst*>(inst)) {
            combine(hasher(i->getPointerOperand()));
            for (auto it = i->idx_begin(); it != i->idx_end(); ++it) {
                combine(hasher(it->get()));
            }
        }

        else {
            combine(hasher(inst));
        }

        return h;
    }
};

struct InstructionEqual {
    bool operator()(const Instruction* lhs, const Instruction* rhs) const {
        if (lhs->getType() != rhs->getType()) return false;
        if (lhs->getNumOperands() != rhs->getNumOperands()) return false;

        // 处理二元运算的交换律
        const auto* biL = dynamic_cast<const BinaryOperator*>(lhs);
        const auto* biR = dynamic_cast<const BinaryOperator*>(rhs);
        if (biL && biR) {
            if (biL->getBinaryOps() != biR->getBinaryOps()) return false;
            if (biL->isCommutative())
                return (biL->getLHS() == biR->getLHS() &&
                        biL->getRHS() == biR->getRHS()) ||
                       (biL->getLHS() == biR->getRHS() &&
                        biL->getRHS() == biR->getLHS());
            else
                return (biL->getLHS() == biR->getLHS() &&
                        biL->getRHS() == biR->getRHS());
        }

        // 处理比较运算
        const auto* ciL = dynamic_cast<const ICmpInst*>(lhs);
        const auto* ciR = dynamic_cast<const ICmpInst*>(rhs);
        if (ciL && ciR) {
            if (ciL->getPredicate() != ciR->getPredicate()) return false;
            if (ciL->getPredicate() == CmpInst::ICMP_EQ ||
                ciL->getPredicate() == CmpInst::ICMP_NE)
                return (ciL->getLHS() == ciR->getLHS() &&
                        ciL->getRHS() == ciR->getRHS()) ||
                       (ciL->getLHS() == ciR->getRHS() &&
                        ciL->getRHS() == ciR->getLHS());
            else
                return (ciL->getLHS() == ciR->getLHS() &&
                        ciL->getRHS() == ciR->getRHS());
        }

        const auto* cL = dynamic_cast<const CastInst*>(lhs);
        const auto* cR = dynamic_cast<const CastInst*>(rhs);
        if (cL && cR) {
            return cL->getDestTy() == cR->getDestTy() &&
                   cL->getSrc() == cR->getSrc();
        }

        // 比较 GEP
        const auto* gL = dynamic_cast<const GetElementPtrInst*>(lhs);
        const auto* gR = dynamic_cast<const GetElementPtrInst*>(rhs);
        if (gL && gR) {
            if (gL->getPointerOperand() != gR->getPointerOperand())
                return false;
            if (gL->getNumOperands() != gR->getNumOperands()) return false;
            // 逐个比较索引内容
            auto itL = const_cast<GetElementPtrInst*>(gL)->indices().begin();
            auto itR = const_cast<GetElementPtrInst*>(gR)->indices().begin();
            while (itL != const_cast<GetElementPtrInst*>(gL)->indices().end()) {
                if (itL->get() != itR->get()) return false;
                ++itL;
                ++itR;
            }
            return true;
        }

        return lhs == rhs;
    }
};

bool SubexprEliminationPass::runOnFunction(Function& f) {
    bool changed = false;
    for (BasicBlock& bb : f) {
        std::unordered_set<Instruction*, InstructionHasher, InstructionEqual>
            set;
        for (auto instIt = bb.begin(); instIt != bb.end();) {
            Instruction& inst = *instIt++;
            if (hasSideEffect(&inst)) continue;
            if (auto it = set.find(&inst); it != set.end()) {
                inst.replaceAllUsesWith(*it);
                inst.dropAllReferences();
                bb.erase(inst);
                changed = true;
            } else {
                set.insert(&inst);
            }
        }
    }
    return changed;
}

#include "codegen/pass/IRTranslator.hpp"

#include <cstdint>
#include <cstdio>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "ir/BasicBlock.hpp"
#include "ir/Constants.hpp"
#include "ir/Function.hpp"
#include "ir/GlobalValue.hpp"
#include "ir/Instructions.hpp"
#include "ir/Value.hpp"
#include "util/assert.hpp"

using namespace codegen;

Register IRTranslator::getVReg(const ir::Value* val) {
    ASSERT(val);
    return vmap.at(val);
}

Register IRTranslator::CreateVReg(const ir::Value* val) {
    ASSERT(val);
    auto [it, inserted] = vmap.try_emplace(val);
    ASSERT_WITH(inserted, "Value already assign to vreg.");
    return it->second = currentFunction->CreateVReg();
}

Register IRTranslator::getOrCreateVReg(const ir::Value* val) {
    ASSERT(val);
    auto [it, inserted] = vmap.try_emplace(val);
    if (inserted) it->second = currentFunction->CreateVReg();
    return it->second;
}

void IRTranslator::AssignVReg(const ir::Value* val, Register reg) {
    ASSERT(val);
    auto [it, inserted] = vmap.try_emplace(val);
    ASSERT_WITH(inserted, "Reg already assigned");
    it->second = reg;
}

bool IRTranslator::runOnMachineFunction(MachineFunction& mf) {
    currentFunction = &mf;

    const ir::Function& function = mf.getFunction();

    bbMap.clear();
    for (const ir::BasicBlock& bb : function) {
        MachineBasicBlock& b =
            currentFunction->emplace_back(std::string(bb.getName()));
        bbMap[&bb] = &b;
    }

    for (const ir::BasicBlock& bb : function) {
        translateBasicBlock(&bb);
    }

    return true;
}

void IRTranslator::translateBasicBlock(const ir::BasicBlock* bb) {
    currentBB = bbMap.at(bb);

    for (const ir::Instruction& instr : *bb) {
        translateInstruction(&instr);
    }
}

void IRTranslator::translateInstruction(const ir::Instruction* instr) {
    if (const auto* i = dynamic_cast<const ir::BinaryOperator*>(instr)) {
        translateBinaryOperator(i);
    } else if (const auto* i = dynamic_cast<const ir::ICmpInst*>(instr)) {
        translateICmpInst(i);
    } else if (const auto* i = dynamic_cast<const ir::AllocaInst*>(instr)) {
        translateAllocaInst(i);
    } else if (const auto* i = dynamic_cast<const ir::LoadInst*>(instr)) {
        translateLoadInst(i);
    } else if (const auto* i = dynamic_cast<const ir::StoreInst*>(instr)) {
        translateStoreInst(i);
    } else if (const auto* i =
                   dynamic_cast<const ir::GetElementPtrInst*>(instr)) {
        translateGetElementPtrInst(i);
    } else if (const auto* i = dynamic_cast<const ir::ReturnInst*>(instr)) {
        translateReturnInst(i);
    } else if (const auto* i = dynamic_cast<const ir::BranchInst*>(instr)) {
        translateBranchInst(i);
    } else if (const auto* i = dynamic_cast<const ir::CastInst*>(instr)) {
        translateCastInst(i);
    } else if (const auto* i = dynamic_cast<const ir::CallInst*>(instr)) {
        translateCallInst(i);
    }
}

void IRTranslator::translateBinaryOperator(const ir::BinaryOperator* i) {
    const MachineInstrDesc* desc;
    switch (i->getBinaryOps()) {
        case ir::BinaryOperator::Add:
            desc = &DESC_ADD;
            break;
        case ir::BinaryOperator::Sub:
            desc = &DESC_SUB;
            break;
        case ir::BinaryOperator::Mul:
            desc = &DESC_MUL;
            break;
        case ir::BinaryOperator::SDiv:
            desc = &DESC_SDIV;
            break;
        case ir::BinaryOperator::SRem:
            desc = &DESC_SREM;
            break;
        case ir::BinaryOperator::Xor:
            desc = &DESC_XOR;
            break;
        default:
            UNREACHABLE();
    }
    Register lhs = prepareReg(i->getLHS());
    Register rhs = prepareReg(i->getRHS());
    currentBB->emplace_back(*desc, CreateVReg(i), lhs, rhs);
}

void IRTranslator::translateICmpInst(const ir::ICmpInst* i) {
    const MachineInstrDesc* desc;
    switch (i->getPredicate()) {
        case ir::CmpInst::ICMP_EQ:
            desc = &DESC_SEQ;
            break;
        case ir::CmpInst::ICMP_NE:
            desc = &DESC_SNE;
            break;
        case ir::CmpInst::ICMP_SGT:
            desc = &DESC_SGT;
            break;
        case ir::CmpInst::ICMP_SGE:
            desc = &DESC_SGE;
            break;
        case ir::CmpInst::ICMP_SLT:
            desc = &DESC_SLT;
            break;
        case ir::CmpInst::ICMP_SLE:
            desc = &DESC_SLE;
            break;
    }
    Register lhs = prepareReg(i->getLHS());
    Register rhs = prepareReg(i->getRHS());
    currentBB->emplace_back(*desc, CreateVReg(i), lhs, rhs);
}

void IRTranslator::translateAllocaInst(const ir::AllocaInst* i) {
    auto stackID = static_cast<int64_t>(currentFunction->CreateStackObject(
        dl.getTypeSizeInBits(i->getAllocatedType()), i));
    currentBB->emplace_back(DESC_FRAME, CreateVReg(i),
                            ImmediateOpKind{stackID});
}

void IRTranslator::translateLoadInst(const ir::LoadInst* i) {
    Register ptr = prepareReg(i->getPointerOperand());
    currentBB->emplace_back(DESC_LW, CreateVReg(i), ptr, ImmediateOpKind{0});
}

void IRTranslator::translateStoreInst(const ir::StoreInst* i) {
    Register ptr = prepareReg(i->getPointerOperand());
    Register val = prepareReg(i->getValueOperand());
    currentBB->emplace_back(DESC_SW, val, ptr, ImmediateOpKind{0});
}

void IRTranslator::translateGetElementPtrInst(const ir::GetElementPtrInst* i) {
    Register ptrBase = prepareReg(i->getPointerOperand());

    // Calculate offset
    std::vector<ir::Value*> idxList;
    for (auto idx : i->indices()) {
        idxList.push_back(idx.get());
        ir::Type* ty = ir::GetElementPtrInst::getIndexedType(
            i->getSourceElementType(), idxList);
        size_t size = dl.getTypeSizeInBits(ty) / 8;

        Register idxReg  = prepareReg(idx.get());
        Register multReg = currentFunction->CreateVReg();

        Register tempReg = currentFunction->CreateVReg();
        currentBB->emplace_back(DESC_LI, tempReg,
                                ImmediateOpKind{static_cast<int64_t>(size)});
        currentBB->emplace_back(DESC_MUL, multReg, idxReg, tempReg);

        Register addReg = currentFunction->CreateVReg();
        currentBB->emplace_back(DESC_ADD, addReg, multReg, ptrBase);
        ptrBase = addReg;
    }
    AssignVReg(i, ptrBase);
}

void IRTranslator::translateReturnInst(const ir::ReturnInst* i) {
    if (const ir::Value* retVal = i->getReturnValue()) {
        currentBB->emplace_back(DESC_SET_RET_VAL, prepareReg(retVal));
    }
    currentBB->emplace_back(DESC_RET);
}

void IRTranslator::translateBranchInst(const ir::BranchInst* i) {
    MachineBasicBlock* targetBB;
    if (i->isConditional()) {
        targetBB = bbMap.at(i->getFalseBB());
        currentBB->emplace_back(DESC_BEQZ, prepareReg(i->getCondition()),
                                targetBB);
        currentBB->successors.push_back(targetBB);
        targetBB->predecessors.push_back(currentBB);
    }
    targetBB = bbMap.at(i->getTrueBB());
    currentBB->emplace_back(DESC_JUMP, bbMap.at(i->getTrueBB()));
    currentBB->successors.push_back(targetBB);
    targetBB->predecessors.push_back(currentBB);
}

void IRTranslator::translateCastInst(const ir::CastInst* i) {
    ASSERT_WITH(i->getOpcode() == ir::CastInst::ZExt, "Not support SExt");
    AssignVReg(i, prepareReg(i->getSrc()));
}

void IRTranslator::translateCallInst(const ir::CallInst* i) {
    int64_t count = 0;
    for (auto arg : i->args()) {
        currentBB->emplace_back(DESC_SET_CALL_ARG, prepareReg(arg.get()),
                                ImmediateOpKind{count});
        count++;
    }
    auto* func = dynamic_cast<ir::Function*>(i->getCalledOperand());
    ASSERT(func);
    currentBB->emplace_back(DESC_CALL, GlobalValueOpKind{func, 0});
    if (!i->getType()->isVoidTy()) {
        currentBB->emplace_back(DESC_GET_RET_VAL, CreateVReg(i));
    }
}

Register IRTranslator::prepareReg(const ir::Value* value) {
    if (const auto* arg = dynamic_cast<const ir::Argument*>(value)) {
        Register vreg = getOrCreateVReg(arg);
        currentBB->emplace_back(DESC_GET_CUR_ARG, RegisterOpKind{vreg},
                                ImmediateOpKind{arg->getArgNo()});
        return vreg;
    } else if (const auto* ci = dynamic_cast<const ir::ConstantInt*>(value)) {
        Register vreg = currentFunction->CreateVReg();
        currentBB->emplace_back(DESC_LI, RegisterOpKind{vreg},
                                ImmediateOpKind{ci->getValue()});
        return vreg;
    } else if (const auto* gv =
                   dynamic_cast<const ir::GlobalVariable*>(value)) {
        Register vreg = currentFunction->CreateVReg();
        currentBB->emplace_back(DESC_LA, vreg, GlobalValueOpKind{gv, 0});
        return vreg;
    } else {
        return getVReg(value);
    }
}

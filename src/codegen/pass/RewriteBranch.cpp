#include "codegen/pass/RewriteBranch.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"

using namespace ir;
using namespace codegen;

MachineInstr* writeOnce(MachineInstr* mi) {
    if (mi->getOpcode() != DESC_BEQZ.opcode &&
        mi->getOpcode() != DESC_BNEZ.opcode)
        return nullptr;

    bool reverse = mi->getOpcode() == DESC_BEQZ.opcode;

    MachineBasicBlock& mbb    = *mi->parent();
    MachineFunction&   mf     = *mbb.parent();
    Register           cond   = mi->getOperand(0).getRegister();
    MachineBasicBlock* target = mi->getOperand(1).getMachineBasicBlock();

    RegisterInfo& info = mf.getRegisterInfo(cond);
    if (info.defs() != 1) return nullptr;
    MachineInstr& def = info.defList[0]->getParent();

    Register      lhs;
    Register      rhs;
    MachineInstr* newInst;
    switch (def.getOpcode()) {
        case DESC_SNE.opcode:
            lhs = def.getOperand(1).getRegister();
            rhs = def.getOperand(2).getRegister();
            if (lhs == REG_ZERO || rhs == REG_ZERO) {
                Register newCond = lhs == REG_ZERO ? rhs : lhs;
                newInst = &*mbb.emplace(mi, reverse ? DESC_BEQZ : DESC_BNEZ,
                                        newCond, target);
                break;
            }
            newInst = &*mbb.emplace(mi, reverse ? DESC_BEQ : DESC_BNE, lhs, rhs,
                                    target);
            break;

        case DESC_SEQ.opcode:
            lhs = def.getOperand(1).getRegister();
            rhs = def.getOperand(2).getRegister();
            if (lhs == REG_ZERO || rhs == REG_ZERO) {
                Register newCond = lhs == REG_ZERO ? rhs : lhs;
                newInst = &*mbb.emplace(mi, reverse ? DESC_BNEZ : DESC_BEQZ,
                                        newCond, target);
                break;
            }
            newInst = &*mbb.emplace(mi, reverse ? DESC_BNE : DESC_BEQ, lhs, rhs,
                                    target);
            break;

        case DESC_SGT.opcode: {
            lhs = def.getOperand(1).getRegister();
            rhs = def.getOperand(2).getRegister();
            if (lhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BGEZ : DESC_BLTZ, rhs, target);
                mbb.erase(mi);
                return &newInst;
            } else if (rhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BLEZ : DESC_BGTZ, lhs, target);
                mbb.erase(mi);
                return &newInst;
            } else {
                return nullptr;
            }
        }

        case DESC_SLT.opcode: {
            lhs = def.getOperand(1).getRegister();
            rhs = def.getOperand(2).getRegister();
            if (lhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BLEZ : DESC_BGTZ, rhs, target);
                mbb.erase(mi);
                return &newInst;
            } else if (rhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BGEZ : DESC_BLTZ, lhs, target);
                mbb.erase(mi);
                return &newInst;
            } else {
                return nullptr;
            }
        }

        case DESC_SGE.opcode: {
            lhs = def.getOperand(1).getRegister();
            rhs = def.getOperand(2).getRegister();
            if (lhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BGTZ : DESC_BLEZ, rhs, target);
                mbb.erase(mi);
                return &newInst;
            } else if (rhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BLTZ : DESC_BGEZ, lhs, target);
                mbb.erase(mi);
                return &newInst;
            } else {
                return nullptr;
            }
        }

        case DESC_SLE.opcode: {
            lhs = def.getOperand(1).getRegister();
            rhs = def.getOperand(2).getRegister();
            if (lhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BLTZ : DESC_BGEZ, rhs, target);
                mbb.erase(mi);
                return &newInst;
            } else if (rhs == REG_ZERO) {
                auto& newInst = *mbb.emplace(
                    mi, reverse ? DESC_BGTZ : DESC_BLEZ, lhs, target);
                mbb.erase(mi);
                return &newInst;
            } else {
                return nullptr;
            }
        }

        default:
            return nullptr;
    }

    mbb.erase(mi);
    return newInst;
}

bool RewriteBranchPass::runOnMachineFunction(MachineFunction& mf) {
    bool changed = false;
    for (MachineBasicBlock& mbb : mf) {
        for (auto miIt = mbb.begin(); miIt != mbb.end();) {
            MachineInstr& mi  = *miIt++;
            MachineInstr* cur = &mi;
            do {
                cur = writeOnce(cur);
                changed |= (cur != nullptr);
            } while (cur != nullptr);
        }
    }
    return changed;
}

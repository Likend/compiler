#include "codegen/pass/ReplaceRegister.hpp"

#include <map>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/pass/LinerScanRegisterAlloc.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"
#include "util/assert.hpp"

using namespace codegen;

bool ReplaceRegisterPass::runOnMachineFunction(MachineFunction& mf) {
    auto                  allocation = allocationAnalysis->at(&mf);
    std::vector<Register> regs;
    regs.reserve(mf.regInfos.size());
    for (auto& [reg, regInfo] : mf.regInfos) {
        if (reg.isVirtual()) regs.push_back(reg);
    }
    std::map<Register, int64_t> stackRegObjectId;
    for (Register sourceVReg : regs) {
        auto&    info      = mf.regInfos.at(sourceVReg);
        Register targetReg = allocation.getAssignedReg(sourceVReg);
        if (targetReg.isPhysical()) {
            RegisterInfo& targetInfo = mf.regInfos[targetReg];
            for (MachineOperand* def : info.def()) {
                def->ChangeTo({RegisterOpKind{targetReg}});
                targetInfo.addDef(*def);
            }
            for (MachineOperand* use : info.use()) {
                use->ChangeTo({RegisterOpKind{targetReg}});
                targetInfo.addUse(*use);
            }
        } else if (targetReg.isStack()) {
            stackRegObjectId[sourceVReg] =
                static_cast<int64_t>(mf.CreateStackObject(32));
        } else {
            UNREACHABLE();
        }
        mf.regInfos.erase(sourceVReg);
    }

    for (MachineBasicBlock& mbb : mf) {
        for (auto it = mbb.begin(); it != mbb.end();) {
            MachineInstr& mi = *it;
            for (MachineOperand& op : mi.explicit_uses()) {
                if (auto stackObj = stackRegObjectId.find(op.getRegister());
                    stackObj != stackRegObjectId.end()) {
                    size_t   use_id  = &op - mi.explicit_uses().begin();
                    Register tempReg = allocReg(use_id);

                    op.ChangeTo({RegisterOpKind{tempReg}});
                    mf.regInfos[tempReg].addUse(op);
                    IntrusiveIterator<MachineInstr> add{new MachineInstr{
                        DESC_LOAD_FRAME, tempReg,
                        ImmediateOpKind{stackObj->second}, ImmediateOpKind{0}}};
                    add.insert_into(it);
                }
            }
            ++it;
            for (MachineOperand& op : mi.explicit_defs()) {
                if (auto stackObj = stackRegObjectId.find(op.getRegister());
                    stackObj != stackRegObjectId.end()) {
                    size_t   def_id  = &op - mi.explicit_defs().begin();
                    Register tempReg = allocReg(def_id);

                    op.ChangeTo({RegisterOpKind{tempReg}});
                    mf.regInfos[tempReg].addDef(op);
                    IntrusiveIterator<MachineInstr> add{new MachineInstr{
                        DESC_STORE_FRAME, tempReg,
                        ImmediateOpKind{stackObj->second}, ImmediateOpKind{0}}};
                    add.insert_into(it);
                }
            }
        }
    }

    return true;
}

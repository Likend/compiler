#include "codegen/ReplaceRegister.hpp"

#include <algorithm>
#include <iterator>
#include <map>

#include "codegen/LinerScanRegisterAlloc.hpp"
#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "Register.hpp"
#include "RegisterInfo.hpp"

using namespace codegen;

void ReplaceRegisterPass::runOnMachineFunction(MachineFunction& mf) {
    auto                  allocation = allocationAnalysis->at(&mf);
    std::vector<Register> regs;
    regs.reserve(mf.regInfos.size());
    std::transform(mf.regInfos.begin(), mf.regInfos.end(),
                   std::back_inserter(regs),
                   [](const auto& p) { return p.first; });
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

        } else {
            stackRegObjectId[sourceVReg] =
                static_cast<int64_t>(mf.CreateStackObject(32));
        }
        mf.regInfos.erase(sourceVReg);
    }

    for (MachineBasicBlock& mbb : mf) {
        for (auto it = mbb.begin(); it != mbb.end();) {
            MachineInstr& mi = *it;
            for (MachineOperand& op : mi.explicit_uses()) {
                if (auto stackObj = stackRegObjectId.find(op.getRegister());
                    stackObj != stackRegObjectId.end()) {
                    size_t        use_id  = &op - mi.explicit_uses().begin();
                    MachineInstr& instr   = op.getParent();
                    Register      tempReg = allocReg(use_id);

                    op.ChangeTo({RegisterOpKind{tempReg}});
                    mf.regInfos[tempReg].addUse(op);

                    instr.getParent().insert(it, DESC_LOAD_FRAME, tempReg,
                                             ImmediateOpKind{stackObj->second});
                }
            }
            ++it;
            for (MachineOperand& op : mi.explicit_defs()) {
                if (auto stackObj = stackRegObjectId.find(op.getRegister());
                    stackObj != stackRegObjectId.end()) {
                    size_t        def_id  = &op - mi.explicit_defs().begin();
                    MachineInstr& instr   = op.getParent();
                    Register      tempReg = allocReg(def_id);

                    op.ChangeTo({RegisterOpKind{tempReg}});
                    mf.regInfos[tempReg].addDef(op);

                    instr.getParent().insert(it, DESC_STORE_FRAME, tempReg,
                                             ImmediateOpKind{stackObj->second});
                }
            }
        }
    }
}

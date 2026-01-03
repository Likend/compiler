#include "codegen/pass/ReplaceRegister.hpp"

#include <algorithm>
#include <iterator>
#include <set>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/pass/LinerScanRegisterAlloc.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"
#include "ir/Constants.hpp"
#include "ir/GlobalValue.hpp"
#include "util/assert.hpp"
#include "util/IntrusiveList.hpp"

using namespace codegen;

void ReplaceRegisterPass::ReplacePhysic(MachineFunction& mf, RegisterInfo& info,
                                        Register targetReg) {
    RegisterInfo& targetInfo = mf.regInfos[targetReg];
    for (MachineOperand* def : info.def()) {
        def->ChangeTo({RegisterOpKind{targetReg}});
        targetInfo.addDef(*def);
    }
    for (MachineOperand* use : info.use()) {
        use->ChangeTo({RegisterOpKind{targetReg}});
        targetInfo.addUse(*use);
    }
}

void ReplaceRegisterPass::ReplaceStack(MachineFunction& mf,
                                       RegisterInfo&    info) {
    if (info.defs() == 1) {
        MachineInstr& def = (*info.def_begin())->getParent();
        if (def.getOpcode() == DESC_LI.opcode) {
            int64_t imm = def.getOperand(1).getImmediate();

            for (MachineOperand* useOp : info.use()) {
                MachineInstr& use     = useOp->getParent();
                size_t        use_id  = useOp - use.explicit_uses().begin();
                Register      tempReg = allocReg(use_id);
                useOp->ChangeTo({RegisterOpKind{tempReg}});
                use.parent()->emplace(use, DESC_LI, tempReg,
                                      ImmediateOpKind{imm});
                mf.regInfos[tempReg].addUse(*useOp);
            }

            def.parent()->erase(def);
            return;
        } else if (def.getOpcode() == DESC_LA.opcode) {
            const ir::GlobalValue* gv = def.getOperand(1).getGlobalValue();

            for (MachineOperand* useOp : info.use()) {
                MachineInstr& use     = useOp->getParent();
                size_t        use_id  = useOp - use.explicit_uses().begin();
                Register      tempReg = allocReg(use_id);
                useOp->ChangeTo({RegisterOpKind{tempReg}});
                use.parent()->emplace(use, DESC_LA, tempReg,
                                      GlobalValueOpKind{gv, 0});
                mf.regInfos[tempReg].addUse(*useOp);
            }

            def.parent()->erase(def);
            return;
        }
    }

    auto id = static_cast<int64_t>(mf.CreateStackObject(32));

    // std::vector<MachineOperand*> collects;
    // std::copy(info.use_begin(), info.use_end(),
    // std::back_inserter(collects));
    for (MachineOperand* useOp : info.use()) {
        MachineInstr& use     = useOp->getParent();
        size_t        use_id  = useOp - use.explicit_uses().begin();
        Register      tempReg = allocReg(use_id);
        useOp->ChangeTo({RegisterOpKind{tempReg}});
        mf.regInfos[tempReg].addUse(*useOp);
        use.parent()->emplace(use, DESC_LOAD_FRAME, tempReg,
                              ImmediateOpKind{id}, ImmediateOpKind{0});
    }

    // collects.clear();
    // std::copy(info.def_begin(), info.def_end(),
    // std::back_inserter(collects));
    for (MachineOperand* defOp : info.def()) {
        MachineInstr& def     = defOp->getParent();
        size_t        def_id  = defOp - def.explicit_defs().begin();
        Register      tempReg = allocReg(def_id);
        defOp->ChangeTo({RegisterOpKind{tempReg}});
        mf.regInfos[tempReg].addDef(*defOp);
        def.parent()->emplace(++IntrusiveIterator{def}, DESC_STORE_FRAME,
                              tempReg, ImmediateOpKind{id}, ImmediateOpKind{0});
    }
}

bool ReplaceRegisterPass::runOnMachineFunction(MachineFunction& mf) {
    auto                  allocation = allocationAnalysis->at(&mf);
    std::vector<Register> regs;
    regs.reserve(mf.regInfos.size());
    for (auto& [reg, regInfo] : mf.regInfos) {
        if (reg.isVirtual()) regs.push_back(reg);
    }

    for (Register sourceVReg : regs) {
        RegisterInfo& info      = mf.regInfos.at(sourceVReg);
        Register      targetReg = allocation.getAssignedReg(sourceVReg);
        if (targetReg.isPhysical()) {
            ReplacePhysic(mf, info, targetReg);
        } else if (targetReg.isStack()) {
            ReplaceStack(mf, info);
        } else {
            UNREACHABLE();
        }
        mf.regInfos.erase(sourceVReg);
    }
    return true;
}

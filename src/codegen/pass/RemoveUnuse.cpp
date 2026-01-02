#include "codegen/pass/RemoveUnuse.hpp"

#include <algorithm>
#include <set>

#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"

using namespace codegen;

bool RemoveUnusePass::runOnMachineFunction(MachineFunction& mf) {
    std::set<Register> worklist;
    for (auto& [reg, regInfo] : mf.regInfos) {
        worklist.insert(reg);
    }

    bool changed = false;

    while (!worklist.empty()) {
        auto     it      = worklist.begin();
        Register currReg = *it;

        worklist.erase(it);

        if (mf.regInfos.find(currReg) == mf.regInfos.end()) continue;
        const RegisterInfo& regInfo = mf.regInfos.at(currReg);

        if (!currReg.isVirtual()) continue;

        std::set<MachineInstr*> needToRemove;
        if (regInfo.use_empty()) {
            // 这里所有有副作用的指令都没有 def 寄存器，可以放心删除
            for (MachineOperand* def : regInfo.def()) {
                MachineInstr& defInst = def->getParent();

                bool allEmpty = std::all_of(
                    defInst.explicit_defs().begin(),
                    defInst.explicit_defs().end(), [&mf](MachineOperand& def) {
                        return mf.getRegisterInfo(def.getRegister())
                            .use_empty();
                    });
                if (!allEmpty) continue;

                needToRemove.insert(&defInst);

                for (MachineOperand& use : defInst.explicit_uses()) {
                    Register reg = use.getRegister();
                    worklist.insert(reg);
                }
            }
        }

        changed |= !needToRemove.empty();
        for (MachineInstr* mi : needToRemove) {
            mi->parent()->erase(mi);
        }
    }

    return changed;
}

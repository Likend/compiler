#include "codegen/pass/MovePropagation.hpp"

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"

using namespace codegen;

bool MovePropagationPass::runOnMachineFunction(MachineFunction& mf) {
    bool changed = false;

    // Key: 待替换的寄存器 (dest), Value: 替换后的源 (src 或 Constant)
    std::unordered_map<Register, Register> propagationMap;
    std::vector<MachineInstr*>             moveInsts;

    for (MachineBasicBlock& mbb : mf) {
        for (MachineInstr& mi : mbb) {
            // 情况 A: 标准寄存器拷贝 %dest = MOVE %src
            if (mi.getOpcode() == DESC_MOVE.opcode) {
                Register dest = mi.getOperand(0).getRegister();
                Register src  = mi.getOperand(1).getRegister();

                if (dest.isVirtual() && dest != src) {
                    propagationMap[dest] = src;
                    moveInsts.push_back(&mi);
                }
            }
            // 情况 B: 常量载入 %dest = LI Constant (集成常量传播)
            else if (mi.getOpcode() == DESC_LI.opcode) {
                Register dest = mi.getOperand(0).getRegister();
                if (dest.isVirtual() && mi.getOperand(1).getImmediate() == 0) {
                    propagationMap[dest] = REG_ZERO;
                    moveInsts.push_back(&mi);
                }
            }
        }
    }

    // 级联收缩（Folding）：处理 %a = %b; %b = %c 这种情况，使 %a 直接指向 %c
    for (auto& [dest, src] : propagationMap) {
        Register finalSrc = src;
        while (propagationMap.count(finalSrc)) {
            finalSrc = propagationMap[finalSrc];
        }
        src = finalSrc;
    }

    //  执行替换
    for (auto& [dest, src] : propagationMap) {
        RegisterInfo& regInfo = mf.getRegisterInfo(dest);

        // 关键：先拷贝 Use 列表，防止 changeOperand 导致迭代器失效
        std::vector<MachineOperand*> usesToUpdate;
        for (MachineOperand* u : regInfo.use()) {
            usesToUpdate.push_back(u);
        }

        for (MachineOperand* useOp : usesToUpdate) {
            MachineInstr& useInst = useOp->getParent();
            useInst.changeOperand(*useOp, {src});
            changed = true;
        }
    }

    // 清理：删除无用的指令
    for (MachineInstr* mi : moveInsts) {
        Register dest = mi->getOperand(0).getRegister();
        if (mf.getRegisterInfo(dest).use_empty()) {
            mi->parent()->erase(mi);
        }
    }

    return changed;
}

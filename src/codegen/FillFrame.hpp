#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"

namespace codegen {
using namespace std::string_literals;

class FillFramePass final : public MachineFunctionPass {
    bool runOnMachineFunction(MachineFunction& mf) override;

    std::optional<MachineBasicBlock::iterator> findFirstInstr(
        MachineFunction& mf) {
        for (MachineBasicBlock& mbb : mf) {
            if (!mbb.empty()) return mbb.begin();
        }
        return std::nullopt;
    }

    Register sp = REG_SP;
    Register v0 = REG_V0;
    Register ra = REG_RA;

    int64_t frameSize;

    // void checkRegInfo(MachineFunction& mf) {
    //     for (auto& mbb : mf) {
    //         for (auto& mi : mbb) {
    //             for (auto& op : mi.explicit_defs()) {
    //                 auto& info = mf.getRegisterInfo(op.getRegister());
    //                 auto  it   = std::find(info.defList.begin(),
    //                                        info.defList.end(), &op);
    //                 if (it == info.defList.end()) UNREACHABLE();
    //             }
    //             for (auto& op : mi.explicit_uses()) {
    //                 auto& info = mf.getRegisterInfo(op.getRegister());
    //                 auto  it   = std::find(info.useList.begin(),
    //                                        info.useList.end(), &op);
    //                 if (it == info.useList.end()) UNREACHABLE();
    //             }
    //         }
    //     }
    // }
};
}  // namespace codegen

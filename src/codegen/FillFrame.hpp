#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "util/assert.hpp"

namespace codegen {
using namespace std::string_literals;

class FillFramePass final : public MachineFunctionPass {
    void runOnMachineFunction(MachineFunction& mf) override {
        std::map<Register, int64_t> regToStack;

        auto find = findFirstInstr(mf);
        if (!find.has_value()) return;  // empty function

        auto               firstInstr = *find;
        MachineBasicBlock& firstBB    = firstInstr->getParent();

        MachineBasicBlock& lastBB =
            *mf.basicBlocks
                 .emplace_back(std::make_unique<MachineBasicBlock>(mf))
                 .get();
        lastBB.name    = "last";
        auto lastInstr = lastBB.end();

        // 调用者保存所有寄存器
        for (const auto& [reg, _] : mf.regInfos) {
            auto i          = static_cast<int64_t>(mf.CreateStackObject(32));
            regToStack[reg] = i;
            firstBB.insert(firstInstr, DESC_STORE_FRAME, reg,
                           ImmediateOpKind{i});
            lastBB.insert(lastInstr, DESC_LOAD_FRAME, reg, ImmediateOpKind{i});
        }

        // Replace ret to br
        for (MachineBasicBlock& mbb : mf) {
            for (auto it = mbb.begin(); it != mbb.end();) {
                MachineInstr& mi = *it;
                if (mi.desc.opcode == DESC_RET.opcode) {
                    // change to DESC_JUMP
                    mbb.insert(it, DESC_JUMP, MachineBBOpKind{&lastBB});
                    it = mbb.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Frame size in bits
        frameSize = 0;
        for (StackObject& stack : mf.stackObjects) {
            frameSize += static_cast<int64_t>(stack.size);
        }
        ASSERT(frameSize % 32 == 0);
        frameSize /= 8;  // in bytes
        // Find call args
        int64_t maxArgIndex = -1;
        for (MachineBasicBlock& mbb : mf) {
            for (MachineInstr& mi : mbb) {
                if (mi.desc.opcode == DESC_SET_CALL_ARG.opcode) {
                    int64_t argIndex = mi.getOperand(1).getImmediate();
                    if (argIndex > maxArgIndex) {
                        maxArgIndex = argIndex;
                    }
                }
            }
        }
        int64_t argCount = maxArgIndex + 1;
        frameSize += 4 * argCount;
        frameSize += 4;  // $ra

        firstInstr = firstBB.begin();
        firstBB.insert(firstInstr, DESC_ADDI, sp, sp,
                       ImmediateOpKind{-frameSize});
        firstBB.insert(firstInstr, DESC_SW, ra, sp,
                       ImmediateOpKind{frameSize - 4});
        lastBB.insert(lastInstr, DESC_LW, ra, sp,
                      ImmediateOpKind{frameSize - 4});
        lastBB.insert(lastInstr, DESC_ADDI, sp, sp, ImmediateOpKind{frameSize});

        // Frame
        std::unordered_map<size_t, int64_t> indexToOffset;
        int64_t                             offset = 4 * argCount;
        for (StackObject& stack : mf.stackObjects) {
            indexToOffset[stack.stackID] = offset;
            offset += static_cast<int64_t>(stack.size) / 8;
        }

        // Replace
        for (MachineBasicBlock& mbb : mf) {
            for (auto it = mbb.begin(); it != mbb.end();) {
                MachineInstr& mi = *it;
                switch (mi.desc.opcode) {
                    case DESC_FRAME.opcode: {
                        int64_t     stackIdx = mi.getOperand(1).getImmediate();
                        int64_t     currentOffset = indexToOffset.at(stackIdx);
                        std::string annotation =
                            "Frame"s + std::to_string(currentOffset);
                        mbb.insert(it, DESC_ADDI,
                                   mi.getOperand(0).getRegister(), sp,
                                   ImmediateOpKind{currentOffset})
                            ->addAnnotation(annotation);
                        it = mbb.erase(it);
                        break;
                    }
                    case DESC_LOAD_FRAME.opcode: {
                        int64_t     stackIdx = mi.getOperand(1).getImmediate();
                        int64_t     currentOffset = indexToOffset.at(stackIdx);
                        std::string annotation =
                            "Frame"s + std::to_string(currentOffset);
                        mbb.insert(it, DESC_LW, mi.getOperand(0).getRegister(),
                                   sp, ImmediateOpKind{currentOffset})
                            ->addAnnotation(annotation);
                        it = mbb.erase(it);
                        break;
                    }
                    case DESC_STORE_FRAME.opcode: {
                        int64_t     stackIdx = mi.getOperand(1).getImmediate();
                        int64_t     currentOffset = indexToOffset.at(stackIdx);
                        std::string annotation =
                            "Frame"s + std::to_string(currentOffset);
                        mbb.insert(it, DESC_SW, mi.getOperand(0).getRegister(),
                                   sp, ImmediateOpKind{currentOffset})
                            ->addAnnotation(annotation);
                        it = mbb.erase(it);
                        break;
                    }
                    case DESC_GET_RET_VAL.opcode: {
                        mbb.insert(it, DESC_MOVE,
                                   mi.getOperand(0).getRegister(), v0);
                        it = mbb.erase(it);
                        break;
                    }
                    case DESC_SET_RET_VAL.opcode: {
                        mbb.insert(it, DESC_MOVE, v0,
                                   mi.getOperand(0).getRegister());
                        it = mbb.erase(it);
                        break;
                    }
                    case DESC_GET_CUR_ARG.opcode: {
                        int64_t     argIndex = mi.getOperand(1).getImmediate();
                        int64_t     currentOffset = frameSize + argIndex * 4;
                        std::string annotation =
                            "GetArg"s + std::to_string(argIndex);
                        mbb.insert(it, DESC_LW, mi.getOperand(0).getRegister(),
                                   sp, ImmediateOpKind{currentOffset})
                            ->addAnnotation(annotation);
                        it = mbb.erase(it);
                        break;
                    }
                    case DESC_SET_CALL_ARG.opcode: {
                        int64_t     argIndex = mi.getOperand(1).getImmediate();
                        int64_t     currentOffset = argIndex * 4;
                        std::string annotation =
                            "SetArg"s + std::to_string(argIndex);
                        mbb.insert(it, DESC_SW, mi.getOperand(0).getRegister(),
                                   sp, ImmediateOpKind{currentOffset})
                            ->addAnnotation(annotation);
                        it = mbb.erase(it);
                        break;
                    }
                    default:
                        ++it;
                }
            }
        }

        lastBB.insert(lastInstr, DESC_RET);
    }

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

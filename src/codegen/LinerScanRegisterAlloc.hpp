#pragma once

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "codegen/MachineFunctionPass.hpp"
#include "codegen/Register.hpp"
#include "ir/Module.hpp"
#include "util/assert.hpp"
#include "util/BitSet.hpp"

namespace codegen {
class MachineFunction;
class MachineBasicBlock;
class MachineInstr;

struct RegisterAllocation : std::unordered_map<Register, Register> {
    Register getAssignedReg(Register vreg) const { return at(vreg); }
    void     AssignReg(Register vreg, Register reg) {
        auto [it, inserted] = try_emplace(vreg, reg);
        ASSERT_WITH(inserted,
                        "Virtual register is already be assigned to a  register.");
    }
    void UnassignReg(Register vreg) { erase(vreg); }
};

struct RegisterAllocationAnalysis
    : std::unordered_map<MachineFunction*, RegisterAllocation> {};

class PhysicalRegPool {
    // std::bitset<TOTAL_REG_NUM> available;
    // std::set<uint32_t>         freePool;
    // std::bitset<TOTAL_REG_NUM> active;
    BitSet<32> availableMask;  // 32-bit mask
    BitSet<32> activeMask;     // 32-bit mask

    BitSet<32> freeMask() const { return availableMask & (~activeMask); }

   public:
    constexpr static size_t TOTAL_REG_NUM = 32;

    explicit PhysicalRegPool(std::initializer_list<Register> availableRegs) {
        for (auto reg : availableRegs) {
            ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
            availableMask.set(reg.index());
        }
    }

    std::optional<Register> allocate() {
        // 计算当前既在配置中、又没有被占用的寄存器
        BitSet<32> free = freeMask();

        size_t reg = free.find_first();
        if (reg != static_cast<size_t>(-1)) {
            Register ret{Register::PhysicalRegister,
                         static_cast<uint32_t>(reg)};
            activate(ret);
            return ret;
        } else {
            return std::nullopt;
        }
    }

    void activate(Register reg) {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        ASSERT(isFree(reg));
        activeMask.set(reg.index());
    }

    void release(Register reg) {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        activeMask.reset(reg.index());
    }

    void reset() { activeMask.reset(); }

    bool isActive(Register reg) const {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        return activeMask[reg.index()];
    }

    bool isFree(Register reg) const { return !isActive(reg); }

    bool isAvailable(Register reg) const {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        return availableMask.test(reg.index());
    }

    std::vector<Register> getAllPhysRegs() const {
        std::vector<Register> regs;
        regs.reserve(TOTAL_REG_NUM);
        auto temp = availableMask;

        size_t regIdx;
        while ((regIdx = temp.find_first()) != static_cast<size_t>(-1)) {
            regs.emplace_back(regIdx);  // 假设 Register(int) 可用
            temp.reset(regIdx);         // 清除已处理的位
        }
        return regs;
    }
};

class LinerScanRegisterAllocPass final : public MachineFunctionPass {
   public:
    LinerScanRegisterAllocPass(std::initializer_list<Register> availableRegs)
        : physRegPool(availableRegs) {}

   private:
    void runOnMachineFunction(MachineFunction&) override;
    void doFinalization(ir::Module&) override {
        setAnalysis<RegisterAllocationAnalysis>(std::move(allocationAnalysis));
    }

    void computeGlobalLiveness();      // out: livenesses
    void computeGlobalIntervals();     // out: intervals, rely on: livenesses
    void computeGlobalInstrIndexes();  // out: instrIndexes

    /// Convert virtual register to phisycal register
    Register getPhysicalReg(Register vreg) const {
        Register ret = allocation.getAssignedReg(vreg);
        ASSERT_WITH(ret.isPhysical(),
                    "vreg is not assigned to physical register.");
        return ret;
    }
    void AssignPhysicalReg(Register vreg, Register preg) {
        ASSERT_WITH(preg.isPhysical(), "preg is not physical register.");
        allocation.AssignReg(vreg, preg);
    }
    void UnassignPhysicalReg(Register vreg) {
        ASSERT_WITH(allocation.getAssignedReg(vreg).isPhysical(),
                    "vreg is not assigned to physical register.");
        allocation.UnassignReg(vreg);
    }
    void AssignStackReg(Register vreg) {
        Register sreg{Register::StackRegister, vreg.index()};
        allocation.AssignReg(vreg, sreg);
    }

    MachineFunction*           curFunc;
    PhysicalRegPool            physRegPool;
    RegisterAllocation         allocation;
    RegisterAllocationAnalysis allocationAnalysis;

    struct LivenessAnalysisItem {
        using set = std::unordered_set<Register>;
        set in;
        set out;
        set def;
        set use;
    };

    std::unordered_map<MachineBasicBlock*, LivenessAnalysisItem> livenesses;

    // index table for all instructions
    std::unordered_map<MachineInstr*, size_t> instrIndexes;

    struct Interval {
        Register vreg;
        size_t   start = std::numeric_limits<size_t>::max();
        size_t   end   = 0;
    };
    // Interval for virtual registers
    std::unordered_map<Register, Interval> intervals;
};
}  // namespace codegen

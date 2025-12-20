#pragma once

#include <initializer_list>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "codegen/MachineFunctionPass.hpp"
#include "codegen/Register.hpp"
#include "ir/Module.hpp"
#include "util/assert.hpp"

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
    uint32_t availableMask = 0;  // 32-bit mask
    uint32_t activeMask    = 0;  // 32-bit mask
    // TODO: 封装成 bitset

    // 返回输入数二进制表示从最低位开始(右起)的连续的0的个数
    int findFirstSet(uint32_t mask) const {
        if (mask == 0) return -1;
#ifdef _MSC_VER
        unsigned long index;
        _BitScanForward(&index, mask);
        return static_cast<int>(index);
#else
        return __builtin_ctz(mask);
#endif
    }

    uint32_t freeMask() const { return availableMask & (~activeMask); }

   public:
    constexpr static size_t TOTAL_REG_NUM = 32;

    explicit PhysicalRegPool(std::initializer_list<Register> availableRegs) {
        for (auto reg : availableRegs) {
            ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
            availableMask |= (1U << reg.index());
        }
    }

    std::optional<Register> allocate() {
        // 计算当前既在配置中、又没有被占用的寄存器
        uint32_t free = freeMask();

        int reg = findFirstSet(free);
        if (reg != -1) {
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
        activeMask |= (1U << reg.index());
    }

    void release(Register reg) {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        activeMask &= ~(1U << reg.index());
    }

    void reset() { activeMask = 0; }

    bool isActive(Register reg) const {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        return (activeMask & (1U << reg.index())) != 0;
    }

    bool isFree(Register reg) const {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        return (freeMask() & (1U << reg.index())) != 0;
    }

    bool isAvailable(Register reg) const {
        ASSERT(reg.isPhysical() && reg.index() < TOTAL_REG_NUM);
        // if (!reg.isPhysical()) return false;
        // if (reg.innerVal() >= TOTAL_REG_NUM) return false;
        return (availableMask & (1U << reg.index())) != 0;
    }

    std::vector<Register> getAllPhysRegs() const {
        std::vector<Register> regs;
        regs.reserve(TOTAL_REG_NUM);
        uint32_t temp = availableMask;

        int regIdx;
        while ((regIdx = findFirstSet(temp)) != -1) {
            regs.emplace_back(regIdx);  // 假设 Register(int) 可用
            temp &= ~(1U << regIdx);    // 清除已处理的位
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

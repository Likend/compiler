#include "codegen/pass/LinerScanRegisterAlloc.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineFunction.hpp"
#include "codegen/MachineFunctionPass.hpp"
#include "codegen/MachineInstr.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"

using namespace codegen;

bool LinerScanRegisterAllocPass::runOnMachineFunction(MachineFunction& mf) {
    this->curFunc = &mf;
    allocation.clear();
    physRegPool.reset();

    computeGlobalLiveness();
    computeGlobalInstrIndexes();
    computeGlobalIntervals();

    // 将所有待分配的虚拟寄存器放入一个列表，按 start 升序排列
    std::vector<Interval> unhandled;
    unhandled.reserve(intervals.size());
    std::transform(intervals.begin(), intervals.end(),
                   std::back_inserter(unhandled),
                   [](const auto& p) { return p.second; });
    std::sort(unhandled.begin(), unhandled.end(),
              [&](Interval a, Interval b) { return a.start < b.start; });

    auto activeComparator = [](Interval a, Interval b) {
        if (a.end != b.end)
            return a.end < b.end;
        else
            return a.vreg.index() < b.vreg.index();
    };
    std::set<Interval, decltype(activeComparator)> active{activeComparator};

    for (const Interval& cur : unhandled) {
        // expire old intervals
        while (!active.empty()) {
            auto it = active.begin();
            if ((*it).end >= cur.start) break;
            physRegPool.release(getPhysicalReg(it->vreg));
            active.erase(it);
        }

        if (auto preg = physRegPool.allocate()) {
            AssignPhysicalReg(cur.vreg, *preg);
            active.insert(cur);
        } else {
            // spill at interval
            auto spill = std::prev(active.end());
            if (spill->end > cur.end) {
                Register targetPReg = getPhysicalReg(spill->vreg);
                UnassignPhysicalReg(spill->vreg);
                AssignStackReg(spill->vreg);
                AssignPhysicalReg(cur.vreg, targetPReg);
                active.erase(spill);
                active.insert(cur);
            } else {
                AssignStackReg(cur.vreg);
            }
        }
    }

    allocationAnalysis.emplace(curFunc, std::move(allocation));
    return false;
}

void LinerScanRegisterAllocPass::computeGlobalLiveness() {
    // calc def, use; init in
    for (MachineBasicBlock& mbb : *curFunc) {
        LivenessAnalysisItem& cur = livenesses[&mbb];

        for (MachineInstr& instr : mbb) {
            for (MachineOperand& op : instr.explicit_uses()) {
                Register reg = op.getRegister();
                if (cur.def.find(reg) == cur.def.end()) cur.use.insert(reg);
            }
            for (MachineOperand& op : instr.explicit_defs()) {
                Register reg = op.getRegister();
                if (cur.use.find(reg) == cur.use.end()) cur.def.insert(reg);
            }
        }

        cur.in = cur.use;
    }
    bool modified;
    do {
        modified = false;
        for (auto it = curFunc->rbegin(); it != curFunc->rend(); ++it) {
            MachineBasicBlock&    mbb = *it;
            LivenessAnalysisItem& cur = livenesses[&mbb];
            // calc out
            size_t prevOutSize = cur.out.size();
            for (MachineBasicBlock* suc : mbb.successors) {
                auto& sucIn = livenesses[suc].in;
                cur.out.insert(sucIn.begin(), sucIn.end());
            }
            size_t updateOutSize = cur.out.size();
            if (updateOutSize != prevOutSize) {
                // calc in
                size_t prevInSize = cur.in.size();
                // 差集 cur.out - cur.def -> cur.in
                std::copy_if(cur.out.begin(), cur.out.end(),
                             std::inserter(cur.in, cur.in.begin()),
                             [&](const Register& reg) {
                                 return cur.def.find(reg) == cur.def.end();
                             });
                size_t updateInSize = cur.in.size();
                if (updateInSize != prevInSize) modified = true;
            }
        }
    } while (modified);
}

void LinerScanRegisterAllocPass::computeGlobalInstrIndexes() {
    for (MachineBasicBlock& mbb : *curFunc) {
        for (MachineInstr& mi : mbb) {
            instrIndexes[&mi] = instrIndexes.size();
        }
    }
}

void LinerScanRegisterAllocPass::computeGlobalIntervals() {
    for (MachineBasicBlock& mbb : *curFunc) {
        if (mbb.empty()) continue;

        auto& liveness = livenesses.at(&mbb);

        // 如果一个寄存器在 BB 的 out 集中，说明它至少活到这个 BB 的最后一条指令
        size_t bbEndIdx = instrIndexes.at(&mbb.back());
        for (Register reg : liveness.out) {
            intervals[reg].end = std::max(intervals[reg].end, bbEndIdx);
        }

        for (MachineInstr& mi : mbb) {
            size_t curIdx = instrIndexes.at(&mi);

            for (auto& op : mi.explicit_defs()) {
                Register reg         = op.getRegister();
                intervals[reg].start = std::min(intervals[reg].start, curIdx);
            }
            for (auto& op : mi.explicit_uses()) {
                Register reg       = op.getRegister();
                intervals[reg].end = std::max(intervals[reg].end, curIdx);
            }
        }
    }

    for (auto& p : intervals) {
        p.second.vreg = p.first;
    }
}

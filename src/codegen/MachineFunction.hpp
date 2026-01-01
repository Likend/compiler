#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "codegen/MachineBasicBlock.hpp"
#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"
#include "ir/Function.hpp"
#include "util/assert.hpp"

namespace codegen {
struct StackObject {
    uint64_t stackID;
    int64_t  spOffset;
    uint64_t size = 32;  // bits
    // Align Alignment;
    const ir::Value* alloca = nullptr;
};

class MachineFunction : public IntrusiveList<MachineBasicBlock> {
   public:
    const ir::Function& f;

    std::map<Register, RegisterInfo> regInfos;
    size_t                           lastRegID = 0;

    std::vector<StackObject> stackObjects;
    uint64_t                 lastStackID = 0;

   public:
    std::string annotation;

    MachineFunction(const ir::Function& f)
        : IntrusiveList<MachineBasicBlock>(*this), f(f) {}

    const ir::Function& getFunction() const { return f; }

    uint64_t CreateStackObject(size_t size, const ir::Value* alloca = nullptr) {
        ASSERT(size % 32 == 0);
        StackObject obj;
        obj.stackID  = lastStackID++;
        obj.spOffset = 0;
        obj.alloca   = alloca;
        obj.size     = size;
        stackObjects.push_back(obj);
        return obj.stackID;
    }

    StackObject* getStackObject(const ir::Value& alloca) {
        for (auto& it : stackObjects) {
            if (it.alloca == &alloca) {
                return &it;
            }
        }
        return nullptr;
    }

    Register CreateVReg() {
        uint32_t index = lastRegID++;
        Register reg{Register::VirtualRegister, index};
        regInfos[reg] = RegisterInfo{};
        return reg;
    }

    RegisterInfo& getRegisterInfo(Register reg) {
        if (auto it = regInfos.find(reg); it != regInfos.end()) {
            return it->second;
        } else {
            UNREACHABLE();
        }
    }

    void addDef(MachineOperand& op) { regInfos[op.getRegister()].addDef(op); }
    void addUse(MachineOperand& op) { regInfos[op.getRegister()].addUse(op); }
    void removeDef(MachineOperand& op) {
        auto  it   = regInfos.find(op.getRegister());
        auto& info = it->second;
        info.removeDef(op);
        if (info.def_empty() && info.use_empty()) {
            regInfos.erase(it);
        }
    }
    void removeUse(MachineOperand& op) {
        auto  it   = regInfos.find(op.getRegister());
        auto& info = it->second;
        info.removeUse(op);
        if (info.def_empty() && info.use_empty()) {
            regInfos.erase(it);
        }
    }
};
}  // namespace codegen

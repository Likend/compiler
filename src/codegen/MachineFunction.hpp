#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "codegen/MachineOperand.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterInfo.hpp"
#include "ir/Function.hpp"
#include "util/assert.hpp"

namespace codegen {
class MachineBasicBlock;

struct StackObject {
    uint64_t stackID;
    int64_t  spOffset;
    uint64_t size = 32;  // bits
    // Align Alignment;
    const ir::Value* alloca = nullptr;
};

class MachineFunction {
   public:
    const ir::Function& f;

    std::vector<std::unique_ptr<MachineBasicBlock>> basicBlocks;

    std::map<Register, RegisterInfo> regInfos;
    size_t                           lastRegID = 0;

    std::vector<StackObject> stackObjects;
    uint64_t                 lastStackID = 0;

   public:
    std::string annotation;

    MachineFunction(const ir::Function& f);
    ~MachineFunction();

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

    void addDef(MachineOperand& op) {
        // auto [it, inserted] = regInfos.try_emplace(reg, RegisterInfo{});
        regInfos[op.getRegister()].addDef(op);
    }

    void addUse(MachineOperand& op) {
        // auto [it, inserted] = regInfos.try_emplace(reg, RegisterInfo{});
        regInfos[op.getRegister()].addUse(op);
    }

    struct iterator
        : public iterator_transform<iterator, decltype(basicBlocks)::iterator,
                                    MachineBasicBlock> {
        using iterator_transform::iterator_transform;
        MachineBasicBlock& transform(
            std::unique_ptr<MachineBasicBlock>& i) const {
            return *i;
        }
    };
    struct const_iterator
        : public iterator_transform<const_iterator,
                                    decltype(basicBlocks)::const_iterator,
                                    const MachineBasicBlock> {
        using iterator_transform::iterator_transform;
        const MachineBasicBlock& transform(
            const std::unique_ptr<MachineBasicBlock>& i) const {
            return *i;
        }
    };
    struct reverse_iterator
        : public iterator_transform<reverse_iterator,
                                    decltype(basicBlocks)::reverse_iterator,
                                    MachineBasicBlock> {
        using iterator_transform::iterator_transform;
        MachineBasicBlock& transform(
            std::unique_ptr<MachineBasicBlock>& i) const {
            return *i;
        }
    };
    struct const_reverse_iterator
        : public iterator_transform<
              const_reverse_iterator,
              decltype(basicBlocks)::const_reverse_iterator,
              const MachineBasicBlock> {
        using iterator_transform::iterator_transform;
        const MachineBasicBlock& transform(
            const std::unique_ptr<MachineBasicBlock>& i) const {
            return *i;
        }
    };

    iterator       begin() { return basicBlocks.begin(); }
    const_iterator begin() const { return basicBlocks.begin(); }
    iterator       end() { return basicBlocks.end(); }
    const_iterator end() const { return basicBlocks.end(); }

    reverse_iterator       rbegin() { return basicBlocks.rbegin(); }
    const_reverse_iterator rbegin() const { return basicBlocks.rbegin(); }
    reverse_iterator       rend() { return basicBlocks.rend(); }
    const_reverse_iterator rend() const { return basicBlocks.rend(); }
};
}  // namespace codegen

#pragma once

#include <memory>
#include <unordered_map>

#include "ir/Function.hpp"
#include "ir/Module.hpp"
#include "ir/Pass.hpp"

namespace codegen {
class MachineFunction;

class MachineModule {
    const ir::Module* module;

    std::unordered_map<const ir::Function*, std::unique_ptr<MachineFunction>>
        functionMap;

   public:
    MachineModule(const ir::Module& module);
    ~MachineModule();

    MachineModule(const MachineModule&)            = delete;
    MachineModule(MachineModule&&)                 = default;
    MachineModule& operator=(const MachineModule&) = delete;
    MachineModule& operator=(MachineModule&&)      = default;

    const ir::Module& getModule() const { return *module; }
    MachineFunction&  getOrCreateMachineFunction(const ir::Function& function);
    MachineFunction*  getMachineFunction(const ir::Function& function);
    void              insertFunction(const ir::Function*              function,
                                     std::unique_ptr<MachineFunction> machineFunction);
};

class MachineModuleAnalysisPass final : public ir::ImmutablePass {
    bool doInitialization(ir::Module& module) override {
        setAnalysis<MachineModule>(static_cast<const ir::Module&>(module));
        auto& mm = getAnalysis<MachineModule>();
        for (auto& function : module) {
            mm.getOrCreateMachineFunction(function);
        }
        return false;
    }
};
}  // namespace codegen

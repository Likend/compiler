#include "codegen/MachineModule.hpp"

#include <memory>

#include "codegen/MachineFunction.hpp"

using namespace codegen;

MachineModule::MachineModule(const ir::Module& module) : module(&module) {}

MachineModule::~MachineModule() = default;

MachineFunction& MachineModule::getOrCreateMachineFunction(
    const ir::Function& function) {
    auto [it, inserted] = functionMap.try_emplace(&function);
    if (inserted) it->second = std::make_unique<MachineFunction>(function);
    return *it->second;
}
MachineFunction* MachineModule::getMachineFunction(
    const ir::Function& function) {
    if (auto it = functionMap.find(&function); it != functionMap.end()) {
        return it->second.get();
    } else {
        return nullptr;
    }
}
void MachineModule::insertFunction(
    const ir::Function*              function,
    std::unique_ptr<MachineFunction> machineFunction) {
    auto [it, insertet] =
        functionMap.try_emplace(function, std::move(machineFunction));
    ASSERT(insertet);
}

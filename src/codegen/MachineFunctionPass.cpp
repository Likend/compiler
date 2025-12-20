#include "codegen/MachineFunctionPass.hpp"

#include "codegen/MachineFunction.hpp"
#include "codegen/MachineModule.hpp"
#include "ir/Function.hpp"
#include "util/assert.hpp"

using namespace codegen;

void MachineFunctionPass::runOnFunction(ir::Function& function) {
    auto&            module = getAnalysis<MachineModule>();
    MachineFunction* mf     = module.getMachineFunction(function);
    ASSERT(mf);
    runOnMachineFunction(*mf);
}

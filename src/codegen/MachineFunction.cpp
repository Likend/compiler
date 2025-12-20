#include "codegen/MachineFunction.hpp"

#include "codegen/MachineBasicBlock.hpp"

using namespace codegen;

MachineFunction::~MachineFunction() = default;
MachineFunction::MachineFunction(const ir::Function& f) : f(f) {}

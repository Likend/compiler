#include "ir/LLVMContext.hpp"

#include <memory>

#include "ir/LLVMContextImpl.hpp"
#include "ir/Module.hpp"
using namespace ir;

LLVMContext::LLVMContext()
    : pImpl(std::unique_ptr<LLVMContextImpl>(  // NOLINT
          new LLVMContextImpl{*this})) {}

LLVMContext::~LLVMContext() = default;

void LLVMContext::addModule(Module* module) {
    pImpl->ownedModules.insert(module);
}

void LLVMContext::removeModule(Module* module) {
    pImpl->ownedModules.erase(module);
}

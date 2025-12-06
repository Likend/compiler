#include "ir/LLVMContextImpl.hpp"

#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"

using namespace ir;
LLVMContextImpl::LLVMContextImpl(LLVMContext& c)
    : voidTy(c, Type::VoidTyID), labelTy(c, Type::LabelTyID) {}

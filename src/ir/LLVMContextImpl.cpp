#include "ir/LLVMContextImpl.hpp"

#include "ir/LLVMContext.hpp"
#include "ir/Type.hpp"

using namespace ir;
LLVMContextImpl::LLVMContextImpl(LLVMContext& c)
    : voidTy(c, Type::VoidTyID),
      labelTy(c, Type::LabelTyID),
      int1Ty(c, 1),
      int8Ty(c, 8),
      int32Ty(c, 32),
      pointerTy(c, 0) {}

#include "ir/Type.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <tuple>

#include "ir/LLVMContext.hpp"
#include "ir/LLVMContextImpl.hpp"
#include "util/assert.hpp"

using namespace ir;

Type* Type::getVoidTy(LLVMContext& c) { return &c.pImpl->voidTy; }
Type* Type::getLabelTy(LLVMContext& c) { return &c.pImpl->labelTy; }

IntegerType* IntegerType::get(LLVMContext& c, unsigned NumBits) {
    LLVMContextImpl* pImpl = c.pImpl.get();
    auto&            it    = pImpl->intTys[NumBits];
    if (!it.get()) {
        it = std::unique_ptr<IntegerType>(new IntegerType{c, NumBits});
    }
    return it.get();
}

FunctionType::FunctionType(Type* ReturnType, const std::vector<Type*>& params,
                           bool isVarArgs)
    : Type(ReturnType->getContext(), FunctionTyID) {
    containedTys.reserve(1 + params.size());
    containedTys.push_back(ReturnType);
    std::copy(params.begin(), params.end(), std::back_inserter(containedTys));
    subclassData = static_cast<size_t>(isVarArgs);
}

FunctionType* FunctionType::get(Type*                     returnType,
                                const std::vector<Type*>& params,
                                bool                      isVarArg) {
    LLVMContextImpl* pImpl = returnType->getContext().pImpl.get();
    // assert
    for (Type* type : params) ASSERT(type);

    auto find = [returnType, &params](const std::unique_ptr<FunctionType>& ty) {
        if (ty->getReturnType() != returnType) return false;
        if (params.size() != ty->getNumParams()) return false;
        for (size_t i = 0; i < params.size(); i++) {
            if (params[i] != ty->getParamType(i)) return false;
        }
        return true;
    };
    if (auto it = std::find_if(pImpl->functionTys.begin(),
                               pImpl->functionTys.end(), find);
        it != pImpl->functionTys.end())
        return it->get();
    else
        return pImpl->functionTys
            .emplace_back(std::unique_ptr<FunctionType>(
                new FunctionType{returnType, params, isVarArg}))
            .get();
}

ArrayType* ArrayType::get(Type* elemType, size_t elemNum) {
    LLVMContextImpl* pImpl = elemType->getContext().pImpl.get();
    auto&            it    = pImpl->arrTyps[std::make_tuple(elemType, elemNum)];
    if (!it.get()) {
        it = std::unique_ptr<ArrayType>(new ArrayType{elemType, elemNum});
    }
    return it.get();
}

PointerType* PointerType::get(LLVMContext&                  c,
                              [[maybe_unused]] unsigned int addrSpace) {
    return &c.pImpl->pointerTy;
}

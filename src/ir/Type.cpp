#include "ir/Type.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "ir/LLVMContext.hpp"
#include "ir/LLVMContextImpl.hpp"
#include "util/assert.hpp"

using namespace ir;

Type* Type::getVoidTy(LLVMContext& c) { return &c.pImpl->voidTy; }
Type* Type::getLabelTy(LLVMContext& c) { return &c.pImpl->labelTy; }

IntegerType::IntegerType(LLVMContext& c, unsigned numBits)
    : Type(c, IntegerTyID) {
    subclassData = numBits;
}

IntegerType* IntegerType::get(LLVMContext& c, unsigned NumBits) {
    LLVMContextImpl* pImpl = c.pImpl.get();
    if (auto it = pImpl->intTys.find(NumBits); it != pImpl->intTys.end()) {
        return it->second.get();
    } else {
        auto emplateResult = pImpl->intTys.try_emplace(
            NumBits, std::unique_ptr<IntegerType>(new IntegerType{c, NumBits}));
        ASSERT(emplateResult.second);
        return emplateResult.first->second.get();
    }
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

ArrayType::ArrayType(Type* elemType, size_t elemNum)
    : Type(elemType->getContext(), ArrayTyID) {
    containedTys.push_back(elemType);
    subclassData = elemNum;
}

ArrayType* ArrayType::get(Type* elemType, size_t elemNum) {
    LLVMContextImpl* pImpl = elemType->getContext().pImpl.get();
    auto find = [elemType, elemNum](const std::unique_ptr<ArrayType>& ty) {
        return ty->getElementType() == elemType &&
               ty->getNumElements() == elemNum;
    };
    if (auto it =
            std::find_if(pImpl->arrTyps.begin(), pImpl->arrTyps.end(), find);
        it != pImpl->arrTyps.end())
        return it->get();
    else
        return pImpl->arrTyps
            .emplace_back(
                std::unique_ptr<ArrayType>(new ArrayType{elemType, elemNum}))
            .get();
}

PointerType::PointerType(LLVMContext& c, unsigned addrSpace)
    : Type(c, PointerTyID) {
    subclassData = addrSpace;
}

PointerType* PointerType::get(LLVMContext&                  c,
                              [[maybe_unused]] unsigned int addrSpace) {
    return &c.pImpl->pointerTy;
}

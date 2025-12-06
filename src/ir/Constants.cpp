#include "ir/Constants.hpp"

#include <memory>
#include <tuple>

#include "ir/LLVMContextImpl.hpp"
#include "ir/Type.hpp"

using namespace ir;

ConstantInt* ConstantInt::get(IntegerType* ty, int32_t v) {
    LLVMContextImpl* pImpl = ty->getContext().pImpl.get();
    auto& it = pImpl->intConstants[std::make_tuple(ty->getBitWidth(), v)];
    if (!it.get()) {
        it = std::unique_ptr<ConstantInt>(new ConstantInt{ty, v});
    }
    return it.get();
}

ConstantArray* ConstantArray::get(ArrayType*                    ty,
                                  const std::vector<Constant*>& val) {
    LLVMContextImpl* pImpl = ty->getContext().pImpl.get();
    return pImpl->constArrayPool
        .emplace_back(
            std::unique_ptr<ConstantArray>(new ConstantArray{ty, val}))
        .get();
}

ConstantString::ConstantString(LLVMContext& c, std::string str)
    : ConstantData(ArrayType::get(IntegerType::get(c, 8), str.size())) {
    this->str = std::move(str);
}

ConstantString* ConstantString::getString(LLVMContext& c, std::string str) {
    LLVMContextImpl* pImpl = c.pImpl.get();
    return pImpl->globalStringPool
        .emplace_back(std::unique_ptr<ConstantString>(
            new ConstantString{c, std::move(str)}))
        .get();
}

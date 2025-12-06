#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/Constants.hpp"
#include "ir/Type.hpp"

namespace std {
template <typename U, typename V>
struct hash<std::tuple<U, V>> {
    size_t operator()(const std::tuple<U, V>& t) const {
        return (std::hash<U>{}(std::get<0>(t)) << 16) |
               std::hash<V>{}(std::get<1>(t));
    }
};
}  // namespace std

namespace ir {
class ConstantInt;
class Module;

class LLVMContextImpl {
   public:
    Type        voidTy;
    Type        labelTy;
    PointerType pointerTy;

    std::vector<std::unique_ptr<FunctionType>>                 functionTys;
    std::vector<std::unique_ptr<ArrayType>>                    arrTyps;
    std::unordered_map<unsigned, std::unique_ptr<IntegerType>> intTys;

    std::unordered_map<std::tuple<unsigned, int32_t>,
                       std::unique_ptr<ConstantInt>>
        intConstants;

    std::unordered_set<Module*> ownedModules;

    std::vector<std::unique_ptr<ConstantArray>>  constArrayPool;
    std::vector<std::unique_ptr<ConstantString>> globalStringPool;

    friend class LLVMContext;
    LLVMContextImpl(LLVMContext& c);

   public:
    LLVMContextImpl(const LLVMContextImpl&)            = delete;
    LLVMContextImpl& operator=(const LLVMContextImpl&) = delete;
};
}  // namespace ir

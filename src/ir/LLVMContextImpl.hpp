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

    template <typename K, typename V>
    using map = std::unordered_map<K, V>;

    template <typename... T>
    using tuple = std::tuple<T...>;

    template <typename T>
    using ptr = std::unique_ptr<T>;

    std::vector<ptr<FunctionType>>            functionTys;
    map<tuple<Type*, size_t>, ptr<ArrayType>> arrTyps;
    map<unsigned, ptr<IntegerType>>           intTys;

    map<tuple<unsigned, int32_t>, ptr<ConstantInt>> intConstants;
    map<Type*, ptr<ConstantAggregateZero>>          zeroConstants;
    map<Type*, ptr<PoisonValue>>                    poisonValues;

    std::unordered_set<Module*> ownedModules;

    std::vector<ptr<ConstantArray>>  constArrayPool;
    std::vector<ptr<ConstantString>> globalStringPool;

    friend class LLVMContext;
    LLVMContextImpl(LLVMContext& c);

   public:
    LLVMContextImpl(const LLVMContextImpl&)            = delete;
    LLVMContextImpl& operator=(const LLVMContextImpl&) = delete;
};
}  // namespace ir

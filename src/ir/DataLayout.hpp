#pragma once
#include "ir/Type.hpp"
#include "util/assert.hpp"

namespace ir {

class DataLayout {
   public:
    size_t getTypeSizeInBits(Type* ty) const {
        switch (ty->getTypeID()) {
            case Type::VoidTyID:
            case Type::LabelTyID:
                return 0;
            case Type::IntegerTyID:
                return static_cast<IntegerType*>(ty)->getBitWidth();
            case Type::PointerTyID:
                return 32;
            case Type::ArrayTyID: {
                auto* aty = static_cast<ArrayType*>(ty);
                return aty->getNumElements() *
                       getTypeSizeInBits(aty->getElementType());
            }
            case Type::FunctionTyID:
            default:
                UNREACHABLE();
        }
    }
};

}  // namespace ir

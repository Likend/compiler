#pragma once

#include <string>

#include "ir/Constants.hpp"
#include "ir/Type.hpp"
#include "util/IntrusiveList.hpp"

namespace ir {
class GlobalValue : public Constant {
   public:
    enum LinkageTypes {
        ExternalLinkage = 0,  ///< Externally visible function
        InternalLinkage,      ///< Rename collisions when linking (static
                              ///< functions).
    };

   protected:
    GlobalValue(Type* ty, size_t numOperands, LinkageTypes linkage,
                std::string name, unsigned addrSpace)
        : Constant(PointerType::get(ty->getContext(),
                                    addrSpace),  // GlobalValue are pointers
                   numOperands),
          linkage(linkage),
          valueTy(ty) {
        setName(std::move(name));
    }

   private:
    friend class Constant;

    LinkageTypes linkage;
    Type*        valueTy;

   public:
    PointerType* getType() const {
        return static_cast<PointerType*>(Value::getType());
    }
    Type*        getValueType() const { return valueTy; }
    LinkageTypes getLinkageType() const { return linkage; }

    virtual bool isDeclaration() const = 0;
};

class GlobalObject : public GlobalValue {
   protected:
    GlobalObject(Type* ty, size_t numOperands, LinkageTypes linkage,
                 std::string name, unsigned addrSpace)
        : GlobalValue(ty, numOperands, linkage, std::move(name), addrSpace) {}
};

class GlobalVariable final : public GlobalObject,
                             public IntrusiveNodeWithParent<Module> {
    bool isConstantGlobal;

   public:
    GlobalVariable(Type* ty, bool isConstant, LinkageTypes linkage,
                   Constant* initializer, std::string name,
                   unsigned addrSpace = 0)
        : GlobalObject(ty, 1, linkage, std::move(name), addrSpace),
          isConstantGlobal(isConstant) {
        if (initializer) setOperand(0, initializer);
    }

    static GlobalVariable* Create(Module& module, Type* ty, bool isConstant,
                                  LinkageTypes linkage, Constant* initializer,
                                  std::string name, unsigned addrSpace = 0);

    bool hasInitializer() const { return getNumOperands() != 0; }

    const Constant* getInitializer() const {
        ASSERT_WITH(hasInitializer(), "GV doesn't have initializer!");
        return static_cast<Constant*>(getOperand(0));
    }

    Constant* getInitializer() {
        ASSERT_WITH(hasInitializer(), "GV doesn't have initializer!");
        return static_cast<Constant*>(getOperand(0));
    }

    void setInitializer(Constant* InitVal) {
        ASSERT_WITH(InitVal->getType() == getValueType(),
                    "Initializer type must match GlobalVariable type");
        setOperand(0, InitVal);
    }

    bool isConstant() const { return isConstantGlobal; }
    void setConstant(bool Val) { isConstantGlobal = Val; }

    // Globals are definitions if they have an initializer.
    bool isDeclaration() const override { return getNumOperands() != 0; }
};
}  // namespace ir

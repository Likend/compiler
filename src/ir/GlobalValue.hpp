#pragma once

#include <string>

#include "ir/Constants.hpp"
#include "ir/Type.hpp"

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
                std::string name, unsigned addrSpace, Module& m)
        : Constant(PointerType::get(ty, addrSpace),  // GlobalValue are pointers
                   numOperands),
          linkage(linkage),
          // valueTy(ty),
          parent(&m) {
        setName(std::move(name));
    }

   private:
    friend class Constant;

    LinkageTypes linkage;
    // Type*        valueTy;

   protected:
    Module* parent = nullptr;
    void    setParent(Module* parent) { this->parent = parent; }

   public:
    PointerType* getType() const {
        return static_cast<PointerType*>(Value::getType());
    }
    Type*        getValueType() const { return getType()->getPointeeType(); }
    LinkageTypes getLinkageType() const { return linkage; }
    Module*      getParent() const { return parent; }

    virtual bool isDeclaration() const = 0;
};

class GlobalObject : public GlobalValue {
   protected:
    GlobalObject(Type* ty, size_t numOperands, LinkageTypes linkage,
                 std::string name, unsigned addrSpace, Module& m)
        : GlobalValue(ty, numOperands, linkage, std::move(name), addrSpace, m) {
    }
};

class GlobalVariable final : public GlobalObject {
    bool isConstantGlobal;

    GlobalVariable(Module& m, Type* ty, bool isConstant, LinkageTypes linkage,
                   Constant* initializer, std::string name, unsigned addrSpace);

   public:
    static GlobalVariable* Create(Module& m, Type* ty, bool isConstant,
                                  LinkageTypes linkage, Constant* initializer,
                                  std::string name, unsigned addrSpace = 0) {
        return new GlobalVariable(m, ty, isConstant, linkage, initializer,
                                  std::move(name), addrSpace);
    }

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

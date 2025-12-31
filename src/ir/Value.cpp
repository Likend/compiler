#include "ir/Value.hpp"

#include "ir/LLVMContext.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"
#include "ir/Use.hpp"
#include "ir/ValueSymbolTable.hpp"

using namespace ir;

void Value::addUse(Use& use) { use.addToList(&useList); }

LLVMContext& Value::getContext() const { return ty->getContext(); }

void Value::setName(std::string name) {
    ASSERT_WITH(name.empty() || !getType()->isVoidTy(),
                "Cannot assign a name to void values!");
    this->name = std::move(name);
}

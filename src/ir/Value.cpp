#include "ir/Value.hpp"

#include "ir/LLVMContext.hpp"
#include "ir/Module.hpp"
#include "ir/Type.hpp"
#include "ir/Use.hpp"
#include "ir/ValueSymbolTable.hpp"
#include "util/assert.hpp"

using namespace ir;

void Value::addUse(Use& use) { use.addToList(&useList); }

LLVMContext& Value::getContext() const { return ty->getContext(); }

void Value::setName(std::string name) {
    ASSERT_WITH(name.empty() || !getType()->isVoidTy(),
                "Cannot assign a name to void values!");
    this->name = std::move(name);
}
void Value::replaceAllUsesWith(Value* newValue) {
    ASSERT(newValue);
    ASSERT_WITH(newValue->getType() == getType(),
                "replaceAllUses of value with new value of different type!");
    while (useList) {
        Use& use = *useList;
        use.set(newValue);
    }
}

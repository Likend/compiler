#pragma once
#include <memory>

namespace ir {
class Module;
class Type;
class LLVMContextImpl;
class LLVMContext {
   public:
    std::unique_ptr<LLVMContextImpl> pImpl;
    LLVMContext();
    ~LLVMContext();
    LLVMContext(const LLVMContext&)            = delete;
    LLVMContext& operator=(const LLVMContext&) = delete;

   private:
    friend class Module;
    void addModule(Module*);
    void removeModule(Module*);
};
}  // namespace ir

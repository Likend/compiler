#pragma once

#include <vector>

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Instructions.hpp"
#include "ir/Pass.hpp"

namespace opt {
class MemToRegPass final : public ir::FunctionPass {
   public:
    bool runOnFunction(ir::Function&) override;

   private:
    bool replaceInnerBlock(ir::BasicBlock&);
    bool replaceOnceStore(ir::Function&);

    std::vector<ir::AllocaInst*> getAllocas(ir::Function&);
};
}  // namespace opt

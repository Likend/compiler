#pragma once

#include "ir/BasicBlock.hpp"
#include "ir/Function.hpp"
#include "ir/Pass.hpp"

namespace opt {
class SimplifyCFGPass final : public ir::FunctionPass {
   public:
    bool runOnFunction(ir::Function&) override;
};
}  // namespace opt

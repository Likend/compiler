#pragma once

#include "ir/Pass.hpp"

namespace opt {
class ConstantPropagationPass final : public ir::FunctionPass {
   public:
    bool runOnFunction(ir::Function&) override;
};
}  // namespace opt

#pragma once

#include "ir/Pass.hpp"

namespace ir {
class WellFormPass final : public FunctionPass {
   public:
    bool runOnFunction(ir::Function& f) override;
};
}  // namespace ir

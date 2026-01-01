#pragma once

#include <ostream>

#include "ir/Pass.hpp"

namespace ir {
class Module;

class IRPrinterPass final : public ImmutablePass {
   public:
    IRPrinterPass(std::ostream& os) : os(os) {}

    bool doInitialization(Module&) override;

   private:
    std::ostream& os;
};
};  // namespace ir

#pragma once

#include <ostream>

#include "ir/Pass.hpp"

namespace ir {
class Module;

class IRPrinterPass final : public ImmutablePass {
   public:
    IRPrinterPass(std::ostream& os) : os(os) {}

    void doInitialization(Module&) override;

   private:
    std::ostream& os;
};
};  // namespace ir

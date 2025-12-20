#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <variant>

#include "codegen/Register.hpp"
#include "ir/GlobalValue.hpp"
#include "util/assert.hpp"
#include "util/lambda_overload.hpp"

namespace codegen {
class MachineInstr;
class MachineBasicBlock;

struct MachineOperandBase {};
struct RegisterOpKind : MachineOperandBase {
    Register reg;
    RegisterOpKind(Register reg) : reg(reg) {}
};
struct ImmediateOpKind : MachineOperandBase {
    int64_t imm;
    ImmediateOpKind(int64_t imm) : imm(imm) {}
};
struct MachineBBOpKind : MachineOperandBase {
    MachineBasicBlock* mbb;
    MachineBBOpKind(MachineBasicBlock* mbb) : mbb(mbb) {}
};
struct OffsetOperandBase : MachineOperandBase {
    int64_t offset;
    OffsetOperandBase(int64_t offset) : offset(offset) {}
};
struct GlobalValueOpKind : OffsetOperandBase {
    const ir::GlobalValue* globalVal;
    GlobalValueOpKind(const ir::GlobalValue* globalVal, int64_t offset)
        : OffsetOperandBase(offset), globalVal(globalVal) {}
};
struct SymbolOpKind : OffsetOperandBase {
    std::string_view name;
    SymbolOpKind(std::string_view name, int64_t offset)
        : OffsetOperandBase(offset), name(name) {}
};

struct MachineOperandContent
    : std::variant<RegisterOpKind, ImmediateOpKind, MachineBBOpKind,
                   GlobalValueOpKind, SymbolOpKind> {};

class MachineOperand {
   private:
    MachineOperandContent content;
    MachineInstr*         parent;

    template <typename KindT>
    bool isKind() const {
        static_assert(std::is_base_of_v<MachineOperandBase, KindT>);
        return std::holds_alternative<KindT>(content);
    }

    template <typename KindT>
    KindT& get() {
        ASSERT_WITH(isKind<KindT>(), "Invalid Kind");
        return std::get<KindT>(content);
    }

    template <typename KindT>
    const KindT& get() const {
        ASSERT_WITH(isKind<KindT>(), "Invalid Kind");
        return std::get<KindT>(content);
    }

   public:
    explicit MachineOperand(MachineOperandContent t, MachineInstr& parent)
        : content(t), parent(&parent) {}

    MachineInstr&       getParent() { return *parent; }
    const MachineInstr& getParent() const { return *parent; };

    size_t getOperandNo() const;

    const MachineOperandContent& getContent() const { return content; }

    bool isRegister() const { return isKind<RegisterOpKind>(); }
    bool isImmediate() const { return isKind<ImmediateOpKind>(); }
    bool isMachineBasicBlock() const { return isKind<MachineBBOpKind>(); }
    bool isGlobalValue() const { return isKind<GlobalValueOpKind>(); }
    bool isExternalSymbol() const { return isKind<SymbolOpKind>(); }

    // for MO_Register

    Register getRegister() const { return get<RegisterOpKind>().reg; }
    bool     isUse() const;
    bool     isDef() const;

    void setRegister();  // TODO

    // for others

    int64_t getImmediate() const { return get<ImmediateOpKind>().imm; }
    void setImmediate(int64_t immVal) { get<ImmediateOpKind>().imm = immVal; }

    MachineBasicBlock* getMachineBasicBlock() const {
        return get<MachineBBOpKind>().mbb;
    }
    void setMachineBasicBlock(MachineBasicBlock* mbb) {
        get<MachineBBOpKind>().mbb = mbb;
    }

    const ir::GlobalValue* getGlobalValue() const {
        return get<GlobalValueOpKind>().globalVal;
    }

    std::string_view getSymbolName() const { return get<SymbolOpKind>().name; }

    int64_t getOffset() const {
        return std::visit(
            overloaded{
                [](const OffsetOperandBase& o) { return o.offset; },
                [](const MachineOperandBase&) -> int64_t { UNREACHABLE(); }},
            content);
    }
    void setOffset(int64_t offset) {
        std::visit(
            overloaded{[offset](OffsetOperandBase& o) { o.offset = offset; },
                       [](MachineOperandBase&) { UNREACHABLE(); }},
            content);
    }

    // Change to...
    void ChangeTo(MachineOperandContent newContent) { content = newContent; }
};
}  // namespace codegen

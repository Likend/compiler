#pragma once

#include <cstdint>
#include <functional>

namespace codegen {
class Register {
   public:
    enum RegisterType { PhysicalRegister = 0, StackRegister, VirtualRegister };

   private:
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnested-anon-types"
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#endif
    union {
        struct {
            RegisterType type : 2;
            uint32_t     regNo : 30;
        };
        uint32_t val : 32;
    };
#ifdef __clang__
#pragma clang diagnostic pop
#endif

   public:
    constexpr Register(uint32_t val = 0) : val(val) {}
    constexpr Register(RegisterType type, uint32_t index)
        : type(type), regNo(index) {}

    constexpr bool isStack() const { return type == StackRegister; }
    constexpr bool isPhysical() const { return type == PhysicalRegister; }
    constexpr bool isVirtual() const { return type == VirtualRegister; }

    constexpr uint32_t index() const { return regNo; }
    constexpr uint32_t innerVal() const { return val; }

    constexpr bool operator==(Register other) const { return val == other.val; }
    constexpr bool operator!=(Register other) const { return val != other.val; }

    constexpr bool operator<(Register other) const { return val < other.val; }
};

#define HANDLE_REG_DEF(name, index)                         \
    [[maybe_unused]] constexpr static Register REG_##name { \
        Register::PhysicalRegister, index                   \
    }
#include "codegen/RegDefs.hpp"
}  // namespace codegen

namespace std {
template <>
struct hash<codegen::Register> {
    size_t operator()(const codegen::Register& t) const noexcept {
        return t.innerVal();
    }
};
}  // namespace std

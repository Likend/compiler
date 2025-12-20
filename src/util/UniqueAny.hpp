#pragma once

#include <memory>

#include "util/assert.hpp"

class UniqueAny {
    struct Base {
        virtual ~Base()                                     = default;
        virtual const std::type_info& type() const noexcept = 0;
    };
    template <typename T>
    struct Derived : Base {
        T value;
        template <typename... Args>
        Derived(Args&&... args) : value(std::forward<Args>(args)...) {}

        const std::type_info& type() const noexcept override {
            return typeid(T);
        }
    };

    std::unique_ptr<Base> storage;

   public:
    UniqueAny() = default;

    template <typename T, typename... Args>
    void emplace(Args&&... args) {
        storage = std::make_unique<Derived<std::decay_t<T>>>(
            std::forward<Args>(args)...);
    }

    template <typename T>
    T& cast() {
        ASSERT_WITH(has_value(), "UniqueAny is empty");
        if (typeid(std::decay_t<T>) != storage->type()) UNREACHABLE();
        return static_cast<Derived<T>*>(storage.get())->value;
    }

    bool has_value() const { return storage.get(); }

    UniqueAny(UniqueAny&&)            = default;
    UniqueAny& operator=(UniqueAny&&) = default;
};

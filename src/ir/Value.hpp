#pragma once

#include <cstddef>
#include <iterator>
#include <string>

#include "ir/LLVMContext.hpp"
#include "ir/Use.hpp"
#include "util/assert.hpp"
#include "util/iterator_range.hpp"
#include "util/iterator_transform.hpp"

namespace ir {
class User;
class Type;

class Value {
   protected:
    std::string name;

   private:
    Type* ty;
    Use*  useList = nullptr;

   protected:
    Value(Type* ty) : ty(ty) {}

   public:
    virtual ~Value() = default;

    std::string_view getName() const { return name; }
    void             setName(std::string name);

    Type*        getType() const { return ty; }
    LLVMContext& getContext() const;

    void addUse(Use& user);

   private:
    template <typename UseT>  // One of "Use" and "const Use"
    class use_iterator_impl {
        friend class Value;
        UseT* u = nullptr;
        use_iterator_impl(UseT* u) : u(u) {}

       public:
        use_iterator_impl() = default;

        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = UseT;
        using pointer           = UseT*;
        using reference         = UseT&;

        use_iterator_impl& operator++() {
            ASSERT_WITH(u, "Cannot increment end iterator!");
            u = u->getNext();
            return *this;
        }
        use_iterator_impl operator++(int) {
            ASSERT_WITH(u, "Cannot increment end iterator!");
            return {u->getNext()};
        }
        reference operator*() const {
            ASSERT_WITH(u, "Cannot dereference end iterator!");
            return *u;
        }
        pointer operator->() const { return &operator*(); }

        bool operator==(const use_iterator_impl& x) const { return u == x.u; }
        bool operator!=(const use_iterator_impl& x) const { return u != x.u; }
    };

    template <typename UserT,  // UserT: one of "User" and "const User"
              typename UseT>   // UseT: one of "Use" and "const Use"
    class user_iterator_impl
        : public iterator_transform<user_iterator_impl<UserT, UseT>,
                                    use_iterator_impl<UseT>, UserT> {
       public:
        UserT& transform(
            typename use_iterator_impl<UseT>::value_type& u) const {
            return *u.getUser();
        }
    };

   public:
    using use_iterator        = use_iterator_impl<Use>;
    using const_use_iterator  = use_iterator_impl<const Use>;
    using user_iterator       = user_iterator_impl<User, Use>;
    using const_user_iterator = user_iterator_impl<const User, const Use>;
    using use_range           = iterator_range<use_iterator>;
    using const_use_range     = iterator_range<const_use_iterator>;
    using user_range          = iterator_range<user_iterator>;
    using const_user_range    = iterator_range<const_user_iterator>;

    bool use_empty() const { return useList == nullptr; }

    // clang-format off
    use_iterator        use_begin()        { return {useList}; }
    const_use_iterator  use_begin()  const { return {useList}; }
    use_iterator        use_end()          { return {}; }
    const_use_iterator  use_end()    const { return {}; }
    use_range           uses()             { return {use_begin(), use_end()}; }
    const_use_range     uses()       const { return {use_begin(), use_end()}; }

    user_iterator       user_begin()       { return {use_begin()}; }
    const_user_iterator user_begin() const { return {use_begin()}; }
    user_iterator       user_end()         { return {}; }
    const_user_iterator user_end()   const { return {}; }
    user_range          users()            { return {user_begin(), user_end()}; }
    const_user_range    users()      const { return {user_begin(), user_end()}; }
    // clang-format on

    void replaceAllUsesWith(Value* newValue);
};
}  // namespace ir

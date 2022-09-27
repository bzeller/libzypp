/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
* //Code based on a blogpost on https://functionalcpp.wordpress.com/2013/08/05/function-traits/
*
*/
#ifndef ZYPPNG_META_FUNCTION_TRAITS_IMPL_H_INCLUDED
#define ZYPPNG_META_FUNCTION_TRAITS_IMPL_H_INCLUDED

#include <cstddef>
#include <tuple>
#include <zypp-core/zyppng/meta/TypeTraits>

namespace zyppng {

  namespace detail {
    template<class F, class = void >
    struct function_traits_impl;

    template<class R, class... Args>
    struct function_traits_impl<R(Args...)>
    {
        using return_type = R;

        static constexpr std::size_t arity = sizeof...(Args);

        template <std::size_t N>
        struct argument
        {
            static_assert(N >= 0 && N < arity, "error: invalid parameter index.");
            using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
        };
    };

    // function pointer
    template<class R, class... Args>
    struct function_traits_impl<R(*)(Args...)> : public function_traits_impl<R(Args...)>
    { };

    // function ref
    template<class R, class... Args>
    struct function_traits_impl<R(&)(Args...)> : public function_traits_impl<R(Args...)>
    { };

    // member function pointer
    template<class C, class R, class... Args>
    struct function_traits_impl<R(C::*)(Args...)> : public function_traits_impl<R(C&,Args...)>
    {};

    // const member function pointer, this will match lambdas too
    template<class C, class R, class... Args>
    struct function_traits_impl<R(C::*)(Args...) const>  : public function_traits_impl<R(C&,Args...)>
    {};

    // member object pointer
    template<class C, class R>
    struct function_traits_impl<R(C::*)> : public function_traits_impl<R(C&)>
    {};

    template <typename T>
    using has_call_operator = decltype (&T::operator());

    //functor with one overload
    template<class F>
    struct function_traits_impl<F, std::void_t<decltype (&F::operator())>> : public function_traits_impl<decltype (&F::operator())>
    {};
  }

  template<class F >
  struct function_traits : public detail::function_traits_impl< std::remove_cv_t< std::remove_reference_t <F> > >
  {};
}
#endif

#ifndef __LDIFF_TRAITS_H
#define __LDIFF_TRAITS_H

#include <string>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fp {
namespace traits {

template<typename T, typename Tuple>
struct in_tuple;

template<typename T>
struct in_tuple<T, std::tuple<>> : std::false_type
{};

template<typename T, typename... TupleElementTypes>
struct in_tuple<T, std::tuple<TupleElementTypes...>> : std::disjunction<std::is_same<T, TupleElementTypes>...>::type
{};

template<typename Tuple, typename... Item>
struct both_in_tuple : std::conjunction<in_tuple<Item, Tuple>...>::type
{};

template<typename Tuple>
struct pitch_required
{
    template<std::size_t... Indices>
    static constexpr auto make_required(std::index_sequence<Indices...>)
    {
        return std::tuple_cat(std::conditional_t<std::tuple_element_t<Indices, Tuple>::required,
                                                 std::tuple<std::tuple_element_t<Indices, Tuple>>, std::tuple<>>{}...);
    }

    using type = decltype(make_required(std::make_index_sequence<std::tuple_size<Tuple>::value>{}));
};

template<typename RequiredTuple, typename Tuple>
struct check_required
{
    template<std::size_t... Indices>
    static constexpr auto check(std::index_sequence<Indices...>)
    {
        constexpr bool value = (in_tuple<std::tuple_element_t<Indices, RequiredTuple>, Tuple>::value && ...);
        static_assert(value, "Not all types in Required are present in Tuple");
        return std::bool_constant<value>{};
    }
    using type = std::bool_constant<check(std::make_index_sequence<std::tuple_size_v<RequiredTuple>>{})>;
};

template<typename Options, typename RequiredOptions, typename... Args>
struct check_options
{
    using type = std::conjunction<both_in_tuple<Options, Args...>,
                                  typename check_required<RequiredOptions, std::tuple<Args...>>::type>;
};

}  // namespace traits
}  // namespace fp

#endif  // __LDIFF_TRAITS_H
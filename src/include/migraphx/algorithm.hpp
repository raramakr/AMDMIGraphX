#ifndef MIGRAPHX_GUARD_RTGLIB_ALGORITHM_HPP
#define MIGRAPHX_GUARD_RTGLIB_ALGORITHM_HPP

#include <algorithm>
#include <numeric>
#include <migraphx/config.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {

template <class Iterator, class Output, class Predicate, class F>
void transform_if(Iterator start, Iterator last, Output out, Predicate pred, F f)
{
    while(start != last)
    {
        if(pred(*start))
        {
            *out = f(*start);
            ++out;
        }
        ++start;
    }
}

template <class Iterator, class T, class BinaryOp, class UnaryOp>
T transform_accumulate(Iterator first, Iterator last, T init, BinaryOp binop, UnaryOp unaryop)
{
    return std::inner_product(
        first, last, first, init, binop, [&](auto&& x, auto&&) { return unaryop(x); });
}

template <class Iterator, class Output, class Predicate>
void group_by(Iterator start, Iterator last, Output out, Predicate pred)
{
    while(start != last)
    {
        auto it = std::partition(start, last, [&](auto&& x) { return pred(x, *start); });
        out(start, it);
        start = it;
    }
}

template <class Iterator, class Output, class Predicate>
void group_unique(Iterator start, Iterator last, Output out, Predicate pred)
{
    while(start != last)
    {
        auto it = std::find_if(start, last, [&](auto&& x) { return not pred(*start, x); });
        out(start, it);
        start = it;
    }
}

template <class Iterator1, class Iterator2>
std::ptrdiff_t
levenshtein_distance(Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2)
{
    if(first1 == last1)
        return std::distance(first2, last2);
    if(first2 == last2)
        return std::distance(first1, last1);
    if(*first1 == *first2)
        return levenshtein_distance(std::next(first1), last1, std::next(first2), last2);
    auto x1 = levenshtein_distance(std::next(first1), last1, std::next(first2), last2);
    auto x2 = levenshtein_distance(first1, last1, std::next(first2), last2);
    auto x3 = levenshtein_distance(std::next(first1), last1, first2, last2);
    return std::ptrdiff_t{1} + std::min({x1, x2, x3});
}

} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif

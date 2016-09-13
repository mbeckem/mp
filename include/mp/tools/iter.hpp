#ifndef MP_TOOLS_ITER_HPP
#define MP_TOOLS_ITER_HPP

#include "../defs.hpp"

namespace mp {

// An iter_range is a pair of two iterators of the same type.
// One can iterate over the range by using the begin() and end()
// iterators or by using a for-each loop.
template<typename Iter>
struct iter_range
{
    iter_range(Iter begin, Iter end)
        : m_begin(begin)
        , m_end(end)
    {}

    Iter begin() const { return m_begin; }
    Iter end() const   { return m_end; }

private:
    Iter m_begin, m_end;
};

template<typename Iter>
iter_range<Iter> make_iter_range(Iter begin, Iter end)
{
    return {begin, end};
}

template<typename Iter>
iter_range<Iter> make_iter_range(pair<Iter, Iter> p)
{
    return {p.first, p.second};
}

template<typename Iter>
iter_range<Iter> make_iter_range(tuple<Iter, Iter> t)
{
    return {get<0>(t), get<1>(t)};
}

// Iterate over the given range.
// The function "f" will be called with two arguments,
// the index of the current element and a reference
// to the element itself.
template<typename Range, typename Func>
void for_each_index(Range &&r, Func &&f)
{
    size_t index = 0;
    for (auto &&v : r) {
        f(index, v);
        ++index;
    }
}

} // namespace bew

#endif // MP_TOOLS_ITER_HPP

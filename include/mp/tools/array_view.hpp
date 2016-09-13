#ifndef MP_TOOLS_ARRAY_VIEW_HPP
#define MP_TOOLS_ARRAY_VIEW_HPP

#include <type_traits>

#include "../defs.hpp"

namespace mp {

/*
 * An array_view represents a contiguous region of (unowned) memory.
 * It is represented by a pointer pair [begin, end).
 *
 * An array_view can be sliced to create a new array_view that represents
 * an inner region of the view.
 *
 * Use array_view<const T> if you wish to create a view over immutable data.
 */
template<typename T>
class array_view
{
public:
    using value_type        = T;
    using iterator          = T*;
    using const_iterator    = T*;
    using reference         = T&;
    using const_reference   = T&;

public:
    // Constructors
    array_view();
    array_view(T *begin, T *end);
    array_view(T *begin, size_t size);

    template<typename U>
    array_view(vector<U> &vec);

    template<typename U>
    array_view(const vector<U> &vec);

    template<size_t N>
    array_view(T (&array)[N]);

    array_view(const array_view &) = default;
    array_view(array_view &&) = default;

    // Iterators
    iterator begin() const;
    iterator end() const;

    // Access
    bool        empty() const;
    size_t      size() const;
    reference   operator [](size_t index) const;
    reference   at(size_t index) const;
    reference   front() const;
    reference   back() const;

    // Slicing (index based)
    array_view slice(size_t start_index) const;
    array_view slice(size_t start_index, size_t length) const;

    // Slicing (iterator based)
    array_view slice(iterator begin) const;
    array_view slice(iterator begin, iterator end) const;

    array_view& operator =(const array_view &) = default;
    array_view& operator =(array_view &&) = default;

    // Array views are convertible to their const version.
    operator array_view<const T>() const;

private:
    T *m_begin = nullptr;
    size_t m_size = 0;
};

// IMPL

template<typename T>
array_view<T>::array_view() {}

template<typename T>
array_view<T>::array_view(T *begin, T *end)
    : m_begin(begin)
    , m_size(end - begin)
{
    assert((end - begin) >= 0 && "End >= begin");
}

template<typename T>
array_view<T>::array_view(T *begin, size_t size)
    : m_begin(begin)
    , m_size(size)
{}

template<typename T>
template<typename U>
array_view<T>::array_view(vector<U> &vec)
    : m_begin(vec.data())
    , m_size(vec.size())
{}

template<typename T>
template<typename U>
array_view<T>::array_view(const vector<U> &vec)
    : m_begin(vec.data())
    , m_size(vec.size())
{}

template<typename T>
template<size_t N>
array_view<T>::array_view(T (&array)[N])
    : m_begin(array)
    , m_size(N)
{}

template<typename T>
typename array_view<T>::iterator array_view<T>::begin() const
{
    return m_begin;
}

template<typename T>
typename array_view<T>::iterator array_view<T>::end() const
{
    return m_begin + m_size;
}

template<typename T>
bool array_view<T>::empty() const
{
    return m_size == 0;
}

template<typename T>
size_t array_view<T>::size() const
{
    return m_size;
}

template<typename T>
typename array_view<T>::reference array_view<T>::operator [](size_t index) const
{
    assert(index < size() && "Index in range");
    return *(m_begin + index);
}

template<typename T>
typename array_view<T>::reference array_view<T>::at(size_t index) const
{
    if (index >= size()) {
        throw std::out_of_range("array_view::at()");
    }
    return *(m_begin + index);
}

template<typename T>
typename array_view<T>::reference array_view<T>::front() const
{
    assert(!empty() && "Range has front");
    return *m_begin;
}

template<typename T>
typename array_view<T>::reference array_view<T>::back() const
{
    assert(!empty() && "Range has back");
    return *(m_begin + m_size - 1);
}

template<typename T>
array_view<T> array_view<T>::slice(size_t start_index) const
{
    return slice(start_index, size() - start_index);
}

template<typename T>
array_view<T> array_view<T>::slice(size_t start_index, size_t length) const
{
    assert(start_index <= size() && "Start index in range");
    assert(start_index + length <= size() && "Length in range");
    iterator begin = m_begin + start_index;
    return array_view<T>(begin, begin + length);
}

template<typename T>
array_view<T> array_view<T>::slice(iterator begin) const
{
    return slice(begin, end());
}

template<typename T>
array_view<T> array_view<T>::slice(iterator begin, iterator end) const
{
    assert((begin >= m_begin && begin <= this->end()) && "Begin in range");
    assert((end >= m_begin && end <= this->end() && end >= begin && "End in range"));
    return array_view<T>(begin, end);
}

template<typename T>
array_view<T>::operator array_view<const T>() const
{
    return array_view<const T>(m_begin, m_size);
}

} // namespace bew

#endif // MP_TOOLS_ARRAY_VIEW_HPP

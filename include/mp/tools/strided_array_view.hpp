#ifndef MP_TOOLS_STRIDED_ARRAY_VIEW_HPP
#define MP_TOOLS_STRIDED_ARRAY_VIEW_HPP

#include <boost/iterator/iterator_facade.hpp>

#include "../defs.hpp"
#include "array_view.hpp"

namespace mp {

/*
 * A template class similar to "array_view" but with a non-zero
 * gap (the "stride") between each of its elements.
 *
 * For example, given the following vector
 *
 *      vector<int> v{1, 2, 3, 4, 5, 6};
 *
 * and a strided_array_view declared like this
 *
 *      strided_array_view<int> i(v.begin(), v.end(), 2);
 *
 * the contents of i will be {1, 3, 5}.
 */
template<typename T>
class strided_array_view
{
public:
    struct iterator;

    using value_type        = T;
    using const_iterator    = iterator;
    using reference         = T&;
    using const_reference   = T&;

public:
    // Constructors
    strided_array_view();
    strided_array_view(T *begin, T *end, size_t stride);
    strided_array_view(T *begin, size_t size, size_t stride);
    strided_array_view(array_view<T> v, size_t stride);

    strided_array_view(const strided_array_view &) = default;
    strided_array_view(strided_array_view &&) = default;

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
    strided_array_view slice(size_t start_index) const;
    strided_array_view slice(size_t start_index, size_t length) const;

    // Slicing (iterator based)
    strided_array_view slice(iterator begin) const;
    strided_array_view slice(iterator begin, iterator end) const;

    strided_array_view& operator =(const strided_array_view &) = default;
    strided_array_view& operator =(strided_array_view &&) = default;

    // Array views are convertible to their const version.
    operator strided_array_view<const T>() const;

private:
    T *m_begin = nullptr;
    size_t m_size = 0;
    size_t m_stride = 0;
};

template<typename T>
struct strided_array_view<T>::iterator
        : boost::iterator_facade<iterator, T, std::random_access_iterator_tag>
{
public:
    iterator(): m_item(nullptr), m_stride(1) {}

    iterator(T *item, size_t stride)
      : m_item(item)
      , m_stride(stride)
    {
        assert(stride != 0);
    }

private:
    using base = boost::iterator_facade<iterator, T, boost::random_access_traversal_tag>;
    friend class boost::iterator_core_access;

    T& dereference() const
    {
        return *m_item;
    }

    bool equal(const iterator &other) const
    {
        assert(m_stride == other.m_stride);
        return m_item == other.m_item;
    }

    void increment()
    {
        m_item += m_stride;
    }

    void decrement()
    {
        m_item -= m_stride;
    }

    void advance(ptrdiff_t n)
    {
        m_item += n * static_cast<ptrdiff_t>(m_stride);
    }

    ptrdiff_t distance_to(const iterator &other) const
    {
        return (other.m_item - m_item) / static_cast<ptrdiff_t>(m_stride);
    }

private:
    T *m_item;
    size_t m_stride;
};

// Constructors
template<typename T>
strided_array_view<T>::strided_array_view()
    : m_begin(nullptr), m_size(0), m_stride(1)
{}

template<typename T>
strided_array_view<T>::strided_array_view(T *begin, T *end, size_t stride)
    : m_begin(begin)
    , m_size((end - begin) / stride)
    , m_stride(stride)
{
    assert(end >= begin && "End >= begin");
    assert(stride != 0 && "Stride must never be 0");
}

template<typename T>
strided_array_view<T>::strided_array_view(T *begin, size_t size, size_t stride)
    : m_begin(begin)
    , m_size(size)
    , m_stride(stride)
{
    assert(stride != 0 && "Stride must never be 0");
}

template<typename T>
strided_array_view<T>::strided_array_view(array_view<T> v, size_t stride)
    : m_begin(v.begin())
    , m_size(v.size() / stride)
    , m_stride(stride)
{
    assert(stride != 0 && "Stride must never be 0");
}

// Iterators
template<typename T>
typename strided_array_view<T>::iterator strided_array_view<T>::begin() const
{
    return iterator(m_begin, m_stride);
}

template<typename T>
typename strided_array_view<T>::iterator strided_array_view<T>::end() const
{
    return iterator(m_begin + (m_size * m_stride), m_stride);
}

// Access
template<typename T>
bool strided_array_view<T>::empty() const
{
    return m_size == 0;
}

template<typename T>
size_t strided_array_view<T>::size() const
{
    return m_size;
}

template<typename T>
typename strided_array_view<T>::reference strided_array_view<T>::operator [](size_t index) const
{
    assert(index < m_size && "Index in range");
    return *(m_begin + (index * m_stride));
}

template<typename T>
typename strided_array_view<T>::reference strided_array_view<T>::at(size_t index) const
{
    if (index >= m_size) {
        throw std::out_of_range("strided_array_view::at()");
    }
    return *(m_begin + (index * m_stride));
}

template<typename T>
typename strided_array_view<T>::reference strided_array_view<T>::front() const
{
    assert(!empty() && "Range has front");
    return *m_begin;
}

template<typename T>
typename strided_array_view<T>::reference strided_array_view<T>::back() const
{
    assert(!empty() && "Range has back");
    return *(m_begin + ((m_size - 1) * m_stride));
}


// Slicing (index based)
template<typename T>
strided_array_view<T> strided_array_view<T>::slice(size_t start_index) const
{
    return slice(start_index, size() - start_index);
}

template<typename T>
strided_array_view<T> strided_array_view<T>::slice(size_t start_index, size_t length) const
{
    assert(start_index <= size() && "Start index in range");
    assert(start_index + length <= size() && "Length in range");
    T* begin = m_begin + (start_index * m_stride);
    return strided_array_view<T>(begin, length, m_stride);
}

// Slicing (iterator based)
template<typename T>
strided_array_view<T> strided_array_view<T>::slice(iterator begin) const
{
    return slice(begin, end());
}

template<typename T>
strided_array_view<T> strided_array_view<T>::slice(iterator begin, iterator end) const
{
    assert((begin >= this->begin() && begin <= this->end()) && "Begin in range");
    assert((end >= this->begin() && end <= this->end() && end >= begin && "End in range"));
    return strided_array_view<T>(begin.m_item, end.m_item, m_stride);
}

template<typename T>
strided_array_view<T>::operator strided_array_view<const T>() const
{
    return strided_array_view<const T>(m_begin, m_size,  m_stride);
}

} // namespace bew

#endif // MP_TOOLS_STRIDED_ARRAY_VIEW_HPP

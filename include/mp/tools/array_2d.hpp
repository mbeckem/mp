#ifndef MP_TOOLS_2D_ARRAY_HPP
#define MP_TOOLS_2D_ARRAY_HPP

#include "../defs.hpp"
#include "array_view.hpp"
#include "strided_array_view.hpp"

namespace mp {

/*
 * A class that stores a two-dimensional array with a number of rows and columns.
 * Cells are stored in row major order (in a contiguous memory block).
 *
 * array_2d offers methods to create views over a given row or column,
 * e.g. row(0) returns an _unowned_ view over the first row of size "columns".
 */
template<typename T>
class array_2d
{
public:
    using iterator          = T*;
    using const_iterator    = const T*;
    using reference         = T&;
    using const_reference   = const T&;
    using row_view          = array_view<T>;
    using const_row_view    = array_view<const T>;
    using column_view       = strided_array_view<T>;
    using const_column_view = strided_array_view<const T>;

public:
    array_2d();
    array_2d(size_t rows, size_t columns);
    array_2d(size_t rows, size_t columns, const T &default_value);
    // size of v must be rows * columns.
    array_2d(vector<T> v, size_t rows, size_t columns);

    bool   empty()      const { return m_data.empty(); }
    // Returns the number of cells (== rows() * columns()).
    size_t cells()      const { return m_data.size(); }

    // Returns the number of rows.
    size_t rows()       const { return m_rows; }

    // Returns the number of columns.
    size_t columns()    const { return m_columns; }

    iterator begin()    { return m_data.data(); }
    iterator end()      { return m_data.data() + m_data.size(); }

    const_iterator begin() const { return m_data.data(); }
    const_iterator end()   const { return m_data.data() + m_data.size(); }

    reference    cell(size_t index);
    reference    cell(size_t row, size_t column);
    row_view     row(size_t index);
    column_view  column(size_t index);

    const_reference    cell(size_t index) const;
    const_reference    cell(size_t row, size_t column) const;
    const_row_view     row(size_t index) const;
    const_column_view  column(size_t index) const;

    void resize(size_t rows, size_t columns);
    void resize(size_t rows, size_t columns, const T& default_value);

    T* data() { return m_data.data(); }

    const T* data() const { return m_data.data(); }

private:
    size_t m_rows;
    size_t m_columns;
    vector<T> m_data;
};

template<typename T>
bool operator ==(const array_2d<T> &a, const array_2d<T> &b)
{
    return a.columns() == b.columns()
            && a.rows() == b.rows()
            && std::equal(a.begin(), a.end(), b.begin());
}

template<typename T>
bool operator !=(const array_2d<T> &a, const array_2d<T> &b)
{
    return !(a == b);
}

template<typename T>
array_2d<T>::array_2d()
    : m_rows(0), m_columns(0), m_data()
{}

template<typename T>
array_2d<T>::array_2d(size_t rows, size_t columns)
    : m_rows(rows), m_columns(columns), m_data(rows * columns)
{}

template<typename T>
array_2d<T>::array_2d(size_t rows, size_t columns, const T& default_value)
    : m_rows(rows), m_columns(columns), m_data(rows * columns, default_value)
{}

template<typename T>
array_2d<T>::array_2d(vector<T> v, size_t rows, size_t columns)
    : m_rows(rows)
    , m_columns(columns)
    , m_data(std::move(v))
{
    assert(m_data.size() == rows * columns && "Vector has correct input size");
}

template<typename T>
typename array_2d<T>::reference array_2d<T>::cell(size_t index)
{
    assert(index < m_data.size() && "Index in range");
    return m_data[index];
}

template<typename T>
typename array_2d<T>::reference array_2d<T>::cell(size_t row, size_t column)
{
    assert(row < m_rows && "Row index in range");
    assert(column < m_columns && "Column index in range");
    return m_data[row * m_columns + column];
}

template<typename T>
typename array_2d<T>::row_view array_2d<T>::row(size_t index)
{
    assert(index < m_rows && "Row index in range");
    return {m_data.data() + index * m_columns, m_columns};
}

template<typename T>
typename array_2d<T>::column_view array_2d<T>::column(size_t index)
{
    assert(index < m_columns && "Column index in range");
    return {m_data.data() + index, m_rows, m_columns};
}

template<typename T>
typename array_2d<T>::const_reference array_2d<T>::cell(size_t index) const
{
    assert(index < m_data.size() && "Index in range");
    return m_data[index];
}

template<typename T>
typename array_2d<T>::const_reference array_2d<T>::cell(size_t row, size_t column) const
{
    assert(row < m_rows && "Row index in range");
    assert(column < m_columns && "Column index in range");
    return m_data[row * m_columns + column];
}

template<typename T>
typename array_2d<T>::const_row_view array_2d<T>::row(size_t index) const
{
    assert(index < m_rows && "Row index in range");
    return {m_data.data() + index * m_columns, m_columns};
}

template<typename T>
typename array_2d<T>::const_column_view array_2d<T>::column(size_t index) const
{
    assert(index < m_columns && "Column index in range");
    return {m_data.data() + index, m_rows, m_columns};
}

template<typename T>
void array_2d<T>::resize(size_t rows, size_t columns)
{
    m_data.resize(rows * columns);
    m_rows = rows;
    m_columns = columns;
}

template<typename T>
void array_2d<T>::resize(size_t rows, size_t columns, const T &default_value)
{
    m_data.resize(rows * columns, default_value);
    m_rows = rows;
    m_columns = columns;
}

} // namespace bew

#endif // MP_TOOLS_2D_ARRAY_HPP

#ifndef MP_DTW_HPP
#define MP_DTW_HPP

#include <cmath>

#include "defs.hpp"
#include "tools/array_2d.hpp"
#include "tools/array_view.hpp"

namespace mp {

/**
 * Computes the dynamic time warp for two series of data.
 *
 * Stores a working buffer for invocations of run().
 */
class dtw
{
public:
    /**
     * Constructs an instance of dtw.
     * Both parameters must be > 0.
     */
    dtw(size_t a_size, size_t b_size)
        : buffer(a_size, b_size)
    {
        assert(a_size > 0);
        assert(b_size > 0);
    }

    /**
     * run() computes the dynamic time warp cost matrix for two input
     * vectors `a` and `b` and returns the DTW-cost of warping them.
     *
     * Dynamic time warping uses a distance function to calculate the cost between
     * two elements of `a` and `b`.
     *
     * The types `VectorA` and `VectorB` must fulfill the following requirements:
     * - a `.size()` function returning an integral value >= 0 (e.g. a `size_t`)
     * - an `operator[]` that returns a value or (const-) reference to an element at index `i`,
     *   where 0 <= i < size().
     *
     * `a` and `b` must have the length specified in the
     * constructor to dtw.
     * `d` must be a distance function that takes elements of `a` and `b` (in that order)
     * as arguments and returns a double >= 0.
     */
    template<typename VectorA, typename VectorB, typename Distance>
    double run(const VectorA &a, const VectorB &b, Distance &&d)
    {
        const size_t n = a.size();
        const size_t m = b.size();

        assert(n == buffer.rows());
        assert(m == buffer.columns());

        // Returns reference to item at data[row, col]
        auto at = [&](size_t row, size_t col) -> double& {
            assert(row < n && "Valid row index");
            assert(col < m && "Valid col index");
            return buffer.cell(row, col);
        };
        auto min = [](double a, double b, double c) {
            return std::min(a, std::min(b, c));
        };

        // Initialization step.
        // We do not need to zero the whole matrix since the algorithm
        // will only access elements which have previously been written.
        at(0, 0) = d(a[0], b[0]);
        for (size_t i = 1; i < n; ++i) {
            at(i, 0) = d(a[i], b[0]) + at(i - 1, 0);
        }
        for (size_t j = 1; j < m; ++j) {
            at(0, j) = d(a[0], b[j]) + at(0, j - 1);
        }

        for (size_t i = 1; i < n; ++i) {
            for (size_t j = 1; j < m; ++j) {
                at(i, j) = d(a[i], b[j]) + min(at(i - 1, j),
                                               at(i, j - 1),
                                               at(i - 1, j - 1));
            }
        }

        return at(n - 1, m - 1);
    }

    /**
     * Returns the cost matrix for the last computation of DTW.
     */
    const array_2d<double>& cost_matrix() const { return buffer; }

    /**
     * Returns the warp path for the last computation of DTW.
     */
    vector<tuple<size_t, size_t>> warp_path() const
    {
        vector<tuple<size_t, size_t>> v;
        v.reserve(buffer.rows() + buffer.columns());
        warp_path(v);
        return v;
    }

    /**
     * Stores the warp path for the last computation of DTW in `v`.
     * Will clear the vector and reuse its capacity.
     */
    void warp_path(vector<tuple<size_t, size_t>> &v) const;

    // Copies shouldn't be required
    dtw(const dtw &) = delete;
    dtw& operator =(const dtw&) = delete;

private:
    // A reusable buffer that survives invocations
    // of run().
    array_2d<double> buffer;
};

/**
 * Manhattan distance: 1 dimensional case
 */
inline double manhattan_distance_1(double a, double b)
{
    return std::abs(a - b);
}

/**
 * Manhattan distance: n-dimensional case
 */
inline double manhattan_distance(array_view<const double> a,
                                 array_view<const double> b)
{
    assert(a.size() == b.size() && "Vectors must have the same size");
    assert(!a.empty() && "Vectors must not be empty");

    const size_t n = a.size();

    double d = 0.0;
    for (size_t i = 0; i < n; ++i) {
        d += manhattan_distance_1(a[i], b[i]);
    }
    return d;
}

/**
 *  Euclidean distance for n-dimensional vectors.
 */
inline double euclidean_distance(array_view<const double> a,
                                 array_view<const double> b)
{
    assert(a.size() == b.size() && "Vectors must have the same size");
    assert(!a.empty() && "Vectors must not be empty");

    const size_t n = a.size();

    double d = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double diff = a[i] - b[i];
        d += diff * diff;
    }
    return std::sqrt(d);
}

} // namespace mp

#endif // MP_DTW_HPP

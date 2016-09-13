#include "mp/feature_computation.hpp"

#include <algorithm>
#include <limits>
#include <thread>
#include <iostream>

#include "mp/metrics.hpp"
#include "mp/parser.hpp"
#include "mp/tracing_data.hpp"
#include "mp/tools/array_view.hpp"

namespace mp {

using pair_list = vector<tuple<i32, i32>>;

namespace {

/*
 * Computes the similarity_data for some tracing_data input.
 * Uses the Similarity type to provide the actual similarity values
 * for two devices at some fixed timestamp.
 * Will instanciate exactly one Similarity object for each thread.
 */
template<typename Similarity>
struct similarity_computation
{
public:
    similarity_computation(const tracing_data &td,
                           const pair_list &pairs,
                           const feature_computation &settings,
                           similarity_data &result)
        : td(td)
        , pairs(pairs)
        , time_lag(settings.time_lag)
        , window_size(settings.window_size)
        , threads(settings.threads)
        , begin_timestamp(settings.begin_timestamp)
        , end_timestamp(settings.end_timestamp)
        , duration(end_timestamp - begin_timestamp + 1)
        , result(result)
    {}

    void run()
    {
        assert(td.devices.size() > 0);
        assert(td.data_dimension > 0);
        assert(td.duration > 0);
        assert(threads > 0);
        assert(begin_timestamp <= end_timestamp);

        num_devices       = td.devices.size();
        num_pairs         = pairs.size();
        feature_dimension = time_lag * 2 + 1;

        result.begin_timestamp   = begin_timestamp;
        result.end_timestamp     = end_timestamp;
        result.duration          = end_timestamp - begin_timestamp + 1;
        result.feature_dimension = feature_dimension;
        result.devices.reserve(td.devices.size());
        for (auto &dev : td.devices) {
            result.devices.push_back(dev.name);
        }

        // Initialize every pair.
        result.pairs.resize(num_pairs);
        for (i32 i = 0; i < num_pairs; ++i) {
            auto &npair = result.pairs[i];
            auto &opair = pairs[i];

            npair.left = get<0>(opair);
            npair.right = get<1>(opair);
            npair.features.resize(duration, feature_dimension, 0.0);

            assert(npair.left >= 0 && npair.left < num_devices);
            assert(npair.right >= 0 && npair.right < num_devices);
        }

        i32 threads_used = std::min(threads, num_pairs);
        if (threads_used > 1) {
            run_parallel(threads_used);
        } else {
            run_single();
        }
    }

private:
    // Run the similarity algorithm in parallel using "threads_used" threads.
    // The set of all pairs will be partioned into blocks of (approx.) equal size
    // and computed in parallel.
    void run_parallel(i32 threads_used)
    {
        assert(threads_used > 0);

        // Executed by every worker thread:
        auto thread_func = [&](size_t offset, size_t end) {
            // Every thread gets its own Similarity instance because they are not thread-safe
            // (e.g. dtw uses an internal, mutable buffer).
            Similarity sim(static_cast<const similarity_computation&>(*this));

            for (size_t i = offset; i != end; ++i) {
                assert(offset < result.pairs.size());

                auto &pair = result.pairs[i];
                auto &left = td.devices[pair.left];
                auto &right = td.devices[pair.right];
                compute_pair(sim, left, right, pair);
            }
        };

        size_t offset     = 0;
        size_t chunk_size = num_pairs / threads_used;
        assert(chunk_size > 0);

        // Thread i computes the i-th chunk of size "chunk_size".
        vector<std::thread> threads;
        for (i32 i = 0; i < threads_used - 1; ++i) {
            size_t end = offset + chunk_size;
            threads.push_back(std::thread(thread_func, offset, end));
            offset = end;
        }
        // This thread computes the last block, which may be of
        // a different size.
        thread_func(offset, result.pairs.size());

        for (auto &t : threads) {
            t.join();
        }
    }

    void run_single()
    {
        // A simplified version of the above
        Similarity sim(static_cast<const similarity_computation&>(*this));
        for (auto &pair : result.pairs) {
            auto &left = td.devices[pair.left];
            auto &right = td.devices[pair.right];
            compute_pair(sim, left, right, pair);
        }
    }

    // Take the [timestamp -> location/signal-strength] table for two devices a, b
    // and compute the feature vector v_{a,b} for every timestamp.
    void compute_pair(Similarity &sim,
                      const tracing_data::device_data &left,
                      const tracing_data::device_data &right,
                      similarity_data::pair_data &pair)
    {
        for (i64 ts = begin_timestamp; ts <= end_timestamp; ++ts) {
            auto result_row = result.feature_at(pair, ts);

            i32 lag = -time_lag;
            for (size_t i = 0; lag <= time_lag; ++lag, ++i) {
                result_row[i] = sim.compute_similarity(ts, lag, left, right);
            }
        }
    }

public:
    const tracing_data &td;
    const pair_list &pairs;
    const i32 time_lag;
    const i32 window_size;
    const i32 threads;
    const i64 begin_timestamp;
    const i64 end_timestamp;
    const i64 duration;
    similarity_data &result;

private:
    i32 num_devices;
    i32 num_pairs;
    i32 input_dimension;
    i32 feature_dimension;
};

// Keep timestamps in range.
// This has the effect of treating devices like they didn't move if the index
// goes out of bounds.
i64 timestamp_bounds(i64 min, i64 max, i64 v)
{
    if (v < min) {
        return min;
    } else if (v > max) {
        return max;
    } else {
        return v;
    }
}

// Similar to the function above, but also ensure that
// all timestamps in the range [v, v+length) are valid with
// respect to min and max.
i64 timestamp_range_bounds(i64 min, i64 max, i64 v, i64 length)
{
    assert(max - min + 1 >= length);
    if (v < min) {
        return min;
    } else if (v + length - 1 > max) {
        return max - length + 1;
    } else {
        return v;
    }
}

// Computes the similarity of two time-lagged sequences using the euclidean distance metric.
class euclid_similarity
{
public:
    using context = similarity_computation<euclid_similarity>;

public:
    euclid_similarity(const context &ctx)
        : td(ctx.td)
        , data_dimension(td.data_dimension)
        , window_size(ctx.window_size)
        , half_window_size(ctx.window_size / 2)
        , inverse_window_size(1.0 / window_size)
        , left_buf(ctx.td.data_dimension)
        , right_buf(ctx.td.data_dimension)
    {}

    // Called by context class via static dispatch.
    double compute_similarity(i64 ts, i32 lag,
                              const tracing_data::device_data &left,
                              const tracing_data::device_data &right)
    {
        auto ts_bounds = [&](i64 i) {
            return timestamp_bounds(td.min_timestamp, td.max_timestamp, i);
        };

        auto left_has_data = td.has_data_at(left, ts);
        auto right_has_data = td.has_data_at(right, ts_bounds(ts + lag));

        double result = 0.0;
        i64 left_timestamp  = ts - (half_window_size);
        i64 right_timestamp = left_timestamp + lag;
        for (i32 j = 0; j < window_size; ++j, ++left_timestamp, ++right_timestamp) {
            auto left_data  = td.data_at(left, ts_bounds(left_timestamp));
            auto right_data = td.data_at(right, ts_bounds(right_timestamp));

            // Get a vector of viable measurements for both devices.
            i32 n = 0;
            for (i32 c = 0; c < data_dimension; ++c) {
                // Union of available access points.
                if (left_has_data[c] || right_has_data[c]) {
                    left_buf[n] = left_data[c];
                    right_buf[n] = right_data[c];
                    ++n;
                }
            }

            array_view<const double> lview(left_buf.data(), n);
            array_view<const double> rview(right_buf.data(), n);
            result += euclidean_distance(lview, rview);
        }
        return result * inverse_window_size;
    }

private:
    const tracing_data &td;
    const i32       data_dimension;
    const i32       window_size;
    const i32       half_window_size;
    const double    inverse_window_size;

    vector<double> left_buf;
    vector<double> right_buf;
};

// Computes the similarity of two time-lagged sequences using the Dynamic Time Warp algorithm.
class dtw_similarity
{
public:
    using context = similarity_computation<dtw_similarity>;

public:
    dtw_similarity(const context &ctx)
        : td(ctx.td)
        , window_size(ctx.window_size)
        , half_window_size(ctx.window_size / 2)
        , input_dimension(ctx.td.data_dimension)
        , norm_factor(1 / (2.0 * window_size))
        , d(window_size, window_size)
        , left_buf(window_size)
        , right_buf(window_size)
    {
        assert(ctx.td.duration >= window_size
               && "At least window_size timestamps");
    }

    // Called by context class via static dispatch.
    double compute_similarity(i64 ts, i32 lag,
                              const tracing_data::device_data &left,
                              const tracing_data::device_data &right)
    {
        auto ts_bounds = [&](i64 i) {
            return timestamp_bounds(td.min_timestamp, td.max_timestamp, i);
        };
        auto ts_range = [&](i64 i) {
            return timestamp_range_bounds(td.min_timestamp, td.max_timestamp,
                                          i, window_size);
        };

        auto left_has_data = td.has_data_at(left, ts);
        auto right_has_data = td.has_data_at(right, ts_bounds(ts + lag));

        // First timestamp in left/right sequence (length is window_size).
        i64 left_ts = ts - half_window_size;
        i64 right_ts = left_ts + lag;
        left_ts = ts_range(left_ts);
        right_ts = ts_range(right_ts);

        i32 n = 0; // number of data columns used
        double result = 0.0;
        // Iterate column-wise. Every column represents a time series
        // of values for that specific dimension (e.g. signal strength
        // for *one* access point or *one* location coordiante over time).
        for (i32 col = 0; col < input_dimension; ++col) {
            // Union of available access points at the current timestamp.
            if (left_has_data[col] || right_has_data[col]) {
                for (i32 j = 0; j < window_size; ++j) {
                    left_buf[j] = td.data_at(left, left_ts + j)[col];
                    right_buf[j] = td.data_at(right, right_ts + j)[col];
                }
                result += d.run(array_view<const double>(left_buf),
                                array_view<const double>(right_buf),
                                manhattan_distance_1);
                ++n;
            }
        }
        // To normalize the dtw cost, divide it by (n + m)
        // where n and m are the vector length (both == window_size here).
        result *= norm_factor;
        result /= n;
        return result;
    }

private:
    const tracing_data &td;
    const i32       window_size;
    const i32       half_window_size;
    const i32       input_dimension;
    const double    norm_factor;

    dtw d;
    vector<double> left_buf;
    vector<double> right_buf;
};

// Computes the multi-dimensional DTW.
// Instead of computing the DTW for every column in the input data
// on its own, this algorithm takes all columns at once.
// Every row of measurements is treated like an n-dimensional
// measurement vector and thus a sequence of rows becomes
// an n-dimensional time series of measurements, suitable for DTW.
struct multi_dtw_similarity
{
public:
    using context = similarity_computation<multi_dtw_similarity>;

public:
    multi_dtw_similarity(const context &ctx)
        : td(ctx.td)
        , data_dimension(td.data_dimension)
        , window_size(ctx.window_size)
        , half_window_size(ctx.window_size / 2)
        , norm_factor(1.0 / (2 * window_size))
        , d(window_size, window_size)
        , left_buf(window_size, data_dimension)
        , right_buf(window_size, data_dimension)
    {
        assert(ctx.td.duration >= window_size
               && "At least window_size timestamps");
    }

    // Wraps an array_2d, but restricts the number of columns.
    struct table_view
    {
        size_t size() const { return m_data.rows(); }

        array_view<const double> operator [](size_t i) const
        {
            assert(i < m_data.rows());
            // Row size is restricted since the buffer is preallocated
            // but the number of actually available dimensions may be lower than that.
            auto row = m_data.row(i);
            assert(row.size() >= m_columns);
            return row.slice(0, m_columns);
        }

        const array_2d<double> &m_data;
        size_t m_columns;
    };

    // Called by context class via static dispatch.
    double compute_similarity(i64 ts, i32 lag,
                              const tracing_data::device_data &left,
                              const tracing_data::device_data &right)
    {
        auto ts_bounds = [&](i64 i) {
            return timestamp_bounds(td.min_timestamp, td.max_timestamp, i);
        };
        auto ts_range = [&](i64 i) {
            return timestamp_range_bounds(td.min_timestamp, td.max_timestamp,
                                          i, window_size);
        };

        auto has_left_data = td.has_data_at(left, ts);
        auto has_right_data = td.has_data_at(right, ts_bounds(ts + lag));

        i64 left_ts = ts - half_window_size;
        i64 right_ts = left_ts + lag;
        left_ts = ts_range(left_ts);
        right_ts = ts_range(right_ts);

        // Assemble a measurement matrix without unviable columns.
        // TODO: This is called very often and does column-wise iteration. Measure.
        size_t n = 0;
        for (i32 col = 0; col < data_dimension; ++col) {
            if (has_left_data[col] || has_right_data[col]) {
                for (i32 j = 0; j < window_size; ++j) {
                    left_buf.cell(j, n) = td.data_at(left, left_ts + j)[col];
                    right_buf.cell(j, n) = td.data_at(right, right_ts + j)[col];
                }
                ++n;
            }
        }

        table_view left_view{left_buf, n};
        table_view right_view{right_buf, n};
        return norm_factor * d.run(left_view, right_view, euclidean_distance);
    }

private:
    const tracing_data &td;
    const i32 data_dimension;
    const i32 window_size;
    const i32 half_window_size;
    const double norm_factor;

    dtw d;
    array_2d<double> left_buf;
    array_2d<double> right_buf;
};


template<typename Func>
void check(bool cond, Func &&otherwise)
{
    if (!cond) {
        otherwise();
    }
}

template<typename Similarity>
similarity_data run_similarity(const tracing_data &td,
                               const pair_list &pairs,
                               const feature_computation &settings)
{
    check(settings.time_lag >= 0,   []{ throw std::logic_error("Time lag must be >= 0"); });
    check(settings.window_size > 0, []{ throw std::logic_error("Window size must be > 0"); });
    check(settings.threads > 0,     []{ throw std::logic_error("Thread count must be > 0"); });
    check(td.duration >= settings.window_size + settings.time_lag,
          []{ throw std::logic_error("Must at least provide time lag + window size measurements"); });
    check(settings.begin_timestamp >= td.min_timestamp,
          []{ throw std::logic_error("Begin timestamp must be in range of source data"); });
    check(settings.end_timestamp >= settings.begin_timestamp,
          []{ throw std::logic_error("End timestamp must be >= begin timestamp"); });
    check(settings.end_timestamp <= td.max_timestamp,
          []{ throw std::logic_error("End timestamp must be in range of source data"); });

    similarity_data result;
    similarity_computation<Similarity> comp(td, pairs, settings, result);
    comp.run();
    return result;
}

} // namespace

similarity_data feature_computation::compute_euclid(const tracing_data &td,
                                                    const pair_list &pairs)
{
    return run_similarity<euclid_similarity>(td, pairs, *this);
}

similarity_data feature_computation::compute_dtw(const tracing_data &td,
                                                 const pair_list &pairs)
{
    return run_similarity<dtw_similarity>(td, pairs, *this);
}

similarity_data feature_computation::compute_multi_dtw(const tracing_data &td,
                                                       const pair_list &pairs)
{
    return run_similarity<multi_dtw_similarity>(td, pairs, *this);
}

} // namespace mp

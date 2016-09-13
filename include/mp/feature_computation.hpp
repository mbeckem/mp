#ifndef MP_FOLLOWING_FEATURE_HPP
#define MP_FOLLOWING_FEATURE_HPP

#include "defs.hpp"
#include "tools/array_view.hpp"
#include "tools/array_2d.hpp"
#include "serialization.hpp"

namespace mp {

class signal_data;
class tracing_data;

/**
 * Stores feature vectors for every device pair and every timestamp.
 */
struct similarity_data
{
    /**
     * Stores feature vectors for a single pair.
     */
    struct pair_data
    {
        i32 left;  ///< Index into the \p devices vector.
        i32 right; ///< Same.
        /**
         * A matrix of feature values.
         * One row for every second in the source data.
         * Every row represents the feature vector at that second,
         * i.e. the vector v_{a,b} in the original paper.
         * (a and b are the devices of the current pair).
         */
        array_2d<double> features;
    };

    /**
     * Returns the feature vector for the given pair at the given timestamp.
     */
    array_view<double> feature_at(pair_data &pair, i64 timestamp) const
    {
        assert(timestamp >= begin_timestamp && timestamp <= end_timestamp
               && "Timestamp in range");
        return pair.features.row(timestamp - begin_timestamp);
    }

    /**
     * Returns the feature vector for the given pair at the given timestamp.
     */
    array_view<const double> feature_at(const pair_data &pair, i64 timestamp) const
    {
        assert(timestamp >= begin_timestamp && timestamp <= end_timestamp
               && "Timestamp in range");
        return pair.features.row(timestamp - begin_timestamp);
    }

    i64 begin_timestamp;        ///< The first timestamp of the data.
    i64 end_timestamp;          ///< The last timestamp of the data (inclusive).
    i64 duration;               ///< The number of timestamps.
    i32 feature_dimension;      ///< Number of columns in pair_data::features (== length of feature vector).
    vector<string> devices;     ///< All device names.
    vector<pair_data> pairs;    ///< One entry for every pair.
};

/**
 * This class produces similarity_data.
 *
 * The algorithm that assigns similarity rankings
 * for device pairs can be expressed as a nested loop:
 *
 * <pre>
 *      foreach device-pair p
 *          foreach timestamp t
 *              compute similarity based on (location/signal) data around t
 * </pre>
 *
 * \param time_lag
 *      For evey time step, the algorithm will take a lag in the range (-time_lag, ..., time_lag)
 *      into account. time_lag must be >= 0.
 *      If time_lag is 0, only one (non-time-lagged) feature value will be computed.
 *      time_lag is called 'z' in the original paper.
 *
 * \param window_size
 *      For every similarity computation, the algorithm will visit window_size data points
 *      (w/2 into the "past" and w/2 into the "future"). window_size must be greater than 0.
 *      window_size is called 'w' in the original paper.
 *
 * \param threads
 *      Number of threads to utilize.
 *
 * \param begin_timestamp, end_timestamp
 *      The timestamp at which the experiment begins (and ends).
 *      The algorithm will compute a feature vector for every second
 *      between begin_timestamp and end_timestamp (inclusive).
 *      Note that the input data must store a measurement for every second.
 *
 * \note
 *      The caller must provide at least time_lag + window_size data points as input.
 *
 */
struct feature_computation
{
    i32 time_lag = 5;
    i32 window_size = 10;
    i32 threads = 1;
    i64 begin_timestamp = 0;    // inclusive
    i64 end_timestamp = 0;      // inclusive

    /**
     * Computes the similarity data for the given tracing_data and list of pairs.
     * The list of pairs must refer to indices into tracing_data::devices.
     *
     * The result will store similarity data for every pair and every timestamp in
     * the range `[begin_timestamp, end_timestamp]`.
     */
    similarity_data compute_euclid(const tracing_data &td, const vector<tuple<i32, i32>> &pairs);

    /**
     * Uses the multi-dtw algorithm. \sa feature_computation::compute_euclid.
     */
    similarity_data compute_multi_dtw(const tracing_data &td, const vector<tuple<i32, i32>> &pairs);

    /**
     * Uses the dtw algorithm. \sa feature_computation::compute_euclid.
     */
    similarity_data compute_dtw(const tracing_data &td, const vector<tuple<i32, i32>> &pairs);
};

/**
 * Serialize a pair using the given archive.
 *
 * \relates similarity_data::pair_data
 */
template<typename Archive>
void serialize(Archive &ar, similarity_data::pair_data &p)
{
    ar(cereal::make_nvp("left", p.left),
       cereal::make_nvp("right", p.right),
       cereal::make_nvp("features", p.features));
}

/**
 * Serialize similarity data using the given archive.
 *
 * \relates similarity_data
 */
template<typename Archive>
void serialize(Archive &ar, similarity_data &sim)
{
    ar(cereal::make_nvp("begin_timestamp", sim.begin_timestamp),
       cereal::make_nvp("end_timestamp", sim.end_timestamp),
       cereal::make_nvp("duration", sim.duration),
       cereal::make_nvp("feature_dimension", sim.feature_dimension),
       cereal::make_nvp("devices", sim.devices),
       cereal::make_nvp("pairs", sim.pairs));
}

} // namespace mp

#endif // MP_FOLLOWING_FEATURE_HPP

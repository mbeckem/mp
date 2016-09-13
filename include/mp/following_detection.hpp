#ifndef MP_FOLLOWING_DETECTION_HPP
#define MP_FOLLOWING_DETECTION_HPP

#include "defs.hpp"
#include "serialization.hpp"
#include "tools/array_view.hpp"

namespace mp {

class similarity_data;
class co_moving_classifier;

/**
 * The type of a detected following relation.
 */
enum class following_type
{
    following,  ///< first device follows the second one
    leading,    ///< first device leads the second one
    co_leading, ///< they move next to each other
};

class time_lag_estimation
{
public:
    // Takes the time lag parameter (>= 0).
    time_lag_estimation(i32 time_lag);

    // Takes a feature vector "fv" and returns the estimated
    // time lag between the two original devices that belong to "fv".
    // "fv" should already be classified as co-moving.
    double estimate_lag_simple(array_view<const double> fv);

    // Same as the above, but with a more complex estimation scheme.
    // This implements the formula in section 4.1 of the original paper.
    double estimate_lag_complex(array_view<const double> fv);

    // Take an estimated time lag value returned by one of
    // the estimation functions and return a following_type.
    following_type get_following_type(double est_lag);

private:
    i32 m_min_lag;
    i32 m_max_lag;
    i32 m_size;
};

/**
 * Following data stores every detected following-relation for every timestamp.
 */
struct following_data
{
    /**
     * A pair that has been classified as co-moving.
     * If the detected type is `following`, then `left` is following `right`.
     * If it is leading, `left` is leading `right`.
     * For `co_leading`, both devices are considered leading.
     */
    struct pair_data
    {
        i32 left;               ///< Index into devices vector.
        i32 right;              ///< Index into devices vector.
        double lag;             ///< Estimated time lag.
        following_type type;    ///< The type of the relation.
    };

    /**
     * Data for a single timestamp.
     * Stores a list of co-moving device pairs.
     */
    struct timestamp_data
    {
        i64 timestamp = 0;
        // List of co-moving devices.
        // Using a vector because the list of pairs
        // is expected to be quite small.
        // Could also use a table or hash map approach.
        vector<pair_data> co_moving;

        timestamp_data() {}
        timestamp_data(i64 ts, vector<pair_data> cm)
            : timestamp(ts)
            , co_moving(std::move(cm))
        {}
    };

    /**
     * Returns the data for the given timestamp.
     */
    timestamp_data& data_at(i64 timestamp)
    {
        assert(timestamp >= begin_timestamp && timestamp <= end_timestamp);
        size_t index = static_cast<size_t>(timestamp - begin_timestamp);
        return timestamps[index];
    }

    /**
     * Returns the data for the given timestamp.
     */
    const timestamp_data& data_at(i64 timestamp) const
    {
        assert(timestamp >= begin_timestamp && timestamp <= end_timestamp);
        size_t index = static_cast<size_t>(timestamp - begin_timestamp);
        return timestamps[index];
    }

    i64 begin_timestamp;
    i64 end_timestamp;
    i64 duration;
    vector<string> devices;

    // For every timestamp,
    // we store a list of pairs that are
    // classified as co-moving.
    vector<timestamp_data> timestamps;
};

/**
 * Classifies the given similarity_data using the given classifier.
 *
 * Returns the detected following and leadership patterns for every timestamp and every
 * device pair.
 *
 * \relates following_data
 */
following_data classify(co_moving_classifier &c, const similarity_data &data);

/**
 * Save a following_type using the given archive.
 *
 */
template<typename Archive>
string save_minimal(const Archive &, const following_type type)
{
    using integer_type = typename std::underlying_type<following_type>::type;

    switch (type) {
    case following_type::following:
        return "following";
    case following_type::leading:
        return "leading";
    case following_type::co_leading:
        return "co_leading";
    }
    throw std::runtime_error("invalid type: "
                             + std::to_string(static_cast<integer_type>(type)));
}

/**
 * Load a following_type using the given archive.
 */
template<typename Archive>
void load_minimal(const Archive &, following_type &type, const string &value)
{
    if (value == "following") {
        type = following_type::following;
    } else if (value == "leading") {
        type = following_type::leading;
    } else if (value == "co_leading") {
        type = following_type::co_leading;
    } else {
        throw std::runtime_error("invalid type: " + value);
    }
}

/**
 * Serialize pair data using the given archive.
 *
 * \relates following_data::pair_data
 */
template<typename Archive>
void serialize(Archive &ar, following_data::pair_data &p)
{
    ar(cereal::make_nvp("left", p.left),
       cereal::make_nvp("right", p.right),
       cereal::make_nvp("lag", p.lag),
       cereal::make_nvp("type", p.type));
}

/**
 * Serialize timestamp data using the given archive.
 *
 * \relates following_data::timestamp_data
 */
template<typename Archive>
void serialize(Archive &ar, following_data::timestamp_data &t)
{
    ar(cereal::make_nvp("timestamp", t.timestamp),
       cereal::make_nvp("co_moving", t.co_moving));
}

/**
 * Serialize following data using the given archive.
 *
 * \relates following_data
 */
template<typename Archive>
void serialize(Archive &ar, following_data &f)
{
    ar(cereal::make_nvp("begin_timestamp", f.begin_timestamp),
       cereal::make_nvp("end_timestamp", f.end_timestamp),
       cereal::make_nvp("duration", f.duration),
       cereal::make_nvp("devices", f.devices),
       cereal::make_nvp("timestamps", f.timestamps));

    assert(f.begin_timestamp <= f.end_timestamp);
    assert(f.duration == f.end_timestamp - f.begin_timestamp + 1);
}

} // namespace mp

#endif // MP_FOLLOWING_DETECTION_HPP

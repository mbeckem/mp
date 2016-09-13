#ifndef MP_SIGNAL_DATA_HPP
#define MP_SIGNAL_DATA_HPP

#include "defs.hpp"

namespace mp {

/**
 * For each known device, this structure contains a vector
 * of measurements.
 * A measurement entry contains the signal strength of an access_point
 * at a specific point in time (the 'timestamp' attribute).
 */
struct signal_data
{
    /**
     * A single signal measurement.
     */
    struct measurement
    {
        i64 timestamp;       ///< unix timestamp (seconds)
        i32 access_point_id; ///< index into bssids
        i32 signal_strength; ///< dBm
    };

    /**
     * All measurements of a single device.
     */
    struct device_data
    {
        device_data(string name)
            : name(std::move(name))
        {}
        device_data(string name, vector<measurement> data)
            : name(std::move(name))
            , data(std::move(data))
        {}

        /// unique identifier
        string name;

        /// Sorted by timestamp, ascending.
        /// Multiple measurements per access point and timestamp are possible,
        /// as are missing values.
        vector<measurement> data;
    };

    vector<string>      bssids;     ///< names of access points
    vector<device_data> devices;    ///< list of devices
};

/**
 * Finds access points in the input data set that have an average signal strenth
 * below the given minimum average.
 * The result vector will hold those access points' indices.
 * If an access point has no measurements, it will be treated like a "bad" one.
 *
 * \relates signal_data
 */
vector<i32> bad_access_points(const signal_data &sd, double minimum_average);

/**
 * Removes all measurements for every access point in the input vector
 * from the signal data set.
 * The access points will also be purged from the "bssids" name vector.
 * Since the access point ids of measurements point into this name vector,
 * these ids will become invalid and will be updated by this function.
 *
 * Access point ids in the input vector must be valid indices into "sd.bssids".
 *
 * \relates signal_data
 */
void remove_access_points(signal_data &sd, const vector<i32> &access_point_ids);

bool operator ==(const signal_data &a, const signal_data &b);
bool operator !=(const signal_data &a, const signal_data &b);

bool operator ==(const signal_data::measurement &a, const signal_data::measurement &b);
bool operator !=(const signal_data::measurement &a, const signal_data::measurement &b);

bool operator ==(const signal_data::device_data &a, const signal_data::device_data &b);
bool operator !=(const signal_data::device_data &a, const signal_data::device_data &b);

} // namespace mp

#endif // MP_SIGNAL_DATA_HPP

#ifndef MP_TRACING_DATA_HPP
#define MP_TRACING_DATA_HPP

#include "defs.hpp"
#include "tools/array_2d.hpp"

namespace mp {

class signal_data;
class location_data;

/**
 * Tracing data is an abstraction over both signal data and location data.
 * For every device, it stores a matrix with "duration" rows and "data_dimension" columns.
 * Every row contains the tracing data (either location or signal data) for a second
 * in the source data.
 */
struct tracing_data
{
    /**
     * Data for a single device.
     */
    struct device_data
    {
        string name; ///< unique device name.

        /**
         * One row for every time step,
         * Number of columns == data_dimension.
         */
        array_2d<double> data;

        /**
         * Same dimensions as `data`, but contains a 0
         * at `cell(i, j)` iff `data.cell(i, j)` was filled using
         * a default value (1 otherwise).
         * If the tracing data was created using signal data,
         * a value of 0 implies that no measurement for the column's
         * access point was available at that timestamp.
         */
         // Note that i use chars because array_2d interally uses a vector
         // and vector<bool> is completely broken.
        array_2d<char> has_data;
    };

    /**
     * Number of scalar values per time step,
     * e.g. 2 or 3 for spatial coordinates, or
     * one per known access point.
     */
    i32 data_dimension = 0;

    i64 min_timestamp = 0;  ///< first timestamp
    i64 max_timestamp = 0;  ///< last timestamp
    i64 duration = 0;       ///< max - min + 1 seconds

    vector<device_data> devices;  ///< list of devices.


    /**
     * Returns the data vector for the given device and timestamp.
     */
    array_view<double> data_at(device_data &device, i64 timestamp) const
    {
        assert(timestamp >= min_timestamp && timestamp <= max_timestamp
               && "Timestamp in range");
        return device.data.row(timestamp - min_timestamp);
    }

    /**
     * Returns the data vector for the given device and timestamp.
     */
    array_view<const double> data_at(const device_data &device, i64 timestamp) const
    {
        assert(timestamp >= min_timestamp && timestamp <= max_timestamp
               && "Timestamp in range");
        return device.data.row(timestamp - min_timestamp);
    }

    /**
     * Returns the has_data vector for the given device and timestamp.
     * \sa tracing_data::device_data.has_data for more information.
     */
    array_view<char> has_data_at(device_data &device, i64 timestamp)
    {
        assert(timestamp >= min_timestamp && timestamp <= max_timestamp
               && "Timestamp in range");
        return device.has_data.row(timestamp - min_timestamp);
    }

    /**
     * Returns the has_data vector for the given device and timestamp.
     * \sa tracing_data::device_data.has_data for more information.
     */
    array_view<const char> has_data_at(const device_data &device, i64 timestamp) const
    {
        assert(timestamp >= min_timestamp && timestamp <= max_timestamp
               && "Timestamp in range");
        return device.has_data.row(timestamp - min_timestamp);
    }

    /**
     * Returns the list of unique device index pairs.
     */
    vector<tuple<i32, i32>> unique_pairs() const
    {
        const i32 n = devices.size();

        vector<tuple<i32, i32>> result;
        for (i32 i = 0; i < n; ++i) {
            for (i32 j = i + 1; j < n; ++j) {
                result.push_back(make_tuple(i, j));
            }
        }
        return result;
    }
};

/**
 * Transforms the signal data into an object suitable as input
 * for the feature vector calculation algorithms.
 *
 * \param default_signal_strength
 *      This value will be chosen as the default signal strength
 *      if a measurement for an access point is missing for a
 *      timestamp (i.e. maximum distance is assumed).
 *
 * \relates tracing_data
 */
tracing_data transform(const signal_data &sd, i32 default_signal_strength);

/**
 * Does the same as the function above, but for location data.
 *
 * \relates tracing_data
 * \sa transform(const signal_data &, i32)
 */
tracing_data transform(const location_data &ld);

/**
 * Replaces the values at all timestamps and all
 * columns with their moving average at that point.
 * The moving average will be separately computed for every column.
 *
 * `n` is the number of values that are considered for each
 * average calculation (and must be positive).
 */
void moving_average(tracing_data &td, i32 n);

} // namespace mp

#endif // MP_TRACING_DATA_HPP

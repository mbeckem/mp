#include "mp/tracing_data.hpp"

#include <limits>

#include "mp/parser.hpp"

namespace mp {

namespace {

// Transforms a struct of type signal_data into valid tracing_data.
struct signal_data_transform
{
public:
    signal_data_transform(const signal_data &sd, i32 default_signal_strength,
                          tracing_data &result)
        : sd(sd)
        , default_signal_strength(default_signal_strength)
        , result(result)
    {}

    void run()
    {
        init();
        for (i32 i = 0; i < num_devices; ++i) {
            device_step(sd.devices[i], result.devices[i]);
        }
    }

private:
    void init()
    {
        num_devices = sd.devices.size();
        if (num_devices == 0) {
            throw std::runtime_error("no devices");
        }
        num_access_points = sd.bssids.size();
        if (num_access_points == 0) {
            throw std::runtime_error("no access points");
        }

        // Must iterate over every single entry
        // to find min/max timestamp.
        min_timestamp = std::numeric_limits<i64>::max();
        max_timestamp = std::numeric_limits<i64>::min();
        for (auto &dev : sd.devices) {
            for (auto &entry : dev.data) {
                min_timestamp = std::min(min_timestamp, entry.timestamp);
                max_timestamp = std::max(max_timestamp, entry.timestamp);
            }
        }
        if (max_timestamp < min_timestamp) {
            throw std::runtime_error("requries at least one measurement");
        }
        duration = max_timestamp - min_timestamp + 1;

        // Prepare memory for every device
        access_point_seen.resize(num_access_points, 0);
        result.devices.resize(num_devices);
        for (size_t i = 0; i < sd.devices.size(); ++i) {
            auto &dev = result.devices[i];

            dev.name = sd.devices[i].name;
            dev.data.resize(duration, num_access_points, 0.0);
            dev.has_data.resize(duration, num_access_points, 0);
        }

        result.data_dimension = num_access_points;
        result.min_timestamp  = min_timestamp;
        result.max_timestamp  = max_timestamp;
        result.duration       = duration;
    }

    // Compute the data matrix for the given device.
    void device_step(const signal_data::device_data &in, tracing_data::device_data &out)
    {
        array_view<double> row;         // one row <=> one time step
        array_view<char>   has_data;    // 1 iff was assigned actual measurement data instead of default value

        auto entry_begin = in.data.begin();
        auto entry_end   = in.data.end();
        auto entry_iter  = entry_begin;

        for (i64 ts = min_timestamp; ts <= max_timestamp; ++ts) {
            row = result.data_at(out, ts);
            has_data = result.has_data_at(out, ts);

            // Consider all entries for the current time point
            // (entries are sorted by timestamp).
            bool have_entries = false;
            for (; entry_iter != entry_end; ++entry_iter) {
                auto &entry = *entry_iter;
                assert(entry.timestamp >= min_timestamp
                       && entry.timestamp <= max_timestamp);
                assert(entry.access_point_id >= 0
                       && entry.access_point_id < num_access_points);

                if (entry.timestamp != ts) {
                    assert(entry.timestamp > ts
                           && "Entries are sorted");
                    break;
                }

                // There may be multiple signal strength values
                // per time point for a given access point.
                // We count the occurrences of the access point
                // and average the values on row finalization.
                ++access_point_seen[entry.access_point_id];
                row[entry.access_point_id] += entry.signal_strength;
                have_entries = true;
            }

            if (!have_entries) {
                // No entry for the current time point.
                // Assume that the device has not moved.
                // Copy the data from the last timestamp, or take the default
                // value if at first timestamp.
                if (ts > min_timestamp) {
                    auto last_data = result.data_at(out, ts - 1);
                    auto last_has_data = result.has_data_at(out, ts - 1);

                    std::copy(last_data.begin(), last_data.end(), row.begin());
                    std::copy(last_has_data.begin(), last_has_data.end(), has_data.begin());
                } else {
                    std::fill(row.begin(), row.end(), default_signal_strength);
                    // has_data is 0 by default
                }
            } else {
                // Finalize the current row.
                // If an access point has been seen at least once, the signal_strength
                // is averaged (by the number of seen measurements).
                // If it has not been seen, the signal strength is set to the default value
                // (i.e. the highest distance).
                for (i32 ap = 0; ap < num_access_points; ++ap) {
                    i32 seen = access_point_seen[ap];
                    assert(seen >= 0);

                    if (seen == 0) {
                        row[ap] = default_signal_strength;
                        // has_data[ap] == 0 by default.
                    } else {
                        row[ap] /= seen;
                        has_data[ap] = 1;
                    }
                    // Reset for next row.
                    access_point_seen[ap] = 0;
                }
            }
        }
        assert(entry_iter == entry_end
               && "Must have consumed all entries");
    }

private:
    const signal_data &sd;
    const i32          default_signal_strength;
    tracing_data      &result;

    vector<i32> access_point_seen;
    i32 num_devices = 0;
    i32 num_access_points = 0;
    i64 min_timestamp = 0;
    i64 max_timestamp = 0;
    i64 duration = 0;
};

// Transforms a struct of type location_data into valid tracing_data.
struct location_data_transform
{
public:
    location_data_transform(const location_data &ld,
                            tracing_data &result)
        : ld(ld)
        , result(result)
    {}

    void run()
    {
        init();
        for (i32 i = 0; i < num_devices; ++i) {
            device_step(ld.devices[i], result.devices[i]);
        }
    }

private:
    void init()
    {
        num_devices = ld.devices.size();
        if (num_devices == 0) {
            throw std::runtime_error("no devices");
        }

        min_timestamp = std::numeric_limits<i64>::max();
        max_timestamp = std::numeric_limits<i64>::min();
        for (auto &dev : ld.devices) {
            for (auto &entry : dev.data) {
                min_timestamp = std::min(min_timestamp, entry.timestamp);
                max_timestamp = std::max(max_timestamp, entry.timestamp);
            }
        }
        if (max_timestamp < min_timestamp) {
            throw std::runtime_error("requries at least one measurement");
        }
        duration = max_timestamp - min_timestamp + 1;

        // Prepare memory for every device
        result.devices.resize(num_devices);
        for (size_t i = 0; i < ld.devices.size(); ++i) {
            auto &dev = result.devices[i];

            dev.name = ld.devices[i].name;
            dev.data.resize(duration, 3, 0.0);
            // has_data is always true since no spatial dimensions
            // are missing in any measurement.
            dev.has_data.resize(duration, 3, 1);
        }

        result.data_dimension = 3; // Spatial dimension
        result.min_timestamp  = min_timestamp;
        result.max_timestamp  = max_timestamp;
        result.duration       = duration;
    }

    // Very similar to the device_step in signal_data_transform,
    // but this time we don't have to worry about missing access points,
    // since all coordinates are always given.
    void device_step(const location_data::device_data &in, tracing_data::device_data &out)
    {
        array_view<double> row;         // one row <=> one time step
        array_view<double> last_row;    // last row (if any)
        bool have_last_row = false;

        auto entry_begin = in.data.begin();
        auto entry_end   = in.data.end();
        auto entry_iter  = entry_begin;

        for (i64 ts = min_timestamp; ts <= max_timestamp; ++ts) {
            row = result.data_at(out, ts);

            i32 entries = 0;
            for (; entry_iter != entry_end; ++entry_iter) {
                auto &entry = *entry_iter;
                assert(entry.timestamp >= min_timestamp
                       && entry.timestamp <= max_timestamp);

                if (entry.timestamp != ts) {
                    assert(entry.timestamp > ts
                           && "Entries are sorted");
                    break;
                }

                row[0] += entry.lat;
                row[1] += entry.lng;
                row[2] += entry.alt;
                ++entries;
            }

            if (!entries) {
                if (have_last_row) {
                    std::copy(last_row.begin(), last_row.end(), row.begin());
                } else {
                    // Silly default for spatial coordinates ...
                    std::fill(row.begin(), row.end(), 0.0);
                }
            } else if (entries > 1) {
                row[0] /= entries;
                row[1] /= entries;
                row[2] /= entries;
            }

            last_row = row;
            have_last_row = true;
        }
        assert(entry_iter == entry_end
               && "Must have consumed all entries");
    }

private:
    const location_data &ld;
    tracing_data &result;

    i32 num_devices = 0;
    i64 min_timestamp = 0;
    i64 max_timestamp = 0;
    i64 duration = 0;

};

void moving_average(const array_2d<double> &in,
                    array_2d<double> &out, i32 n)
{
    assert(in.rows() == out.rows());
    assert(in.columns() == out.columns());
    assert(n > 0);

    // Computes the moving average value around "row"
    // in "column".
    // Adjusts the index if it would go out of bounds.
    // Could be optimized by not recomputing all values everytime.
    auto avg_around = [&](i32 row, i32 column) {
        i32 first_row = row - n + 1;
        if (first_row < 0) {
            first_row = 0;
        }

        const i32 num = row - first_row + 1;
        double acc = 0.0;
        for (i32 i = first_row; i <= row; ++i) {
            acc += in.cell(i, column);
        }
        return acc / num;
    };

    const i32 cols = numeric_cast<i32>(in.columns());
    const i32 rows = numeric_cast<i32>(in.rows());

    for (i32 i = 0; i < rows; ++i) {
        for (i32 j = 0; j < cols; ++j) {
            out.cell(i, j) = avg_around(i, j);
        }
    }
}

} // namespace

tracing_data transform(const signal_data &sd, i32 default_signal_strength)
{
    tracing_data result;
    signal_data_transform(sd, default_signal_strength, result).run();
    return result;
}

tracing_data transform(const location_data &ld)
{
    tracing_data result;
    location_data_transform(ld, result).run();
    return result;
}

void moving_average(tracing_data &td, i32 n)
{
    assert(n > 0);

    for (auto &dev : td.devices) {
        array_2d<double> result(td.duration, td.data_dimension, 0.0);
        moving_average(dev.data, result, n);
        dev.data = std::move(result);
    }
}

} // namespace mp

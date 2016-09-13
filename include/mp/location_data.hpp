#ifndef BEW_LOCATION_DATA_HPP
#define BEW_LOCATION_DATA_HPP

#include "defs.hpp"

namespace mp {

/*
 * Stores a series of location measurements for every device.
 */
struct location_data
{
    struct measurement
    {
        i64    timestamp;   // unix timestamp, seconds
        double lat;         // latitute
        double lng;         // longitute
        double alt;         // altitude
        double uncertainty;
        double speed;
        double heading;     // heading in degrees (?)
        double vspeed;      // vertical speed
    };

    struct device_data
    {
        device_data(string name)
            : name(std::move(name))
        {}

        device_data(string name, vector<measurement> data)
            : name(std::move(name))
            , data(std::move(data))
        {}

        // unique identifier
        string name;
        // sorted by timestamp, ascending
        vector<measurement> data;
    };

    vector<device_data> devices;
};

} // namespace bew

#endif // BEW_LOCATION_DATA_HPP

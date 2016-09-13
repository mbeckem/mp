#include "mp/signal_data.hpp"

#include <algorithm>
#include <unordered_map>

namespace mp {

vector<i32> bad_access_points(const signal_data &sd, double minimum_average)
{
    const i32 num_aps = numeric_cast<i32>(sd.bssids.size());

    vector<i32> count(num_aps, 0);      // number of signal strenth values
    vector<double> total(num_aps, 0.0); // sum of signal strenth values
    for (auto &dev : sd.devices) {
        for (auto &mes : dev.data) {
            i32 ap = mes.access_point_id;
            total[ap] += mes.signal_strength;
            ++count[ap];
        }
    }

    vector<i32> result;
    result.reserve(num_aps);
    for (i32 ap = 0; ap < num_aps; ++ap) {
        if (count[ap] == 0) {
            result.push_back(ap);
        } else {
            double avg = total[ap] / count[ap];
            if (avg < minimum_average) {
                result.push_back(ap);
            }
        }
    }
    return result;
}

void remove_access_points(signal_data &sd, const vector<i32> &access_point_ids)
{
    const i32 num_aps = numeric_cast<i32>(sd.bssids.size());

    assert(std::all_of(access_point_ids.begin(), access_point_ids.end(),
                       [&](i32 id) { return id >= 0 && id < num_aps; }));

    auto ap_ids_contains = [&](i32 ap) {
        auto begin = access_point_ids.begin();
        auto end = access_point_ids.end();
        return std::find(begin, end, ap) != end;
    };

    // Remove all measurements of the given access points.
    for (auto &dev : sd.devices) {
        using entry = signal_data::measurement;

        auto iter = std::remove_if(dev.data.begin(), dev.data.end(),
                                   [&](const entry &e) {
            return ap_ids_contains(e.access_point_id);
        });
        dev.data.erase(iter, dev.data.end());
    }

    // Construct a new bssids vector, omitting all access points
    // that are to be removed.
    vector<string> new_bssids;
    std::unordered_map<i32, i32> index_map; // old ap index -> new ap index

    i32 new_index = 0;
    for (i32 ap = 0; ap < num_aps; ++ap) {
        if (!ap_ids_contains(ap)) {
            new_bssids.push_back(std::move(sd.bssids[ap]));
            index_map[ap] = new_index++;
        }
    }
    sd.bssids = std::move(new_bssids);

    // Update remaining measurements and their access point ids
    // to make them point into the new, valid name vector.
    for (auto &dev : sd.devices) {
        for (auto &mes : dev.data) {
            auto iter = index_map.find(mes.access_point_id);
            assert(iter != index_map.end());
            mes.access_point_id = iter->second;
        }
    }
}

bool operator ==(const signal_data &a, const signal_data &b)
{
    return a.bssids == b.bssids && a.devices == b.devices;
}

bool operator !=(const signal_data &a, const signal_data &b)
{
    return !(a == b);
}

bool operator ==(const signal_data::measurement &a, const signal_data::measurement &b)
{
    return a.timestamp == b.timestamp
            && a.access_point_id == b.access_point_id
            && a.signal_strength == b.signal_strength;
}

bool operator !=(const signal_data::measurement &a, const signal_data::measurement &b)
{
    return !(a == b);
}

bool operator ==(const signal_data::device_data &a, const signal_data::device_data &b)
{
    return a.name == b.name && a.data == b.data;
}

bool operator !=(const signal_data::device_data &a, const signal_data::device_data &b)
{
    return !(a == b);
}

} // namespace mp

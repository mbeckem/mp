#include "mp/ground_truth.hpp"

#include <algorithm>

namespace mp {

ground_truth_relation ground_truth::relation_at(i64 timestamp,
                                                const string &device_a,
                                                const string &device_b) const
{
    auto iter = timestamps.find(timestamp);
    if (iter == timestamps.end()) {
        return ground_truth_relation::none;
    }

    const vector<device> &devices = iter->second;

    // Finds the device entry for "name" that has the hightest group index.
    // If there are multiple device entries with the same group index,
    // we chose the latest one.
    // (This resolves ambiguity for duplicate entries)
    auto find_device = [&](const string &name) {
        const device *found = nullptr;
        for (const device &dev : devices) {
            if (dev.name == name) {
                if (!found || dev.group >= found->group) {
                    found = &dev;
                }
            }
        }
        return found;
    };

    // The two devices must have been found and must be located in the same group.
    const device *found_a, *found_b;
    found_a = find_device(device_a);
    found_b = find_device(device_b);
    if (!(found_a && found_b && found_a->group == found_b->group)) {
        return ground_truth_relation::none;
    }

    // Both found and in the same group.
    i32 a_order = found_a->order;
    i32 b_order = found_b->order;
    if (a_order <= b_order) {
        return ground_truth_relation::leading;
    } else {
        return ground_truth_relation::following;
    }
}

bool ground_truth::co_moving_at(i64 timestamp,
                                const string &device_a,
                                const string &device_b) const
{
    return relation_at(timestamp, device_a, device_b) != ground_truth_relation::none;
}

} // namespace mp

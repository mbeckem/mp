#ifndef MP_GROUND_TRUTH_HPP
#define MP_GROUND_TRUTH_HPP

#include "defs.hpp"
#include "serialization.hpp"

namespace mp {

/**
 * The type of a relation in the ground truth.
 */
enum class ground_truth_relation
{
    leading,    ///< first device leading the second (includes co-leading)
    following,  ///< first device following the second
    none,       ///< not co-moving at all
};

/**
 * Holds information about the relation between devices.
 */
struct ground_truth
{
    /**
     * Returns the relation between the two devices at the given timestamp.
     */
    ground_truth_relation relation_at(i64 timestamp,
                                          const string &device_a,
                                          const string &device_b) const;

    /**
     * Returns true iff devices a and b are co-moving at the given timestamp.
     *
     */
     // This could be made more efficient e.g. by not using strings or by using
     // a more elaborate data structure, but this should not be a bottleneck at all.
    bool co_moving_at(i64 timestamp, const string &device_a, const string &device_b) const;

    struct device
    {
        string name;
        i32 group; // unique group number for this timestamp
        i32 order; // order of the device in the group, 0 -> leader
    };

    // For every timestamp, a list of devices is stored.
    // The device structures contain a name (the unique device identifier),
    // a group number and a order number in that group.
    // Two devices are considered as co-moving if their group-number
    // is equal.
    // A device is considered as following another device if its order number
    // is greater than the other one's.
    // The leader of a group has the order 0.
    //
    // This is not a hash map because i want timestamps sorted.
    std::map<i64, vector<device>> timestamps;
};

/**
 * Serialize a device using the given archive.
 *
 * \relates ground_truth::device
 */
template<typename Archive>
void serialize(Archive &ar, ground_truth::device &d)
{
    ar(cereal::make_nvp("name", d.name),
       cereal::make_nvp("group", d.group),
       cereal::make_nvp("order", d.order));
}

/**
 * Serialize the ground truth using the given archive.
 *
 * \relates ground_truth
 */
template<typename Archive>
void serialize(Archive &ar, ground_truth &gt)
{
    ar(cereal::make_nvp("timestamps", gt.timestamps));
}

} // namespace mp

#endif // MP_GROUND_TRUTH_HPP

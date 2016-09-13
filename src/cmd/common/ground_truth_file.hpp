#ifndef COMMON_GROUND_TRUTH_FILE_HPP
#define COMMON_GROUND_TRUTH_FILE_HPP

#include <stdexcept>
#include <unordered_set>

#include "mp/ground_truth.hpp"

template<typename Archive>
void save_ground_truth_file(Archive &ar, const mp::ground_truth &gt)
{
    ar(cereal::make_nvp("ground_truth", gt));
}

template<typename Archive>
void load_ground_truth_file(Archive &ar, mp::ground_truth &gt)
{
    ar(cereal::make_nvp("ground_truth", gt));
}

// The ground truth object must have the given start, end and list of devices.
inline void must_match(const mp::ground_truth &gt,
                       mp::i64 start, mp::i64 end,
                       const mp::vector<mp::string> &targets)
{
    if (gt.timestamps.empty()) {
        throw std::invalid_argument("ground truth is empty");
    }
    mp::i64 gt_start = gt.timestamps.begin()->first;
    mp::i64 gt_end = gt.timestamps.rbegin()->first;

    if (gt_start != start) {
        throw std::invalid_argument("start timestamp does not match");
    }
    if (gt_end != end) {
        throw std::invalid_argument("end timestamp does not match");
    }

    std::unordered_set<mp::string> must_devices(targets.begin(), targets.end());
    std::unordered_set<mp::string> gt_devices;
    for (auto &pair : gt.timestamps) {
        for (auto &dev : pair.second) {
            gt_devices.insert(dev.name);
        }
    }

    for (const mp::string &dev : must_devices) {
        if(!gt_devices.count(dev)) {
            throw std::invalid_argument("missing device in ground truth: " + dev);
        }
    }
}

#endif // COMMON_GROUND_TRUTH_FILE_HPP

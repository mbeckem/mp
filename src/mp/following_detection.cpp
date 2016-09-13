#include "mp/following_detection.hpp"

#include <algorithm>
#include <iostream>

#include "mp/co_moving_detection.hpp"
#include "mp/feature_computation.hpp"
#include "mp/tools/iter.hpp"

namespace mp {

time_lag_estimation::time_lag_estimation(i32 time_lag)
    : m_min_lag(-time_lag)
    , m_max_lag(time_lag)
    , m_size(time_lag * 2 + 1)
{
    assert(time_lag >= 0);
}

double time_lag_estimation::estimate_lag_simple(array_view<const double> fv)
{
    assert(fv.size() == static_cast<size_t>(m_size));

    // the iterator to the value with the smallest absolute value.
    auto min = std::min_element(fv.begin(), fv.end(),
                                [](double a, double b) {
        return std::abs(a) < std::abs(b);
    });
    auto index = std::distance(fv.begin(), min);
    return double(m_min_lag) + double(index);
}

double time_lag_estimation::estimate_lag_complex(array_view<const double> fv)
{
    static const double min = 0.001;

    assert(fv.size() == static_cast<size_t>(m_size));

    double iavg = 0.0;  // sum in paper
    double norm = 0.0;  // 's' in paper

    i32 lag = m_min_lag;
    for (double sim : fv) {
        if (std::abs(sim) < min) {
            sim = sim < 0 ? - min : min;
        }

        double inv = 1.0 / sim;
        norm += inv;
        iavg += lag * inv;

        ++lag;
    }

    return iavg / norm;
}

following_type time_lag_estimation::get_following_type(double est_lag)
{
    // this value could also be based on the maximum time lag,
    // e.g. for very big feature vectors, this could be larger.
    static constexpr double threshold = 0.1;
    if (std::abs(est_lag) <= threshold) {
        return following_type::co_leading;
    } else if (est_lag < 0.0) {
        return following_type::following;
    } else {
        return following_type::leading;
    }
}

following_data classify(co_moving_classifier &c, const similarity_data &data)
{
    const i32 time_lag = (data.feature_dimension - 1) / 2;

    following_data result;
    result.devices = data.devices;
    // Function could also take a different time span
    // to only evaluate a specific range.
    result.begin_timestamp = data.begin_timestamp;
    result.end_timestamp = data.end_timestamp;
    result.duration = data.duration;
    result.timestamps.resize(static_cast<size_t>(data.duration));
    for (size_t i = 0; i < result.timestamps.size(); ++i) {
        result.timestamps[i].timestamp = result.begin_timestamp + i64(i);
    }

    time_lag_estimation est(time_lag);
    for (i64 ts = data.begin_timestamp; ts <= data.end_timestamp; ++ts) {
        for (auto &pair : data.pairs) {
            auto feature = data.feature_at(pair, ts);
            // Pair (left, right) is co-moving at ts.
            // Classify its following type and store the result.
            if (c.co_moving(feature)) {
                double est_lag = est.estimate_lag_complex(feature);
                following_type type = est.get_following_type(est_lag);

                auto &co_moving = result.data_at(ts).co_moving;
                co_moving.push_back({pair.left, pair.right, est_lag, type});
            }
        }
    }

    return result;
}

} // namespace mp

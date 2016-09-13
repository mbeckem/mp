#ifndef COMMON_FOLLOWER_FILE
#define COMMON_FOLLOWER_FILE

#include "mp/following_detection.hpp"

#include "feature_file.hpp"

template<typename Archive>
void save_follower_file(Archive &ar,
                        const mp::following_data &f,
                        const feature_parameters &p)
{
    ar(cereal::make_nvp("params", p),
       cereal::make_nvp("followers", f));
}

template<typename Archive>
void load_follower_file(Archive &ar,
                        mp::following_data &f,
                        feature_parameters &p)
{
    ar(cereal::make_nvp("params", p),
       cereal::make_nvp("followers", f));
}

#endif // COMMON_FOLLOWER_FILE

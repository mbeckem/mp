#ifndef COMMON_LEADER_FILE_HPP
#define COMMON_LEADER_FILE_HPP

#include "mp/following_graph.hpp"

#include "feature_file.hpp"

template<typename Archive>
void save_leader_file(Archive &ar,
                      const mp::leader_data &ld,
                      const feature_parameters &params)
{
    ar(cereal::make_nvp("params", params),
       cereal::make_nvp("leader_data", ld));
}

template<typename Archive>
void load_leader_file(Archive &ar,
                      mp::leader_data &ld,
                      feature_parameters &params)
{
    ar(cereal::make_nvp("params", params),
       cereal::make_nvp("leader_data", ld));
}

#endif // COMMON_LEADER_FILE_HPP

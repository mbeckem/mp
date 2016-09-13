#ifndef COMMON_EVAL_LEADER_FILE_HPP
#define COMMON_EVAL_LEADER_FILE_HPP

#include "binary_classifier.hpp"
#include "feature_file.hpp"

template<typename Archive>
void save_eval_leader_file(Archive &ar,
                           const feature_parameters &params,
                           const binary_classifier_result &result)
{
    ar(cereal::make_nvp("params", params),
       cereal::make_nvp("result", result));
}

#endif // COMMON_EVAL_LEADER_FILE_HPP

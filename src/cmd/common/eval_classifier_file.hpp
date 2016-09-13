#ifndef COMMON_EVAL_CLASSIFIER_FILE_HPP
#define COMMON_EVAL_CLASSIFIER_FILE_HPP

#include "feature_file.hpp"
#include "binary_classifier.hpp"

template<typename Archive>
void save_eval_classifier_file(Archive &ar,
                               const feature_parameters &params,
                               const std::vector<binary_classifier_result> &results)
{
    ar(cereal::make_nvp("params", params),
       cereal::make_nvp("results", results));
}

#endif // COMMON_EVAL_CLASSIFIER_FILE_HPP

#ifndef COMMON_CLASSIFIER_FILE_HPP
#define COMMON_CLASSIFIER_FILE_HPP

#include "mp/co_moving_detection.hpp"

#include "feature_file.hpp"

// Save a classifier file (the classifier and the parameters of the original
// feature vectors).
template<typename Archive>
void save_classifier_file(Archive &ar,
                          const mp::co_moving_classifier &c,
                          const feature_parameters &p)
{
    ar(cereal::make_nvp("params", p),
       cereal::make_nvp("classifier", c));
}

// Load a classifier file.
template<typename Archive>
void load_classifier_file(Archive &ar,
                          mp::co_moving_classifier &c,
                          feature_parameters &p)
{
    ar(cereal::make_nvp("params", p),
       cereal::make_nvp("classifier", c));
}

#endif // COMMON_CLASSIFIER_FILE_HPP

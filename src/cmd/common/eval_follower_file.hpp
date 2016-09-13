#ifndef COMMON_EVAL_CLASSIFIER_FILE_HPP
#define COMMON_EVAL_CLASSIFIER_FILE_HPP

#include "feature_file.hpp"

struct follower_evaluation_result
{
    std::string name;
    mp::i64 correct = 0;
    mp::i64 total = 0;
    mp::i64 total_co_moving = 0;
    double accuracy = 0.0;
    double accuracy_co_moving = 0.0;
};

template<typename Archive>
void serialize(Archive &ar, follower_evaluation_result &r)
{
    ar(cereal::make_nvp("name", r.name),
       cereal::make_nvp("correct", r.correct),
       cereal::make_nvp("total", r.total),
       cereal::make_nvp("total_co_moving", r.total_co_moving),
       cereal::make_nvp("accuracy", r.accuracy),
       cereal::make_nvp("accuracy_co_moving", r.accuracy_co_moving));
}

template<typename Archive>
void save_eval_follower_file(Archive &ar,
                             const feature_parameters &params,
                             const follower_evaluation_result &result)
{
    ar(cereal::make_nvp("params", params),
       cereal::make_nvp("result", result));
}

#endif // COMMON_EVAL_CLASSIFIER_FILE_HPP

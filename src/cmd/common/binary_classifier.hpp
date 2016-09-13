#ifndef COMMON_BINARY_CLASSIFIER_HPP
#define COMMON_BINARY_CLASSIFIER_HPP

#include "mp/defs.hpp"

// The result of evaluating a classifier on a single data set.
// See http://en.wikipedia.org/wiki/Confusion_matrix#Table_of_confusion
struct binary_classifier_result
{
    binary_classifier_result() {}
    inline binary_classifier_result(const std::string &name,
                                    mp::i64 true_positive,
                                    mp::i64 false_positive,
                                    mp::i64 false_negative,
                                    mp::i64 true_negative);

    // Dataset name
    std::string name;

    // Absolute values
    mp::i64 true_positive = 0;
    mp::i64 false_positive = 0;
    mp::i64 false_negative = 0;
    mp::i64 true_negative = 0;

    // Derived values (rates).
    double recall = 0;
    double specificity = 0;
    double precision = 0;
    double accuracy = 0;
    double f = 0;
};

binary_classifier_result::binary_classifier_result(const std::string &name,
                                                   mp::i64 p_true_positive, mp::i64 p_false_positive,
                                                   mp::i64 p_false_negative, mp::i64 p_true_negative)
    : name(name)
    , true_positive(p_true_positive)
    , false_positive(p_false_positive)
    , false_negative(p_false_negative)
    , true_negative(p_true_negative)
{
    mp::i64 p = true_positive + false_negative;
    mp::i64 n = false_positive + true_negative;

    // Values like "nan" or "+/- inf" are illegal json.
    // Chosing 0.0 is good enough since very small "b" values
    // most likely are an error (e.g. no samples or 0 precision).
    auto div_0 = [](double a, double b) {
        if (std::abs(b) > 0.0000001) {
            return a / b;
        }
        std::cout << "b = " << b << std::endl;
        return 0.0;
    };

    recall = div_0(true_positive, p);
    specificity = div_0(true_negative, n);
    precision = div_0(true_positive, true_positive + false_positive);
    accuracy = div_0(true_positive + true_negative, p + n);
    f = 2.0 * div_0(precision * recall, precision + recall);
    assert(f != 0);
}

template<typename Archive>
void serialize(Archive &ar, binary_classifier_result &r)
{
    ar(cereal::make_nvp("name", r.name),
       cereal::make_nvp("true_positive", r.true_positive),
       cereal::make_nvp("false_positive", r.false_positive),
       cereal::make_nvp("false_negative", r.false_negative),
       cereal::make_nvp("true_negative", r.true_negative),
       cereal::make_nvp("recall", r.recall),
       cereal::make_nvp("specificity", r.specificity),
       cereal::make_nvp("precision", r.precision),
       cereal::make_nvp("accuracy", r.accuracy),
       cereal::make_nvp("f", r.f));
}

#endif // COMMON_BINARY_CLASSIFIER_HPP

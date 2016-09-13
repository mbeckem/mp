#include "mp/co_moving_detection.hpp"

#include <iostream>
#include <dlib/svm.h>

#include "mp/feature_computation.hpp"
#include "mp/ground_truth.hpp"

namespace mp {

// Increment if any details in the layout of
// co_moving_classifier::impl change.
static const i32 VERSION = 3;

using sample_type        = dlib::matrix<double, 0, 1>; // 0 -> row size determined at runtime
using vector_normalizer  = dlib::vector_normalizer<sample_type>;
using kernel_type        = dlib::linear_kernel<sample_type>;
using trainer_type       = dlib::svm_c_linear_trainer<kernel_type>; // TODO: Decide on a final trainer type.
using decision_func_type = dlib::decision_function<kernel_type>;
using function_type      = dlib::normalized_function<decision_func_type>;

// Extracts a vector of samples (== feature vectors) and labels (== co-moving/not co-moving)
// from the input data.
static void get_learning_data(const similarity_data &data, const ground_truth &gt,
                              vector<sample_type> &samples, vector<double> &labels)
{
    samples.clear();
    labels.clear();

    samples.reserve(data.pairs.size() * data.duration);
    labels.reserve(data.pairs.size() * data.duration);

    sample_type sample;
    sample.set_size(data.feature_dimension, 1);
    // For each pair ...
    for (auto &pair : data.pairs) {
        const string &left_name = data.devices[pair.left];
        const string &right_name = data.devices[pair.right];

        // ... and each timestamp ...
        for (i64 ts = data.begin_timestamp; ts <= data.end_timestamp; ++ts) {
            // ... get the feature vector and its corresponding ground truth
            auto feature = data.feature_at(pair, ts);
            for (i32 i = 0; i < data.feature_dimension; ++i) {
                sample(i) = feature[i];
            }
            samples.push_back(sample);

            bool co_moving = gt.co_moving_at(ts, left_name, right_name);
            labels.push_back(co_moving ? 1.0 : -1.0);
        }
    }
}

// Trains the given normalizer using the provided samples
// and then normalizes the samples.
static void normalize(vector_normalizer &norm, vector<sample_type> &samples)
{
    norm.train(samples);
    for (auto &sample : samples) {
        sample = norm(sample);
    }
}

struct co_moving_classifier::impl
{
    function_type func;
    size_t        training_dimension;
    sample_type   sample;
};

void co_moving_classifier::print_cross_validation(const similarity_data &data, const ground_truth &gt, std::ostream &o)
{
    vector<sample_type> samples;
    vector<double> labels;
    get_learning_data(data, gt, samples, labels);
    dlib::randomize_samples(samples, labels);

    vector_normalizer norm;
    normalize(norm, samples);

    trainer_type trainer;
    o << "Cross validation results: " << std::endl;
    for (double C = 1; C < 100000; C *= 5) {
        trainer.set_c(C);

        // Two values:
        // - fraction of correctly identified +1 cases
        // - ________________________________ -1 cases
        o << "C = " << C
          << ", accuracy = " << dlib::cross_validate_trainer(trainer, samples, labels, 3);
        o << std::flush;
    }
}

bool co_moving_classifier::co_moving(array_view<const double> a)
{
    if (!m_impl) {
        return false;
    }

    // May only be used with an input dimension
    // that equals the training dimension.
    assert(a.size() == m_impl->training_dimension);

    // Sample is an instance of sample_type that is
    // cached by this instance of co_moving_classifier.
    // This saves us an allocation per classification.
    sample_type &sample = m_impl->sample;
    for (size_t i = 0; i < m_impl->training_dimension; ++i) {
        sample(i) = a[i];
    }
    return m_impl->func(sample) >= 0;
}

void co_moving_classifier::learn(const similarity_data &data, const ground_truth &gt)
{
    trainer_type trainer;
    trainer.set_c(1);

    vector<sample_type> samples;
    vector<double> labels;
    get_learning_data(data, gt, samples, labels);
    dlib::randomize_samples(samples, labels);

    vector_normalizer norm;
    normalize(norm, samples);

    std::unique_ptr<impl> ptr(new impl());
    ptr->training_dimension = data.feature_dimension;
    ptr->func.normalizer = norm; // other samples will be normalized, too.
    ptr->func.function = trainer.train(samples, labels);
    ptr->sample.set_size(ptr->training_dimension, 1);
    m_impl = std::move(ptr);
}

void co_moving_classifier::load(std::istream &i)
{
    i32 version;
    dlib::deserialize(version, i);
    if (version != VERSION) {
        throw std::runtime_error("Serialized classifier has incorrect version " + std::to_string(version)
                                 + ", version must be " + std::to_string(VERSION));
    }

    bool valid;
    dlib::deserialize(valid, i);
    if (valid) {
        std::unique_ptr<impl> ptr(new impl());
        dlib::deserialize(ptr->func, i);
        dlib::deserialize(ptr->training_dimension, i);
        ptr->sample.set_size(ptr->training_dimension, 1);
        m_impl = std::move(ptr);
    } else {
        m_impl.reset();
    }
}

void co_moving_classifier::save(std::ostream &o) const
{
    dlib::serialize(VERSION, o);
    bool valid = static_cast<bool>(m_impl);
    dlib::serialize(valid, o);

    if (valid) {
        dlib::serialize(m_impl->func, o);
        dlib::serialize(m_impl->training_dimension, o);
        // sample does not need to be stored,
        // because its value does not matter and its
        // size equals training_dimension.
    }
}

co_moving_classifier::co_moving_classifier()
    : m_impl()
{}

co_moving_classifier::~co_moving_classifier()
{}

co_moving_classifier::co_moving_classifier(co_moving_classifier &&other)
    : m_impl(std::move(other.m_impl))
{}

co_moving_classifier& co_moving_classifier::operator =(co_moving_classifier &&other)
{
    m_impl = std::move(other.m_impl);
    return *this;
}

} // namespace mp

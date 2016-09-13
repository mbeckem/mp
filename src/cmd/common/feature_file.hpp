#ifndef COMMON_FEATURE_FILE_HPP
#define COMMON_FEATURE_FILE_HPP

#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>

#include "mp/feature_computation.hpp"
#include "mp/ground_truth.hpp"

#include "util.hpp"

// Feature parameters are stored together
// with the classifier or feature data and
// contain information about the original data
// used to compute the feature data / classifier.
struct feature_parameters
{
    mp::string data_source;
    mp::string algorithm;
    mp::i32 window_size = 0;
    mp::i32 time_lag = 0;
};

// Terminates with an error message if the two objects are not equal.
// This is done to ensure that only feature values with equal parameters
// are compared.
// For example, a classifier trained with signal data can make little to no
// predictions about location data feature vectors.
inline void must_equal(const feature_parameters &master,
                       const feature_parameters &f, const std::string &f_path)
{
    if (master.data_source != f.data_source) {
        std::cerr << "input file\"" << f_path
                  << "\" uses a different data source (" << f.data_source << ")"
                  << std::endl;
        exit(1);
    }
    if (master.algorithm != f.algorithm) {
        std::cerr << "input file \"" << f_path
                  << "\" uses a different algorithm (" << f.algorithm << ")"
                  << std::endl;
        exit(1);
    }
    if (master.window_size != f.window_size) {
        std::cerr << "input file \"" << f_path
                  << "\" uses a different window size (" << f.window_size << ")"
                  << std::endl;
        exit(1);
    }
    if (master.time_lag != f.time_lag) {
        std::cerr << "input file \"" << f_path
                  << "\" uses a different time lag (" << f.time_lag << ")"
                  << std::endl;
        exit(1);
    }
}

template<typename Archive>
void serialize(Archive &ar, feature_parameters &p)
{
    ar(cereal::make_nvp("data_source", p.data_source),
       cereal::make_nvp("algorithm", p.algorithm),
       cereal::make_nvp("window_size", p.window_size),
       cereal::make_nvp("time_lag", p.time_lag));

    assert(p.window_size > 0);
    assert(p.time_lag >= 0);
}

// Save a feature file (feature vectors, ground truth and parameters).
template<typename Archive>
void save_feature_file(Archive &ar,
                       const mp::similarity_data &sim,
                       const feature_parameters &p)
{
    ar(cereal::make_nvp("params", p),
       cereal::make_nvp("feature_data", sim));
}

// Load a feature file.
template<typename Archive>
void load_feature_file(Archive &ar,
                       mp::similarity_data &sim,
                       feature_parameters &p)
{
    ar(cereal::make_nvp("params", p),
       cereal::make_nvp("feature_data", sim));

    assert(sim.begin_timestamp <= sim.end_timestamp);
    assert(sim.duration == sim.end_timestamp - sim.begin_timestamp + 1);
    assert(sim.feature_dimension == p.time_lag * 2 + 1);
}

inline void read_feature_file(const std::string &path,
                              const std::string &type,
                              mp::similarity_data &sim,
                              feature_parameters &params)
{
    std::fstream in_stream;
    std::ios_base::openmode mode = std::ios_base::in;
    if (type == "binary") {
        mode |= std::ios_base::binary;
    }

    try_open(in_stream, path, mode);

    if (type == "json") {
        cereal::JSONInputArchive ar(in_stream);
        load_feature_file(ar, sim, params);
    } else if (type == "binary") {
        cereal::PortableBinaryInputArchive ar(in_stream);
        load_feature_file(ar, sim, params);
    } else {
        throw std::logic_error("unsupported input type: " + type);
    }
}

inline void write_feature_file(const std::string &path, const std::string &type,
                               const mp::similarity_data &sim,
                               const feature_parameters &params)
{
    std::fstream out_stream;
    std::ios_base::openmode mode = std::ios_base::out;
    if (type == "binary") {
        mode |= std::ios_base::binary;
    }

    try_open(out_stream, path, mode);

    if (type == "json" || type == "compact-json") {
        cereal::JSONOutputArchive ar(out_stream, type == "json"
                                     ? cereal::JSONOutputArchive::Options::Default()
                                     : cereal::JSONOutputArchive::Options::NoIndent());
        save_feature_file(ar, sim, params);
    } else if (type == "binary") {
        cereal::PortableBinaryOutputArchive ar(out_stream);
        save_feature_file(ar, sim, params);
    } else {
        throw std::logic_error("unsupported output format: " + type);
    }
}

#endif // COMMON_FEATURE_FILE_HPP

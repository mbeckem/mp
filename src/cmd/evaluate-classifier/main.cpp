#include <cmath>
#include <memory>
#include <iostream>

#include <boost/program_options.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>

#include <picojson/picojson.h>

#include "mp/co_moving_detection.hpp"

#include "../common/util.hpp"
#include "../common/classifier_file.hpp"
#include "../common/eval_classifier_file.hpp"
#include "../common/feature_file.hpp"
#include "../common/ground_truth_file.hpp"

using namespace std;
using namespace mp;

void parse_options(int argc, char *argv[]);
void validate_options();
void must_open_read(fstream &f, const string &path);
void read_feature_file(fstream &f,
                       similarity_data &sim,
                       ground_truth &gt,
                       feature_parameters &params);

vector<binary_classifier_result> evaluate_classifier(co_moving_classifier &c,
                                                     const feature_parameters &training_params);

// The classifier is stored in this file.
string classifier_file;

// The input data type (either "json" or "binary")
string in_type;

// The list of feature files.
vector<string> in_files;

// Output file.
string out_file;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    co_moving_classifier classifier;
    feature_parameters training_params;

    // Load the classifier from file
    {
        fstream classifier_input;
        try {
            try_open(classifier_input, classifier_file, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open classifier file \""
                 << classifier_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONInputArchive ar(classifier_input);
        load_classifier_file(ar, classifier, training_params);

        cout << "Using classifier:\n"
             << "  Source:      " << classifier_file << "\n"
             << "  Data source: " << training_params.data_source << "\n"
             << "  Algorithm:   " << training_params.algorithm << "\n"
             << "  Window size: " << training_params.window_size << " seconds\n"
             << "  Time lag:    " << training_params.time_lag << " seconds\n"
             << flush;
    }

    cout << "Evaluating classifier:\n"
         << "  Output file: " << out_file  << "\n"
         << endl;

    vector<binary_classifier_result> results;
    double seconds = execution_seconds([&]{
        results = evaluate_classifier(classifier, training_params);
    });
    cout << "Evaluation took " << seconds << " seconds" << endl;

    // Write to disk
    {
        fstream out_stream;
        try {
            try_open(out_stream, out_file, ios_base::out);
        } catch (const std::exception &e) {
            cerr << "failed to open output file \""
                 << out_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONOutputArchive ar(out_stream);
        save_eval_classifier_file(ar, training_params, results);
    }

    return 0;
}

void parse_options(int argc, char *argv[])
{
    namespace po = boost::program_options;

    po::options_description opts("Options");
    opts.add_options()
            ("help,h",
             "Print this help message")
            ("classifier",
             po::value<string>(&classifier_file)->value_name("PATH")->required(),
             "The classifier to evaluate.\n"
             "The classifier file must have been produced by the train-classifier program.")
            ("input-type",
             po::value<string>(&in_type)->value_name("TYPE")->required(),
             "The input type for feature files. Input files must be valid feature files.\n"
             "Supported input types are:\n"
             "  json:   \tRead the feature files as json.\n"
             "  binary: \tRead the feature files as binary files.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "Evaluation results will be written to this file.")
            ;

    // Hidden options are not shown in --help
    po::options_description hidden("Hidden options");
    hidden.add(opts);
    hidden.add_options()
            ("input-file",
             po::value<vector<string>>(&in_files),
             "All feature files")
            ;
    po::positional_options_description p;
    p.add("input-file", -1);

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(hidden);
        parser.positional(p);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options] (feature-file, ground-truth-file)..." << "\n";
            cerr << "\n";
            cerr << "This program will load a previously trained classifier from a file\n"
                 << "and test it against user-provided feature files and their associated ground truth.\n"
                 << "Feature files will be read and classified.\n"
                 << "The results of the classification will be compared against the ground truth.\n"
                 << "Input files must be specified in pairs, i.e. always a feature file followed by its ground truth.\n"
                 << "\n";
            cerr << opts << flush;
            exit(1);
        }

        po::notify(vm);
    } catch (const po::error &e) {
        cerr << e.what() << endl;
        exit(1);
    }
}

void validate_options()
{
    auto contains = [](const vector<string> &v, const string &s) {
        return std::find(v.begin(), v.end(), s) != v.end();
    };

    static const vector<string> allowed_input_types{"json", "binary"};

    bool ok = true;
    if (!contains(allowed_input_types, in_type)) {
        cerr << "input type is invalid (" << in_type << ")" << endl;
        ok = false;
    }

    if (!ok) {
        exit(1);
    }
}

vector<binary_classifier_result> evaluate_classifier(co_moving_classifier &c,
                                                     const feature_parameters &training_params)
{
    vector<binary_classifier_result> result;

    if (in_files.size() % 2 != 0) {
        cerr << "Expected even number of input files (feature data and ground truth pairs)" << endl;
        exit(1);
    }

    // Compare against all feature files and their ground truths.
    for (size_t i = 0; i < in_files.size();) {
        const string &feature_path = in_files[i++];
        const string &ground_truth_path = in_files[i++];

        similarity_data sim;
        ground_truth gt;
        feature_parameters params;

        // Read feature file and validate the parameters.
        try {
            read_feature_file(feature_path, in_type, sim, params);
        } catch (const std::exception &e) {
            cerr << "failed to read feature file \""
                 << feature_path << "\": "
                 << e.what() << endl;
            exit(1);
        }
        must_equal(training_params, params, feature_path);

        // Load the ground truth file.
        {
            fstream gt_stream;
            try {
                try_open(gt_stream, ground_truth_path, ios_base::in);
            } catch (const exception &e) {
                cerr << "failed to open ground truth file \""
                     << ground_truth_path << "\": "
                     << e.what() << endl;
                exit(1);
            }

            cereal::JSONInputArchive ar(gt_stream);
            load_ground_truth_file(ar, gt);
        }

        try {
            must_match(gt, sim.begin_timestamp, sim.end_timestamp, sim.devices);
        } catch (const std::exception &e) {
            cerr << "Ground truth mistmatch for \""
                 << ground_truth_path << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cout << "Evaluating the classifier against feature data:\n"
             << "  Source:       " << feature_path << "\n"
             << "  Ground truth: " << ground_truth_path << "\n"
             << "  Devices:      " << sim.devices.size() << "\n"
             << "  Duration:     " << sim.duration << " seconds\n"
             << flush;

        i64 true_positive = 0;
        i64 false_positive = 0;
        i64 false_negative = 0;
        i64 true_negative = 0;

        for (auto &pair : sim.pairs) {
            auto &left_name = sim.devices[pair.left];
            auto &right_name = sim.devices[pair.right];

            for (i64 ts = sim.begin_timestamp; ts <= sim.end_timestamp; ++ts) {
                bool co_moving = gt.co_moving_at(ts, left_name, right_name);
                bool predicted = c.co_moving(sim.feature_at(pair, ts));

                if (co_moving) {
                    if (predicted) {
                        ++true_positive;
                    } else {
                        ++false_negative;
                    }
                } else {
                    if (!predicted) {
                        ++true_negative;
                    } else {
                        ++false_positive;
                    }
                }
            }
        }

        result.push_back(binary_classifier_result(feature_path,
                                                  true_positive, false_positive,
                                                  false_negative, true_negative));

        cout << "\n" << flush;
    }
    return result;
}

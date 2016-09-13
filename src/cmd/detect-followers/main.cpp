#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include "mp/co_moving_detection.hpp"
#include "mp/feature_computation.hpp"
#include "mp/following_detection.hpp"

#include "../common/classifier_file.hpp"
#include "../common/feature_file.hpp"
#include "../common/follower_file.hpp"
#include "../common/util.hpp"

using namespace mp;
using namespace std;

void parse_options(int argc, char *argv[]);
void validate_options();

// Path to input feature file and its type.
string in_file;
string in_type;

// Path to classifier file.
string classifier_file;

// Path to output file.
string out_file;

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    // Load classifier
    co_moving_classifier classifier;
    feature_parameters classifier_params;
    {
        fstream in_stream;
        try {
            try_open(in_stream, classifier_file, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open classifier file \""
                 << classifier_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONInputArchive ar(in_stream);
        load_classifier_file(ar, classifier, classifier_params);
    }

    cout << "Using classifier:\n"
         << "  Source:      " << classifier_file << "\n"
         << "  Data source: " << classifier_params.data_source << "\n"
         << "  Algorithm:   " << classifier_params.algorithm << "\n"
         << "  Window size: " << classifier_params.window_size << " seconds\n"
         << "  Time lag:    " << classifier_params.time_lag << " seconds\n"
         << flush;

    // Load input feature data
    similarity_data sim;
    feature_parameters params;
    try {
        read_feature_file(in_file, in_type, sim, params);
    } catch (const std::exception &e) {
        cerr << "failed to read feature file \""
             << in_file << "\": "
             << e.what() << endl;
        exit(1);
    }

    must_equal(classifier_params, params, in_file);
    cout << "Using feature data:\n"
         << "  Source:   " << in_file << "\n"
         << "  Devices:  " << sim.devices.size() << "\n"
         << "  Duration: " << sim.duration << " seconds\n"
         << flush;

    cout << "Computing results..." << endl;
    following_data result;
    double seconds = execution_seconds([&]{
        result = classify(classifier, sim);
    });
    cout << "Computation took " << seconds << " seconds." << endl;

    // Serialize results to file
    {
        fstream out_stream;
        try {
            try_open(out_stream, out_file, ios_base::out | ios_base::trunc);
        } catch (const std::exception &e) {
            cerr << "failed to open output file \""
                 << out_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONOutputArchive ar(out_stream);
        save_follower_file(ar, result, params);
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
             "The classifier to use for detecting co-movement patterns.\n"
             "The classifier file must have been produced by the train-classifier program.")
            ("input",
             po::value<string>(&in_file)->value_name("PATH")->required(),
             "The feature file that will be classified by this program.\n"
             "The feature file must have been produces by the produce-features command.")
            ("input-type",
             po::value<string>(&in_type)->value_name("TYPE")->required(),
             "The input type for feature files. Input files must be valid feature files.\n"
             "Supported input types are:\n"
             "  json:   \tRead the feature files as json.\n"
             "  binary: \tRead the feature files as binary files.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "The outfile file.\n"
             "Detected leadership and following relations will be written to this file.")
            ;

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(opts);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options]" << "\n";
            cerr << "\n";
            cerr << "This program detects following and leadership patterns in\n"
                 << "the input feature data by using the given classifier.\n"
                 << "The outfile file will contain an entry for every detected\n"
                 << "following/leading/co-moving relation between to devices.\n"
                 << "\n"
                 << "Feature files can be created by using the produce-features command.\n"
                 << "Classifiers are trained by executing the train-classifier program.\n"
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

    static const vector<string> allowed_input_types{"binary", "json"};

    bool ok = true;
    if (!contains(allowed_input_types, in_type)) {
        cerr << "input type is invalid (" << in_type << ")" << endl;
        ok = false;
    }

    if (!ok) {
        exit(1);
    }
}

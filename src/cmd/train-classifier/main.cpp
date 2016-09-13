#include <iostream>

#include <boost/program_options.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>

#include "mp/co_moving_detection.hpp"
#include "mp/tracing_data.hpp"

#include "../common/classifier_file.hpp"
#include "../common/feature_file.hpp"
#include "../common/ground_truth_file.hpp"
#include "../common/util.hpp"

using namespace mp;
using namespace std;

void parse_options(int argc, char *argv[]);
void validate_options();

// Input file path and its file type ("json" or "binary").
string in_file;
string in_type;

string ground_truth_file;

// Output file path.
string out_file;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    similarity_data sim;
    feature_parameters params;
    ground_truth gt;

    // Load feature file
    try {
        read_feature_file(in_file, in_type, sim, params);
    } catch (const std::exception &e) {
        cerr << "failed to read input file \""
             << in_file << "\": "
             << e.what() << endl;
        exit(1);
    }

    // Load ground truth
    {
        fstream gt_stream;
        try {
            try_open(gt_stream, ground_truth_file, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to read ground truth file \""
                 << in_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONInputArchive ar(gt_stream);
        load_ground_truth_file(ar, gt);
    }

    try {
        must_match(gt, sim.begin_timestamp, sim.end_timestamp, sim.devices);
    } catch (const std::exception &e) {
        cerr << "Ground truth mismatch: " << e.what() << endl;
        exit(1);
    }

    // Train classifier
    cout << "Training feature data:\n"
         << "  Source:      " << in_file << "\n"
         << "  Data source: " << params.data_source << "\n"
         << "  Algorithm:   " << params.algorithm << "\n"
         << "  Window size: " << params.window_size << " seconds\n"
         << "  Time lag:    " << params.time_lag << " seconds\n"
         << "  Devices:     " << sim.devices.size() << "\n"
         << "  Duration:    " << sim.duration << " seconds\n"
         << endl;

    cout << "Training co-moving classifier ..." << endl;

    co_moving_classifier classifier;
    double seconds = execution_seconds([&]{
        classifier.learn(sim, gt);
    });

    cout << "Training took " << seconds << " seconds" << endl;

    // Serialize classifier
    {
        fstream out_stream;
        ios_base::openmode mode = ios_base::out | ios_base::trunc;

        try {
            try_open(out_stream, out_file, mode);
        } catch (const std::exception &e) {
            cerr << "failed to open output file \""
                 << out_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONOutputArchive ar(out_stream);
        save_classifier_file(ar, classifier, params);
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
            ("input",
             po::value<string>(&in_file)->value_name("PATH")->required(),
             "Input file (feature data)")
            ("input-type",
             po::value<string>(&in_type)->value_name("TYPE")->required(),
             "The input type. Input files must be valid feature files.\n"
             "Supported input types are:\n"
             "  json:   \tRead the feature files as json.\n"
             "  binary: \tRead the feature files as binary files.")
            ("ground-truth",
             po::value<string>(&ground_truth_file)->value_name("PATH")->required(),
             "The path to the ground truth file. The ground truth is needed to classify the learning data.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "The output file. The classifier will be serialized into this file.")
            ;

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(opts);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options]" << "\n";
            cerr << "\n";
            cerr << "This program will train a co-moving classifier using the feature\n"
                 << "data found in the input file.\n"
                 << "The classifier will be serialized to disk and can be used by other programs.\n"
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
        cerr << "invalid input type (" << in_type << ")" << endl;
        ok = false;
    }

    if (!ok) {
        exit(1);
    }
}
